#include "InterferenceGraph.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace {
constexpr double earthRadius = 6371000.0;
constexpr double pi = 3.14159265358979323846;
using Point = std::array<double, 3>;
struct Cell {
    int x, y, z;
    bool operator==(const Cell& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};
struct CellHash {
    size_t operator()(const Cell& c) const {
        size_t h = std::hash<int>{}(c.x);
        h ^= std::hash<int>{}(c.y) + size_t(0x9e3779b9) + (h << 6) + (h >> 2);
        return h ^ (std::hash<int>{}(c.z) + size_t(0x9e3779b9) + (h << 6) + (h >> 2));
    }
};
double chord(double meters) {
    return 2 * earthRadius * std::sin(std::min(meters / earthRadius, pi) * 0.5);
}
}

InterferenceGraph::InterferenceGraph() = default;

void InterferenceGraph::clear()
{
    m_edges.clear();
    m_vertexCount = 0;
}

void InterferenceGraph::build(const std::vector<Vehicle>& vehicles,
                              const std::unordered_map<int64_t, GraphNode>&,
                              const WebMercatorProjection& projection, double maxRange)
{
    clear();
    m_vertexCount = vehicles.size();
    if (vehicles.empty() || !std::isfinite(maxRange) || maxRange <= 0) return;

    // One conversion per vehicle. Earth-centered coordinates are in meters:
    // no zoom/latitude approximation, including across the date line.
    std::vector<Point> positions(vehicles.size());
    std::vector<bool> valid(vehicles.size(), false);
    for (size_t i = 0; i < vehicles.size(); ++i) {
        if (!std::isfinite(vehicles[i].x) || !std::isfinite(vehicles[i].y)) continue;
        double lat, lon;
        projection.unproject(vehicles[i].x, vehicles[i].y, lat, lon);
        if (!std::isfinite(lat) || !std::isfinite(lon)) continue;
        lat *= pi / 180;
        lon *= pi / 180;
        const double r = earthRadius * std::cos(lat);
        positions[i] = {r * std::cos(lon), r * std::sin(lon), earthRadius * std::sin(lat)};
        valid[i] = true;
    }
    auto compare = [&](size_t i, size_t j) {
        if (i >= j || !valid[i] || !valid[j]) return;
        // Conflict if either transmitter reaches the other; maxRange caps the search.
        const double range = std::min(maxRange, std::max(vehicles[i].transmissionRadius,
                                                       vehicles[j].transmissionRadius));
        if (!std::isfinite(range) || range <= 0) return;
        const auto& a = positions[i];
        const auto& b = positions[j];
        const double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        const double squared = dx * dx + dy * dy + dz * dz;
        const double threshold = chord(range);
        if (squared >= threshold * threshold && range <= pi * earthRadius) return;
        const double distance = 2 * earthRadius * std::asin(
            std::clamp(std::sqrt(squared) / (2 * earthRadius), 0.0, 1.0));
        if (distance < range) m_edges.push_back({i, j, distance});
    };

    if (!m_useSpatialHashing || vehicles.size() <= 100) {
        for (size_t i = 0; i < vehicles.size(); ++i)
            for (size_t j = i + 1; j < vehicles.size(); ++j) compare(i, j);
        return;
    }

    // Each Cartesian component of a chord is at most its length, so these
    // neighboring cubes cover every conflict. Dense output remains quadratic.
    const double cellSize = std::max(1.0, chord(maxRange));
    auto key = [cellSize](const Point& p) {
        return Cell{int(std::floor(p[0] / cellSize)), int(std::floor(p[1] / cellSize)),
                    int(std::floor(p[2] / cellSize))};
    };
    std::unordered_map<Cell, std::vector<size_t>, CellHash> grid;
    grid.reserve(vehicles.size());
    for (size_t i = 0; i < vehicles.size(); ++i)
        if (valid[i]) grid[key(positions[i])].push_back(i);
    for (size_t i = 0; i < vehicles.size(); ++i) {
        if (!valid[i]) continue;
        const auto cell = key(positions[i]);
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
                for (int z = -1; z <= 1; ++z) {
                    auto it = grid.find({cell.x + x, cell.y + y, cell.z + z});
                    if (it != grid.end())
                        for (size_t j : it->second) compare(i, j);
                }
    }
}
