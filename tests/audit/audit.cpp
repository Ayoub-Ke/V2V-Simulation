#include "GraphColoring.h"
#include "MapView.h"
#include "OsmParser.h"
#include "VehicleSimulation.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <limits>

namespace {
int passed = 0, failed = 0;
void check(bool condition, const std::string& name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << std::endl;
    condition ? ++passed : ++failed;
}

std::set<std::pair<size_t, size_t>> pairs(const InterferenceGraph& graph)
{
    std::set<std::pair<size_t, size_t>> result;
    for (const auto& e : graph.edges())
        result.emplace(e.vehicleA, e.vehicleB);
    return result;
}

OsmData smallData()
{
    OsmData data;
    data.nodes = {{1, {47.75, 7.33}}, {2, {47.75, 7.3302}},
                  {3, {47.7502, 7.3302}}};
    data.ways.push_back({1, "residential", false, {1, 2, 3}});
    return data;
}

void testProjectionAndRoads()
{
    auto data = smallData();
    double reference = 0;
    for (int z : {10, 14, 18}) {
        WebMercatorProjection projection(z, 47.75, 7.33);
        double x, y, lat, lon;
        projection.project(47.7501, 7.3301, x, y);
        projection.unproject(x, y, lat, lon);
        check(std::abs(lat - 47.7501) < 1e-9 && std::abs(lon - 7.3301) < 1e-9,
              "projection round trip zoom=" + std::to_string(z));
        RoadGraph graph;
        graph.build(data, projection);
        check(graph.nodeCount() == 3 && graph.edgeCount() == 4,
              "bidirectional road zoom=" + std::to_string(z));
        if (z == 10) reference = graph.edges()[0].length;
        check(graph.edges()[0].length > 14 && graph.edges()[0].length < 16 &&
              std::abs(graph.edges()[0].length - reference) < 1e-9,
              "GPS road length independent of zoom=" + std::to_string(z));
    }
    data.ways[0].oneway = true;
    RoadGraph graph;
    graph.build(data, WebMercatorProjection(16, 47.75, 7.33));
    check(graph.edgeCount() == 2 && graph.edgesFrom(3).empty(), "oneway road direction");

    // A T junction can be represented by two OSM ways, one crossing the junction.
    data.nodes[4] = {47.7498, 7.3302};
    data.ways = {{1, "residential", false, {1, 2, 3}},
                 {2, "residential", false, {4, 2}}};
    graph.build(data, WebMercatorProjection(16, 47.75, 7.33));
    check(graph.edgesFrom(2).size() == 3, "T junction has three outgoing directions");
    check(graph.degree(2) == 3, "T junction recognized by its three distinct neighbors");

    // A way split without a branch is not an intersection, but current UI marks it.
    data.ways = {{1, "residential", false, {1, 2}},
                 {2, "residential", false, {2, 3}}};
    graph.build(data, WebMercatorProjection(16, 47.75, 7.33));
    check(graph.degree(2) < 3, "OSM way split without branch is not marked as intersection");
}

void testParser()
{
    QTemporaryDir dir;
    const auto path = dir.filePath("oneway.osm");
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write("<osm><node id='1' lat='47.75' lon='7.33'/>"
               "<node id='2' lat='47.75' lon='7.3302'/>"
               "<way id='1'><nd ref='1'/><nd ref='2'/>"
               "<tag k='highway' v='residential'/><tag k='oneway' v='-1'/></way></osm>");
    file.close();
    OsmData data;
    std::string error;
    check(OsmParser::load(path.toStdString(), data, error), "read valid OSM fixture");
    RoadGraph graph;
    graph.build(data, WebMercatorProjection(16, 47.75, 7.33));
    check(graph.edgeCount() == 1 && graph.edgesFrom(1).empty() &&
              graph.edgesFrom(2).size() == 1,
          "OSM oneway=-1 allows reverse direction only");
    check(!OsmParser::load(dir.filePath("absent.osm").toStdString(), data, error),
          "missing OSM file rejected");
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write("<osm><way id='1'><nd ref='999'/><nd ref='1000'/>"
               "<tag k='highway' v='residential'/></way></osm>");
    file.close();
    check(!OsmParser::load(path.toStdString(), data, error),
          "OSM without any usable road segment rejected");
    for (const char* latitude : {"NaN", "infinity", "oops", "91", ""}) {
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(QByteArray("<osm><node id='1' lat='") + latitude +
            "' lon='7.33'/><node id='2' lat='47.75' lon='7.34'/>"
            "<way id='1'><nd ref='1'/><nd ref='2'/><tag k='highway' v='residential'/></way></osm>");
        file.close();
        check(!OsmParser::load(path.toStdString(), data, error),
              std::string("invalid latitude rejected: ") + latitude);
    }
    for (const char* tags : {
        "<tag k='junction' v='roundabout'/><tag k='oneway' v='no'/>",
        "<tag k='oneway' v='no'/><tag k='junction' v='roundabout'/>"}) {
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(QByteArray("<osm><node id='1' lat='47.75' lon='7.33'/>"
            "<node id='2' lat='47.75' lon='7.34'/><way id='1'><nd ref='1'/><nd ref='2'/>"
            "<tag k='highway' v='residential'/>") + tags + "</way></osm>");
        file.close();
        const bool loaded = OsmParser::load(path.toStdString(), data, error);
        check(loaded && !data.ways[0].oneway && error.empty(),
              "explicit oneway=no overrides roundabout regardless of tag order");
    }
}

void testInterference()
{
    const std::unordered_map<int64_t, GraphNode> nodes;
    for (int z : {10, 16, 18}) {
        WebMercatorProjection projection(z, 47.75, 7.33);
        std::vector<Vehicle> vehicles(101);
        vehicles[0].x = 299;
        vehicles[1].x = 600;
        for (size_t i = 2; i < vehicles.size(); ++i) {
            vehicles[i].x = 2000 * i;
            vehicles[i].y = 2000;
        }
        InterferenceGraph naive, hashed;
        naive.setSpatialHashingEnabled(false);
        naive.build(vehicles, nodes, projection, 150);
        hashed.build(vehicles, nodes, projection, 150);
        check(pairs(naive) == pairs(hashed),
              "spatial partition equals exhaustive reference zoom=" + std::to_string(z) +
              " (reference=" + std::to_string(naive.edgeCount()) +
              ", partition=" + std::to_string(hashed.edgeCount()) + ")");
    }
    WebMercatorProjection projection(16, 47.75, 7.33);
    std::vector<Vehicle> vehicles(3);
    projection.project(47.75, 7.33, vehicles[0].x, vehicles[0].y);
    projection.project(47.75, 7.3301, vehicles[1].x, vehicles[1].y);
    projection.project(47.75, 7.34, vehicles[2].x, vehicles[2].y);
    InterferenceGraph graph;
    graph.build(vehicles, nodes, projection, 20);
    check(graph.edgeCount() == 1 && graph.edges()[0].vehicleA == 0 &&
              graph.edges()[0].vehicleB == 1 && graph.edges()[0].distance > 7 &&
              graph.edges()[0].distance < 8,
          "interference uses real meter distances");
    graph.build(vehicles, nodes, projection, 0);
    check(graph.edgeCount() == 0 && graph.vertexCount() == 3, "zero range retains isolated vertices");
    graph.build({}, nodes, projection, 150);
    check(graph.edgeCount() == 0 && graph.vertexCount() == 0, "empty interference graph");
}

void testSpatialReference()
{
    constexpr double pi = 3.14159265358979323846;
    std::mt19937 rng(7349);
    bool matches = true;
    for (double latitude : {-80., 0., 47.75, 80.}) {
        for (int zoom : {0, 10, 18}) {
            WebMercatorProjection projection(zoom, latitude, 7.33);
            std::vector<Vehicle> vehicles(128);
            std::vector<std::pair<double, double>> gps;
            for (auto& v : vehicles) {
                const double lat = latitude + (int(rng() % 2000) - 1000) / 100000.;
                const double lon = 7.33 + (int(rng() % 2000) - 1000) / 100000.;
                projection.project(lat, lon, v.x, v.y);
                gps.emplace_back(lat, lon);
                v.transmissionRadius = 20 + rng() % 300;
            }
            std::set<std::pair<size_t, size_t>> expected;
            for (size_t i = 0; i < vehicles.size(); ++i)
                for (size_t j = i + 1; j < vehicles.size(); ++j) {
                    const double dlat = (gps[i].first - gps[j].first) * pi / 180;
                    const double dlon = (gps[i].second - gps[j].second) * pi / 180;
                    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                        std::cos(gps[i].first * pi / 180) * std::cos(gps[j].first * pi / 180) *
                        std::sin(dlon / 2) * std::sin(dlon / 2);
                    const double distance = 2 * 6371000 * std::asin(std::sqrt(std::clamp(a, 0., 1.)));
                    if (distance < std::max(vehicles[i].transmissionRadius, vehicles[j].transmissionRadius))
                        expected.emplace(i, j);
                }
            InterferenceGraph graph;
            graph.build(vehicles, {}, projection, 400);
            matches &= pairs(graph) == expected;
        }
    }
    check(matches, "3D grid equals independent haversine oracle: 12 latitude/zoom scenarios, variable ranges");
    WebMercatorProjection projection(18, 0, 179.99);
    std::vector<Vehicle> vehicles(101);
    for (size_t i = 0; i < vehicles.size(); ++i)
        projection.project(0, 170 + i * .01, vehicles[i].x, vehicles[i].y);
    projection.project(0, 179.9995, vehicles[0].x, vehicles[0].y);
    projection.project(0, -179.9995, vehicles[1].x, vehicles[1].y);
    InterferenceGraph graph;
    graph.build(vehicles, {}, projection, 150);
    check(pairs(graph).count({0, 1}) == 1, "date-line neighbors are connected across longitude wrap");
    vehicles[0].x = std::numeric_limits<double>::quiet_NaN();
    graph.build(vehicles, {}, projection, 150);
    check(pairs(graph).count({0, 1}) == 0, "non-finite vehicle position is safely excluded");
    graph.build(vehicles, {}, projection, std::numeric_limits<double>::infinity());
    check(graph.edgeCount() == 0, "non-finite search range is safely rejected");
}

bool validColoring(size_t count, const std::vector<InterferenceEdge>& edges,
                   const ColoringResult& result)
{
    if (result.colors.size() != count) return false;
    if (!count) return result.colorCount == 0;
    if (*std::min_element(result.colors.begin(), result.colors.end()) < 0) return false;
    if (*std::max_element(result.colors.begin(), result.colors.end()) + 1 != result.colorCount)
        return false;
    for (auto e : edges)
        if (result.colors[e.vehicleA] == result.colors[e.vehicleB]) return false;
    return true;
}

void testColoring()
{
    GraphColoring coloring;
    for (auto algo : {ColoringAlgorithm::Greedy, ColoringAlgorithm::WelshPowell}) {
        const std::string label = algo == ColoringAlgorithm::Greedy ? "greedy" : "Welsh-Powell";
        std::vector<InterferenceEdge> clique;
        for (size_t i = 0; i < 12; ++i)
            for (size_t j = i + 1; j < 12; ++j) clique.push_back({i, j, 1});
        auto result = coloring.color(12, clique, algo);
        check(validColoring(12, clique, result) && result.colorCount == 12,
              label + " complete graph K12");
        result = coloring.color(0, {}, algo);
        check(validColoring(0, {}, result), label + " empty graph");
        result = coloring.color(25, {}, algo);
        check(validColoring(25, {}, result) && result.colorCount == 1,
              label + " isolated vertices");
        std::mt19937 rng(42);
        bool valid = true;
        for (int trial = 0; trial < 100; ++trial) {
            const size_t n = 1 + rng() % 80;
            std::vector<InterferenceEdge> edges;
            for (size_t i = 0; i < n; ++i)
                for (size_t j = i + 1; j < n; ++j)
                    if (rng() % 100 < 25) edges.push_back({i, j, 1});
            valid &= validColoring(n, edges, coloring.color(n, edges, algo));
        }
        check(valid, label + " 100 seeded random graphs: no coloring conflict");
        const std::vector<InterferenceEdge> invalidEdges = {{0, 999, 1}, {0, 0, 1}};
        result = coloring.color(3, invalidEdges, algo);
        check(result.colors.size() == 3 && !GraphColoring::isValid(3, invalidEdges, result),
              label + " malformed edge indices and self-loops are handled without unsafe access");
    }
}

void testSimulation()
{
    RoadGraph graph;
    WebMercatorProjection projection(16, 47.75, 7.33);
    graph.build(smallData(), projection);
    VehicleSimulation sim;
    sim.setGraph(&graph);
    sim.setProjection(&projection);
    sim.setColoringEnabled(true);
    sim.spawn(10);
    check(sim.vehicles().size() == 10, "requested vehicle count created");
    check(sim.interferenceGraph().vertexCount() == 10 && sim.vehicleColors().size() == 10,
          "spawn while paused computes current graph and colors immediately");
    sim.updateInterferenceGraph();
    const auto before = sim.vehicles();
    sim.tick(1);
    bool unchanged = true;
    for (size_t i = 0; i < before.size(); ++i)
        unchanged &= before[i].x == sim.vehicles()[i].x && before[i].y == sim.vehicles()[i].y;
    check(unchanged, "paused simulation keeps vehicle positions");
    sim.setRunning(true);
    sim.tick(0.01);
    check(validColoring(sim.vehicles().size(), sim.interferenceGraph().edges(), sim.coloringResult()),
          "running simulation updates valid coloring");
    auto& vehicle = sim.vehicles()[0];
    vehicle.edgeIndex = 0;
    vehicle.previousNode = graph.edges()[0].from;
    vehicle.speed = 10;
    vehicle.progress = 0.2;
    sim.setSpeedMultiplier(1);
    sim.tick(0.01);
    const double normalProgress = vehicle.progress - 0.2;
    vehicle.progress = 0.2;
    sim.setSpeedMultiplier(2);
    sim.tick(0.01);
    check(std::abs(vehicle.progress - 0.2 - 2 * normalProgress) < 1e-12,
          "speed multiplier doubles traveled distance");
    vehicle.progress = 0.2;
    sim.setSpeedMultiplier(0.5);
    sim.tick(0.01);
    check(std::abs(vehicle.progress - 0.2 - 0.5 * normalProgress) < 1e-12,
          "slowing simulation halves traveled distance");
    sim.setRunning(false);
    sim.spawn(1);
    bool indicesValid = true;
    for (auto e : sim.interferenceGraph().edges())
        indicesValid &= e.vehicleA < sim.vehicles().size() && e.vehicleB < sim.vehicles().size();
    check(indicesValid, "respawn fewer vehicles removes all stale edge indices");
    check(sim.vehicleColors().size() == 1, "respawn refreshes coloring size while paused");
    sim.clear();
    check(sim.vehicles().empty() && sim.vehicleColors().empty() &&
              sim.coloringResult().colorCount == 0,
          "clear also clears coloring and displayed statistics");
}

void testBoundsAndComparison()
{
    WebMercatorProjection projection(16, 47.75, 7.33);
    RoadGraph graph;
    graph.build(smallData(), projection);
    VehicleSimulation sim;
    sim.setProjection(&projection);
    sim.setGraph(&graph);
    sim.setColoringEnabled(true);
    sim.spawn(64);
    const double x = graph.nodes().at(2).x;
    const double y = graph.nodes().at(1).y;
    check(sim.setBounds(x * .25, y - 1, x * .75, y + 1),
          "zone accepts clipped road even with both endpoints outside");
    sim.setRunning(true);
    sim.setSpeedMultiplier(8);
    bool inside = true;
    for (int tick = 0; tick < 80; ++tick) {
        sim.tick(.2);
        for (const auto& v : sim.vehicles())
            inside &= v.x >= x * .25 - 1e-8 && v.x <= x * .75 + 1e-8 &&
                      v.y >= y - 1 - 1e-8 && v.y <= y + 1 + 1e-8;
    }
    check(inside, "vehicles remain inside clipped zone after 80 accelerated updates");
    check(!sim.setBounds(1e6, 1e6, 1e6 + 1, 1e6 + 1) && sim.hasBounds(),
          "zone without roads is rejected and previous zone retained");
    check(!sim.setBounds(0, 0, std::numeric_limits<double>::infinity(), 1),
          "non-finite zone is rejected");
    sim.setRunning(false);
    sim.setColoringAlgorithm(ColoringAlgorithm::WelshPowell);
    GraphColoring solver;
    auto expected = solver.color(sim.vehicles().size(), sim.interferenceGraph().edges(), ColoringAlgorithm::WelshPowell);
    check(sim.vehicleColors() == expected.colors, "algorithm change recalculates colors while paused");
    const auto comparison = sim.compareColorings();
    check(comparison.valid && comparison.vehicles == 64 && comparison.edges == sim.interferenceGraph().edgeCount(),
          "comparison evaluates both methods on identical graph and validates results");
    sim.setTransmissionRadius(1);
    check(std::all_of(sim.vehicles().begin(), sim.vehicles().end(), [](const Vehicle& v) {
        return v.transmissionRadius == 1;
    }), "transmission range update reaches every vehicle while paused");
    sim.setColoringEnabled(false);
    check(sim.vehicleColors().empty() && sim.coloringResult().colorCount == 0,
          "disabling coloring clears displayed allocation");
    sim.clearBounds();
    check(!sim.hasBounds() && sim.vehicles().size() == 64, "clearing zone restores full road population");
    sim.spawn(0);
    check(sim.vehicles().empty() && sim.interferenceGraph().edgeCount() == 0,
          "zero population clears all network state");
    sim.setGraph(nullptr);
    sim.setRunning(true);
    sim.tick(1);
    check(sim.vehicles().empty(), "detached road graph can be ticked safely");
    check(GraphColoring::colorForIndex(3, 4) == GraphColoring::colorForIndex(3, 50),
          "channel display color remains stable as channel count changes");
}

int benchmark()
{
    using Clock = std::chrono::steady_clock;
    OsmData data;
    std::string error;
    if (!OsmParser::load(std::string(AUDIT_DATA_DIR) + "/mulhouse.osm", data, error)) {
        std::cout << "FAIL real dataset: " << error << std::endl;
        return 1;
    }
    const int zoom = chooseZoomForBounds(data.minLat, data.maxLat, data.minLon, data.maxLon);
    WebMercatorProjection projection(zoom, (data.minLat + data.maxLat) / 2,
                                    (data.minLon + data.maxLon) / 2);
    RoadGraph graph;
    auto start = Clock::now();
    graph.build(data, projection);
    const double buildMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    start = Clock::now();
    size_t intersections = 0;
    for (const auto& node : graph.nodes()) intersections += graph.degree(node.first) >= 3;
    const double degreeMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    std::cout << "DATASET nodes=" << graph.nodeCount() << " directed_edges=" << graph.edgeCount()
              << " intersections=" << intersections << " zoom=" << zoom
              << " build_ms=" << buildMs << " one_degree_scan_ms=" << degreeMs << std::endl;
    bool allValid = true;
    for (auto algo : {ColoringAlgorithm::Greedy, ColoringAlgorithm::WelshPowell}) {
        VehicleSimulation sim;
        sim.setGraph(&graph);
        sim.setProjection(&projection);
        sim.spawn(2000);
        std::mt19937 rng(2026);
        // Same initial population for both algorithms, regardless of spawn's random_device.
        for (auto& vehicle : sim.vehicles()) {
            vehicle.edgeIndex = rng() % graph.edgeCount();
            vehicle.previousNode = graph.edges()[vehicle.edgeIndex].from;
            vehicle.progress = (rng() % 1000) / 1000.0;
            vehicle.speed = 8 + (rng() % 600) / 100.0;
        }
        sim.setColoringAlgorithm(algo);
        sim.setColoringEnabled(true);
        sim.setRunning(true);
        sim.tick(0); // Establish positions without consuming route randomness.
        std::vector<double> times;
        bool valid = true;
        double coloringTotal = 0;
        for (int i = 0; i < 30; ++i) {
            start = Clock::now();
            sim.tick(1.0 / 30);
            times.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
            coloringTotal += sim.coloringResult().computationTimeMs;
            valid &= validColoring(sim.vehicles().size(), sim.interferenceGraph().edges(),
                                   sim.coloringResult());
        }
        // Compare both coloring methods on exactly the same final interference snapshot.
        GraphColoring coloring;
        auto greedy = coloring.color(2000, sim.interferenceGraph().edges(), ColoringAlgorithm::Greedy);
        auto welsh = coloring.color(2000, sim.interferenceGraph().edges(), ColoringAlgorithm::WelshPowell);
        InterferenceGraph reference;
        reference.setSpatialHashingEnabled(false);
        reference.build(sim.vehicles(), graph.nodes(), projection, 150);
        const bool referenceMatches = pairs(reference) == pairs(sim.interferenceGraph());
        allValid &= valid && referenceMatches && validColoring(2000, reference.edges(), greedy)
                    && validColoring(2000, reference.edges(), welsh);
        double total = 0;
        for (double t : times) total += t;
        std::sort(times.begin(), times.end());
        std::cout << "BENCH algorithm=" << (algo == ColoringAlgorithm::Greedy ? "greedy" : "Welsh-Powell")
                  << " vehicles=2000 ticks=30 mean_ms=" << total / times.size()
                  << " p95_ms=" << times[28] << " max_ms=" << times.back()
                  << " mean_coloring_ms=" << coloringTotal / times.size()
                  << " edges=" << sim.interferenceGraph().edgeCount()
                  << " colors=" << sim.coloringResult().colorCount
                  << " valid=" << valid
                  << " exhaustive_reference_matches=" << referenceMatches
                  << " same_snapshot_greedy_colors=" << greedy.colorCount
                  << " same_snapshot_welsh_colors=" << welsh.colorCount << std::endl;
    }
    return allValid ? 0 : 1;
}

int reloadMap(bool workflow = false)
{
    QTemporaryDir dir;
    QFile file(dir.filePath("tiny.osm"));
    file.open(QIODevice::WriteOnly);
    file.write("<osm><node id='1' lat='47.75' lon='7.33'/>"
               "<node id='2' lat='47.7502' lon='7.3302'/>"
               "<way id='1'><nd ref='1'/><nd ref='2'/>"
               "<tag k='highway' v='residential'/></way></osm>");
    file.close();
    // Prime the real tile cache so the regression is deterministic and works offline.
    QStandardPaths::setTestModeEnabled(true);
    double x0, y0, x1, y1;
    WebMercatorProjection::projectRaw(47.7502, 7.33, 18, x0, y0);
    WebMercatorProjection::projectRaw(47.75, 7.3302, 18, x1, y1);
    QImage image(256, 256, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int x = int(std::floor(x0 / 256)); x <= int(std::floor(x1 / 256)); ++x) {
        const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                         + QString("/osm-tiles/18/%1").arg(x);
        QDir().mkpath(cache);
        for (int y = int(std::floor(y0 / 256)); y <= int(std::floor(y1 / 256)); ++y)
            if (!image.save(cache + QString("/%1.png").arg(y))) return 2;
    }
    MapView view;
    QString error;
    check(view.loadOsmFile(file.fileName(), &error), "first map load with cached tiles");
    if (!workflow) {
        bool loaded = true;
        for (int i = 0; i < 10; ++i) loaded &= view.loadOsmFile(file.fileName(), &error);
        check(loaded, "ten consecutive map reloads with cached tiles");
        view.setShowBasemap(false);
        view.setShowBasemap(true);
        check(view.loadOsmFile(file.fileName(), &error), "reload after toggling basemap");
        const auto itemCount = view.scene()->items().size();
        QFile other(dir.filePath("uncached.osm"));
        other.open(QIODevice::WriteOnly);
        other.write("<osm><node id='1' lat='48.5' lon='8.45'/>"
                    "<node id='2' lat='48.5002' lon='8.4502'/>"
                    "<way id='1'><nd ref='1'/><nd ref='2'/>"
                    "<tag k='highway' v='residential'/></way></osm>");
        other.close();
        check(view.loadOsmFile(other.fileName(), &error) && view.loadOsmFile(file.fileName(), &error),
              "map change safely cancels pending uncached tile requests");
        QApplication::processEvents();
        check(view.scene()->items().size() == itemCount,
              "canceled tile replies do not add items to replacement map");
        return failed ? 1 : 0;
    }
    view.resize(900, 600);
    view.show();
    QApplication::processEvents();
    view.setSimulationColoringEnabled(true);
    view.spawnVehicles(30);
    view.compareColorings();
    check(view.comparison().valid && view.comparison().vehicles == 30, "GUI comparison uses current population");
    view.setShowInterferenceEdges(true);
    view.setShowIntersections(true);
    view.spawnVehicles(1);
    QImage canvas(view.size(), QImage::Format_ARGB32_Premultiplied);
    auto render = [&]() { QPainter painter(&canvas); view.render(&painter); };
    render();
    check(view.interferenceCount() == 0 && view.coloringResult().colors.size() == 1,
          "GUI redraw after shrinking population has no stale edges or colors");
    view.resetSimulation();
    check(!view.simulationRunning() && view.vehicleCount() == 1, "GUI reset pauses and preserves population count");
    // Use min/max directly: graph node iteration is intentionally unordered.
    double minX = 1e100, minY = 1e100, maxX = -1e100, maxY = -1e100;
    for (const auto& entry : view.roadGraph().nodes()) {
        minX = std::min(minX, entry.second.x); maxX = std::max(maxX, entry.second.x);
        minY = std::min(minY, entry.second.y); maxY = std::max(maxY, entry.second.y);
    }
    const QRectF zone(minX + (maxX - minX) * .25, minY + (maxY - minY) * .25,
                      (maxX - minX) * .5, (maxY - minY) * .5);
    check(view.setSimulationZone(zone) && view.hasSelectedZone(), "GUI zone changes simulation bounds");
    view.spawnVehicles(25);
    check(std::all_of(view.vehicles().begin(), view.vehicles().end(), [&](const Vehicle& v) {
        return zone.adjusted(-1e-7, -1e-7, 1e-7, 1e-7).contains(v.x, v.y);
    }), "GUI generated vehicles are all inside selected zone");
    check(!view.setSimulationZone(QRectF(1e6, 1e6, 1, 1)) && view.hasSelectedZone(),
          "GUI rejects empty road zone without losing previous selection");
    view.clearSimulationZone();
    check(!view.hasSelectedZone() && view.vehicleCount() == 25, "GUI clearing selection restores full network");
    view.compareColorings();
    const auto csv = dir.filePath("measures.csv");
    check(view.exportCsv(csv, &error), "CSV export succeeds");
    QFile exported(csv); exported.open(QIODevice::ReadOnly);
    const auto contents = exported.readAll();
    bool columns = true;
    for (const auto& row : contents.trimmed().split('\n')) columns &= row.split(',').size() == 11;
    check(columns && contents.contains("comparison,") && contents.contains("history,") &&
              contents.contains("Welsh-Powell"), "CSV contains history and both comparison rows with consistent columns");
    check(!view.exportCsv(dir.filePath("absent/measures.csv"), &error), "CSV write error is returned without crash");
    const auto previousCount = view.vehicleCount();
    check(!view.loadOsmFile(dir.filePath("absent.osm"), &error) && view.vehicleCount() == previousCount,
          "failed map load preserves previous simulation");
    render();
    return failed ? 1 : 0;
}
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("V2VAudit");
    const std::string mode = argc > 1 ? argv[1] : "core";
    if (mode == "reload") return reloadMap();
    if (mode == "workflow") return reloadMap(true);
    if (mode == "benchmark") return benchmark();
    testProjectionAndRoads();
    testParser();
    testInterference();
    testSpatialReference();
    testColoring();
    testSimulation();
    testBoundsAndComparison();
    std::cout << "TOTAL passed=" << passed << " failed=" << failed << std::endl;
    return failed ? 1 : 0;
}
