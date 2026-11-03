#include "MapView.h"
#include "OsmParser.h"

#include <QFile>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

MapView::MapView(QWidget* parent) : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QColor("#eaf0f1"));
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(m_scene);
    m_tileLayer = new TileLayer(m_scene, this);
    setDragMode(ScrollHandDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_simulation.setGraph(&m_graph);
    m_simulation.setProjection(&m_proj);
    m_simTimer = new QTimer(this);
    m_simTimer->setTimerType(Qt::PreciseTimer);
    m_simTimer->setInterval(33);
    connect(m_simTimer, &QTimer::timeout, this, &MapView::onSimulationTick);
    m_simTimer->start();
    m_simClock.start();
}

MapView::~MapView()
{
    m_simTimer->stop();
    m_tileLayer->clear(); // Remove owned tiles before QGraphicsScene destroys its items.
}

void MapView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    emit viewResized();
}

void MapView::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0) zoomIn();
    else if (event->angleDelta().y() < 0) zoomOut();
    event->accept();
}

void MapView::zoomIn() { if (transform().m11() < 30) scale(1.2, 1.2); }
void MapView::zoomOut() { if (transform().m11() > 0.04) scale(1 / 1.2, 1 / 1.2); }
void MapView::fitMap() { if (!m_mapBounds.isEmpty()) fitInView(m_mapBounds, Qt::KeepAspectRatio); }

void MapView::setSelectionMode(bool on)
{
    m_selectionModeActive = on;
    setDragMode(on ? NoDrag : ScrollHandDrag);
    setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
    emit selectionModeChanged(on);
}

void MapView::restoreZoneItem()
{
    if (m_zoneItem) { delete m_zoneItem; m_zoneItem = nullptr; }
    if (m_selectedZone.isEmpty()) return;
    QPen pen(QColor("#079b83"), 1.6, Qt::DashLine);
    pen.setCosmetic(true);
    m_zoneItem = m_scene->addRect(m_selectedZone, pen, QColor(14, 165, 139, 22));
    m_zoneItem->setZValue(20);
}

void MapView::mousePressEvent(QMouseEvent* event)
{
    m_clickStart = event->pos();
    if (m_selectionModeActive && event->button() == Qt::LeftButton && m_graph.edgeCount()) {
        m_drawingZone = true;
        m_zoneStartScenePos = mapToScene(event->pos());
        if (m_zoneItem) delete m_zoneItem;
        QPen pen(QColor("#079b83"), 1.6, Qt::DashLine);
        pen.setCosmetic(true);
        m_zoneItem = m_scene->addRect(QRectF(m_zoneStartScenePos, QSizeF()), pen, QColor(14, 165, 139, 28));
        m_zoneItem->setZValue(20);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drawingZone && m_zoneItem) {
        m_zoneItem->setRect(QRectF(m_zoneStartScenePos, mapToScene(event->pos())).normalized());
        event->accept();
    } else QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drawingZone && event->button() == Qt::LeftButton) {
        m_drawingZone = false;
        setSelectionMode(false);
        const QRectF area = m_zoneItem ? m_zoneItem->rect() : QRectF();
        if ((event->pos() - m_clickStart).manhattanLength() < 6) restoreZoneItem();
        else setSimulationZone(area);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && (event->pos() - m_clickStart).manhattanLength() < 5) {
        m_selectedVehicle = -1;
        double best = 100;
        for (size_t i = 0; i < vehicles().size(); ++i) {
            const auto point = mapFromScene(vehicles()[i].x, vehicles()[i].y);
            const auto delta = point - event->pos();
            const double squared = delta.x() * delta.x() + delta.y() * delta.y();
            if (squared < best) { best = squared; m_selectedVehicle = int(i); }
        }
        if (m_selectedVehicle >= 0) {
            const auto& v = vehicles()[size_t(m_selectedVehicle)];
            const auto& colors = m_simulation.vehicleColors();
            const auto channel = size_t(m_selectedVehicle) < colors.size()
                ? QString::number(colors[size_t(m_selectedVehicle)] + 1) : QString("—");
            emit statusMessage(QString("Véhicule %1 · %2 km/h · canal %3 · portée %4 m")
                .arg(m_selectedVehicle + 1).arg(v.speed * 3.6, 0, 'f', 0).arg(channel)
                .arg(v.transmissionRadius, 0, 'f', 0));
        }
        viewport()->update();
    }
}

bool MapView::setSimulationZone(const QRectF& bounds)
{
    const auto area = bounds.normalized();
    if (area.isEmpty() || !m_simulation.setBounds(area.left(), area.top(), area.right(), area.bottom())) {
        restoreZoneItem();
        emit statusMessage("Aucune route dans cette zone. La sélection précédente est conservée.");
        return false;
    }
    m_selectedZone = area;
    restoreZoneItem();
    m_selectedVehicle = -1;
    m_history.clear();
    m_comparison = {};
    m_simClock.restart();
    double lat1, lon1, lat2, lon2;
    m_proj.unproject(area.left(), area.top(), lat1, lon1);
    m_proj.unproject(area.right(), area.bottom(), lat2, lon2);
    emit zoneSelected(std::min(lat1, lat2), std::max(lat1, lat2), std::min(lon1, lon2), std::max(lon1, lon2));
    publishStatistics(true);
    emit comparisonUpdated();
    return true;
}

void MapView::clearSimulationZone()
{
    m_selectedZone = {};
    restoreZoneItem();
    m_simulation.clearBounds();
    m_history.clear();
    m_comparison = {};
    m_selectedVehicle = -1;
    publishStatistics(true);
    emit comparisonUpdated();
    emit statusMessage("Simulation sur l'ensemble du réseau routier.");
}

bool MapView::loadOsmFile(const QString& path, QString* errorMessage)
{
    OsmData data;
    std::string error;
    if (!OsmParser::load(path.toStdString(), data, error)) {
        if (errorMessage) *errorMessage = QString::fromStdString(error);
        return false;
    }
    WebMercatorProjection projection(chooseZoomForBounds(data.minLat, data.maxLat, data.minLon, data.maxLon),
        (data.minLat + data.maxLat) / 2, (data.minLon + data.maxLon) / 2);
    RoadGraph graph;
    graph.build(data, projection);
    if (!graph.edgeCount()) {
        if (errorMessage) *errorMessage = "Le fichier ne contient aucun tronçon routier utilisable.";
        return false;
    }
    m_tileLayer->clear();
    m_intersections.clear();
    m_scene->clear();
    m_zoneItem = nullptr;
    m_selectedZone = {};
    m_drawingZone = false;
    setSelectionMode(false);
    m_selectedVehicle = -1;
    m_proj = projection;
    m_graph = std::move(graph);
    m_simulation.setRunning(false);
    m_simulation.setGraph(&m_graph);
    m_history.clear();
    m_comparison = {};
    m_minLat = data.minLat; m_maxLat = data.maxLat;
    m_minLon = data.minLon; m_maxLon = data.maxLon;
    if (m_showBasemap && !m_tileLayer->loadArea(m_minLat, m_maxLat, m_minLon, m_maxLon, m_proj))
        emit tilesUnavailable();
    m_tileLayer->setVisible(m_showBasemap);

    QPainterPath paths[4];
    for (const auto& way : data.ways) {
        auto& target = paths[int(OsmParser::categoryOf(way.highway))];
        bool started = false;
        for (auto ref : way.nodeRefs) {
            auto it = data.nodes.find(ref);
            if (it == data.nodes.end()) { started = false; continue; }
            double x, y;
            m_proj.project(it->second.lat, it->second.lon, x, y);
            if (started) target.lineTo(x, y); else target.moveTo(x, y);
            started = true;
        }
    }
    const double widths[] = {4.5, 3.8, 3.0, 2.2};
    const QColor colors[] = {QColor("#e7c9a0"), QColor("#eee1c6"), QColor("#ffffff"), QColor("#ffffff")};
    for (int category : {3, 2, 1, 0}) {
        QPen outline(QColor("#bdcdd1"), widths[category] + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        outline.setCosmetic(true);
        m_scene->addPath(paths[category], outline);
        QPen road(colors[category], widths[category], Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        road.setCosmetic(true);
        m_scene->addPath(paths[category], road);
    }
    drawIntersections();
    double x0, y0, x1, y1;
    m_proj.project(data.maxLat, data.minLon, x0, y0);
    m_proj.project(data.minLat, data.maxLon, x1, y1);
    m_mapBounds = QRectF(QPointF(x0, y0), QPointF(x1, y1)).normalized().adjusted(-35, -35, 35, 35);
    m_scene->setSceneRect(m_mapBounds.adjusted(-150, -150, 150, 150));
    resetTransform();
    fitMap();
    m_simClock.restart();
    emit mapLoaded(int(data.ways.size()), int(data.nodes.size()));
    emit graphBuilt(int(m_graph.nodeCount()), int(m_graph.edgeCount()), int(m_intersections.size()));
    publishStatistics(true);
    emit comparisonUpdated();
    return true;
}

void MapView::drawIntersections()
{
    for (const auto& entry : m_graph.nodes()) {
        if (m_graph.degree(entry.first) < 3) continue;
        auto* point = m_scene->addEllipse(-2.2, -2.2, 4.4, 4.4,
            QPen(QColor("#ffffff"), 0.8), QBrush(QColor("#627e89")));
        point->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        point->setPos(entry.second.x, entry.second.y);
        point->setZValue(8);
        point->setVisible(m_showIntersections);
        m_intersections.append(point);
    }
}

void MapView::spawnVehicles(int count)
{
    m_simulation.spawn(count);
    m_selectedVehicle = -1;
    m_history.clear();
    m_comparison = {};
    m_simClock.restart();
    publishStatistics(true);
    emit comparisonUpdated();
}
void MapView::setSimulationRunning(bool running)
{
    m_simulation.setRunning(running && vehicleCount() > 0);
    m_simClock.restart();
    emit statisticsUpdated();
}
void MapView::setSimulationSpeed(double multiplier) { m_simulation.setSpeedMultiplier(multiplier); }
void MapView::setTransmissionRadius(double meters)
{
    m_simulation.setTransmissionRadius(meters);
    publishStatistics(true);
}
void MapView::resetSimulation()
{
    m_simulation.setRunning(false);
    spawnVehicles(vehicleCount());
}
void MapView::setShowInterferenceEdges(bool show) { m_showInterferenceEdges = show; viewport()->update(); }
void MapView::setShowIntersections(bool show)
{
    m_showIntersections = show;
    for (auto* item : m_intersections) item->setVisible(show);
}
void MapView::setShowBasemap(bool show)
{
    if (show && !m_showBasemap && m_graph.edgeCount())
        if (!m_tileLayer->loadArea(m_minLat, m_maxLat, m_minLon, m_maxLon, m_proj)) emit tilesUnavailable();
    m_showBasemap = show;
    m_tileLayer->setVisible(show);
}
void MapView::setSimulationColoringAlgorithm(ColoringAlgorithm algo)
{
    m_simulation.setColoringAlgorithm(algo);
    publishStatistics(true);
}
void MapView::setSimulationColoringEnabled(bool enabled)
{
    m_simulation.setColoringEnabled(enabled);
    publishStatistics(true);
}
void MapView::onSimulationTick()
{
    const double dt = m_simClock.restart() / 1000.0;
    if (!m_simulation.running()) return;
    m_simulation.tick(std::min(dt, 0.15));
    publishStatistics();
}
void MapView::publishStatistics(bool forceSample)
{
    const auto& stats = m_simulation.stats();
    const auto& result = m_simulation.coloringResult();
    SimulationSample sample{stats, vehicles().size(), interferenceCount(), result.colorCount,
                            result.computationTimeMs, m_simulation.coloringAlgorithm()};
    if (forceSample && !m_history.empty() && m_history.back().stats.elapsedSeconds == stats.elapsedSeconds)
        m_history.back() = sample;
    else if (m_history.empty() || forceSample || stats.elapsedSeconds - m_history.back().stats.elapsedSeconds >= 0.25) {
        if (m_history.size() >= 3600) m_history.erase(m_history.begin());
        m_history.push_back(sample);
    }
    emit coloringStatsUpdated(result.colorCount, result.computationTimeMs);
    emit statisticsUpdated();
    viewport()->update();
}
void MapView::compareColorings()
{
    m_comparison = m_simulation.compareColorings();
    emit comparisonUpdated();
}

bool MapView::exportCsv(const QString& path, QString* errorMessage) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    QTextStream stream(&file);
    stream.setLocale(QLocale::c());
    stream << "type,time_s,vehicles,edges,algorithm,colors,graph_ms,coloring_ms,update_ms,changed_channels,valid\n";
    for (const auto& s : m_history)
        stream << "history," << s.stats.elapsedSeconds << ',' << s.vehicles << ',' << s.edges << ','
               << (s.algorithm == ColoringAlgorithm::Greedy ? "Greedy" : "Welsh-Powell") << ','
               << s.colors << ',' << s.stats.graphTimeMs << ',' << s.coloringMs << ','
               << s.stats.updateTimeMs << ',' << s.stats.channelChanges << ",\n";
    if (m_comparison.vehicles) {
        auto row = [&](const char* name, const ColoringResult& r) {
            stream << "comparison," << m_comparison.timeSeconds << ',' << m_comparison.vehicles << ','
                   << m_comparison.edges << ',' << name << ',' << r.colorCount << ",," << r.computationTimeMs
                   << ",,," << (m_comparison.valid ? 1 : 0) << '\n';
        };
        row("Greedy", m_comparison.greedy); row("Welsh-Powell", m_comparison.welshPowell);
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

void MapView::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);
    painter->save();
    const auto& population = vehicles();
    const double zoom = std::max(0.0001, std::abs(painter->transform().m11()));
    const double radius = (population.size() > 1000 ? 2.4 : 3.2) / zoom;
    const bool selected = m_selectedVehicle >= 0 && size_t(m_selectedVehicle) < population.size();
    if (m_showInterferenceEdges || selected) {
        QVector<QLineF> lines;
        for (const auto& edge : m_simulation.interferenceGraph().edges()) {
            if (edge.vehicleA >= population.size() || edge.vehicleB >= population.size()) continue;
            if (!m_showInterferenceEdges && edge.vehicleA != size_t(m_selectedVehicle) &&
                edge.vehicleB != size_t(m_selectedVehicle)) continue;
            const auto& a = population[edge.vehicleA]; const auto& b = population[edge.vehicleB];
            if (!QRectF(QPointF(a.x, a.y), QPointF(b.x, b.y)).normalized().adjusted(-1, -1, 1, 1).intersects(rect)) continue;
            lines.append(QLineF(a.x, a.y, b.x, b.y));
        }
        QPen pen(QColor(16, 116, 135, m_showInterferenceEdges ? 24 : 155), 0.8);
        pen.setCosmetic(true); painter->setPen(pen);
        painter->drawLines(lines);
    }
    const auto& colors = m_simulation.vehicleColors();
    const int count = m_simulation.coloringEnabled() ? m_simulation.coloringResult().colorCount : 0;
    std::vector<QPainterPath> markers(size_t(std::max(1, count)));
    for (size_t i = 0; i < population.size(); ++i) {
        const auto& v = population[i];
        if (!rect.adjusted(-radius * 2, -radius * 2, radius * 2, radius * 2).contains(v.x, v.y)) continue;
        const int color = i < colors.size() ? colors[i] : 0;
        const size_t bucket = count > 0 && color >= 0 && color < count ? size_t(color) : 0;
        markers[bucket].addEllipse(QPointF(v.x, v.y), radius, radius);
    }
    QPen border(QColor(255, 255, 255, 235), 0.7); border.setCosmetic(true);
    painter->setPen(border);
    for (size_t c = 0; c < markers.size(); ++c) {
        painter->setBrush(count > 0 ? GraphColoring::colorForIndex(int(c), count) : QColor("#087f8c"));
        painter->drawPath(markers[c]);
    }
    if (selected) {
        const auto& v = population[size_t(m_selectedVehicle)];
        double lat, lon; m_proj.unproject(v.x, v.y, lat, lon);
        const double metersPerPixel = std::cos(lat * WebMercatorProjection::kDegToRad) * 2 * WebMercatorProjection::kPi
            * 6371000 / (256 * std::pow(2, m_proj.zoom));
        const double range = v.transmissionRadius / std::max(1e-9, metersPerPixel);
        QPen pen(QColor("#087f8c"), 1.4, Qt::DashLine); pen.setCosmetic(true);
        painter->setPen(pen); painter->setBrush(QColor(8, 127, 140, 15));
        painter->drawEllipse(QPointF(v.x, v.y), range, range);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(v.x, v.y), 7 / zoom, 7 / zoom);
    }
    painter->restore();
    painter->save();
    painter->resetTransform();
    painter->setPen(QColor("#617581"));
    QFont font = painter->font(); font.setPixelSize(10); painter->setFont(font);
    painter->drawText(QRect(20, viewport()->height() - 28, viewport()->width() - 40, 20),
                      Qt::AlignRight | Qt::AlignVCenter, "© OpenStreetMap contributors");
    double lat, lon; const auto center = mapToScene(viewport()->rect().center());
    m_proj.unproject(center.x(), center.y(), lat, lon);
    const double mpp = std::cos(lat * WebMercatorProjection::kDegToRad) * 2 * WebMercatorProjection::kPi
        * 6371000 / (256 * std::pow(2, m_proj.zoom)) / zoom;
    if (m_graph.edgeCount() && std::isfinite(mpp) && mpp > 0) {
        const double raw = mpp * 100, base = std::pow(10, std::floor(std::log10(raw)));
        const double length = (raw / base >= 5 ? 5 : raw / base >= 2 ? 2 : 1) * base;
        const int pixels = int(length / mpp);
        painter->setPen(QPen(QColor("#526b76"), 1.5));
        const int y = viewport()->height() - 20;
        painter->drawLine(20, y, 20 + pixels, y);
        painter->drawLine(20, y - 4, 20, y); painter->drawLine(20 + pixels, y - 4, 20 + pixels, y);
        painter->drawText(20, y - 8, length >= 1000 ? QString("%1 km").arg(length / 1000) : QString("%1 m").arg(length));
    }
    painter->restore();
}
