#include "MapView.h"
#include "OsmParser.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace {

    QColor colorFor(RoadCategory category)
    {
        switch (category) {
        case RoadCategory::Motorway:  return QColor(232, 90, 90);
        case RoadCategory::Primary:   return QColor(250, 166, 68);
        case RoadCategory::Secondary: return QColor(240, 214, 120);
        case RoadCategory::Local:     return QColor(150, 155, 165);
        }
        return Qt::white;
    }

    double widthFor(RoadCategory category)
    {
        switch (category) {
        case RoadCategory::Motorway:  return 3.2;
        case RoadCategory::Primary:   return 2.4;
        case RoadCategory::Secondary: return 1.6;
        case RoadCategory::Local:     return 1.0;
        }
        return 1.0;
    }

} // namespace

MapView::MapView(QWidget* parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(18, 20, 24));
    setScene(m_scene);

    m_tileLayer = new TileLayer(m_scene, this);

    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setFrameShape(QFrame::NoFrame);

    // Beaucoup d'objets qui bougent à chaque frame (véhicules) : l'index spatial par défaut
    // (BSP tree) coûte cher à maintenir ici, NoIndex est plus rapide dans ce cas précis.
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    m_simulation.setGraph(&m_graph);
    m_simulation.setProjection(&m_proj);
    m_simTimer = new QTimer(this);
    m_simTimer->setInterval(33); // ~30 Hz
    connect(m_simTimer, &QTimer::timeout, this, &MapView::onSimulationTick);
    m_simTimer->start();
    m_simClock.start();
}

void MapView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    emit viewResized();
}

void MapView::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        scale(m_zoomFactor, m_zoomFactor);
    else
        scale(1.0 / m_zoomFactor, 1.0 / m_zoomFactor);
}

void MapView::zoomIn() { scale(m_zoomFactor, m_zoomFactor); }
void MapView::zoomOut() { scale(1.0 / m_zoomFactor, 1.0 / m_zoomFactor); }

void MapView::setSelectionMode(bool on)
{
    m_selectionModeActive = on;
    setDragMode(on ? QGraphicsView::NoDrag : QGraphicsView::ScrollHandDrag);
    setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
    emit selectionModeChanged(on);
}

void MapView::mousePressEvent(QMouseEvent* event)
{
    if (m_selectionModeActive && event->button() == Qt::LeftButton) {
        m_drawingZone = true;
        m_zoneStartScenePos = mapToScene(event->pos());

        QPen pen(QColor(90, 220, 130));
        pen.setWidthF(2.0);
        pen.setCosmetic(true);
        QBrush brush(QColor(90, 220, 130, 45));

        if (m_zoneItem) {
            m_scene->removeItem(m_zoneItem);
            delete m_zoneItem;
        }
        m_zoneItem = m_scene->addRect(QRectF(m_zoneStartScenePos, QSizeF(0, 0)), pen, brush);
        m_zoneItem->setZValue(50);

        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drawingZone && m_zoneItem) {
        const QPointF current = mapToScene(event->pos());
        m_zoneItem->setRect(QRectF(m_zoneStartScenePos, current).normalized());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drawingZone && event->button() == Qt::LeftButton) {
        m_drawingZone = false;
        setSelectionMode(false); // repasse en mode déplacement après le tracé

        const QRectF r = m_zoneItem->rect();
        double lat1 = 0.0, lon1 = 0.0, lat2 = 0.0, lon2 = 0.0;
        m_proj.unproject(r.left(), r.top(), lat1, lon1);
        m_proj.unproject(r.right(), r.bottom(), lat2, lon2);

        emit zoneSelected(std::min(lat1, lat2), std::max(lat1, lat2),
            std::min(lon1, lon2), std::max(lon1, lon2));

        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

bool MapView::loadOsmFile(const QString& path, QString* errorMessage)
{
    OsmData data;
    std::string error;

    if (!OsmParser::load(path.toStdString(), data, error)) {
        if (errorMessage)
            *errorMessage = QString::fromStdString(error);
        return false;
    }

    const double centerLat = (data.minLat + data.maxLat) / 2.0;
    const double centerLon = (data.minLon + data.maxLon) / 2.0;
    const int zoom = chooseZoomForBounds(data.minLat, data.maxLat, data.minLon, data.maxLon);
    m_proj = WebMercatorProjection(zoom, centerLat, centerLon);

    m_scene->clear();
    if (m_zoneItem)
        m_zoneItem = nullptr; // supprimé par scene->clear()
    m_simulation.clear();

    const bool tilesOk = m_tileLayer->loadArea(data.minLat, data.maxLat, data.minLon, data.maxLon, m_proj);
    if (!tilesOk)
        emit tilesUnavailable();

    QPainterPath paths[4];

    for (const OsmWay& way : data.ways) {
        RoadCategory cat = OsmParser::categoryOf(way.highway);
        QPainterPath& target = paths[static_cast<int>(cat)];

        bool started = false;
        for (int64_t ref : way.nodeRefs) {
            auto it = data.nodes.find(ref);
            if (it == data.nodes.end()) {
                started = false;
                continue;
            }

            double x = 0.0, y = 0.0;
            m_proj.project(it->second.lat, it->second.lon, x, y);

            if (!started) {
                target.moveTo(x, y);
                started = true;
            }
            else {
                target.lineTo(x, y);
            }
        }
    }

    const RoadCategory order[4] = {
        RoadCategory::Local, RoadCategory::Secondary, RoadCategory::Primary, RoadCategory::Motorway
    };
    for (RoadCategory cat : order) {
        QPen pen(colorFor(cat));
        pen.setWidthF(widthFor(cat));
        pen.setCosmetic(true);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        m_scene->addPath(paths[static_cast<int>(cat)], pen);
    }

    m_graph.build(data, m_proj);
    drawIntersections();

    double sceneMinX = 0.0, sceneMinY = 0.0, sceneMaxX = 0.0, sceneMaxY = 0.0;
    m_proj.project(data.maxLat, data.minLon, sceneMinX, sceneMinY);
    m_proj.project(data.minLat, data.maxLon, sceneMaxX, sceneMaxY);
    const QRectF bounds = QRectF(QPointF(sceneMinX, sceneMinY), QPointF(sceneMaxX, sceneMaxY))
        .normalized().adjusted(-50, -50, 50, 50);
    m_scene->setSceneRect(bounds);

    resetTransform();
    fitInView(bounds, Qt::KeepAspectRatio);

    emit mapLoaded(static_cast<int>(data.ways.size()), static_cast<int>(data.nodes.size()));

    int intersectionCount = 0;
    for (const auto& pair : m_graph.nodes()) {
        if (m_graph.degree(pair.first) >= 2)
            ++intersectionCount;
    }
    emit graphBuilt(static_cast<int>(m_graph.nodeCount()),
        static_cast<int>(m_graph.edgeCount()),
        intersectionCount);

    return true;
}

void MapView::drawIntersections()
{
    // Une croix (X) plutôt qu'un point : plus lisible par-dessus le fond de carte/les routes.
    QPainterPath crossPath;
    const double s = 4.0; // demi-longueur des branches, en pixels écran (taille fixe, voir ci-dessous)
    crossPath.moveTo(-s, -s);
    crossPath.lineTo(s, s);
    crossPath.moveTo(-s, s);
    crossPath.lineTo(s, -s);

    QPen crossPen(QColor(255, 60, 60));
    crossPen.setWidthF(2.0);
    crossPen.setCosmetic(true);
    crossPen.setCapStyle(Qt::RoundCap);

    for (const auto& pair : m_graph.nodes()) {
        if (m_graph.degree(pair.first) < 2)
            continue;

        const GraphNode& n = pair.second;
        auto* cross = m_scene->addPath(crossPath, crossPen);
        cross->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        cross->setPos(n.x, n.y);
        cross->setZValue(10);
    }
}

// --- Étape 3 : simulation des véhicules ------------------------------------

void MapView::spawnVehicles(int count)
{
    m_simulation.spawn(count);
    viewport()->update();
}

void MapView::setSimulationRunning(bool running)
{
    m_simulation.setRunning(running);
    if (running)
        m_simClock.restart();
}

void MapView::setSimulationSpeed(double multiplier)
{
    m_simulation.setSpeedMultiplier(multiplier);
}

void MapView::resetSimulation()
{
    m_simulation.spawn(vehicleCount());
    viewport()->update();
}

void MapView::setShowInterferenceEdges(bool show)
{
    m_showInterferenceEdges = show;
    viewport()->update();
}

void MapView::setSimulationColoringAlgorithm(ColoringAlgorithm algo)
{
    m_simulation.setColoringAlgorithm(algo);
}

void MapView::setSimulationColoringEnabled(bool enabled)
{
    m_simulation.setColoringEnabled(enabled);
    if (enabled) {
        // Force une mise à jour immédiate pour afficher les stats
        m_simulation.updateInterferenceGraph();
        const auto& result = m_simulation.coloringResult();
        emit coloringStatsUpdated(result.colorCount, result.computationTimeMs);
    }
    viewport()->update();
}

void MapView::onSimulationTick()
{
    const double dt = m_simClock.restart() / 1000.0; // secondes réelles écoulées
    m_simulation.tick(dt);

    // Émettre les stats de coloration si activé
    if (m_simulation.coloringEnabled()) {
        const auto& result = m_simulation.coloringResult();
        emit coloringStatsUpdated(result.colorCount, result.computationTimeMs);
    }

    viewport()->update(); // redessine juste drawForeground() : pas de scène à réindexer
}

void MapView::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);

    const auto& vehicles = m_simulation.vehicles();
    if (vehicles.empty())
        return;

    // Un seul appel de peinture pour TOUS les véhicules, au lieu d'un QGraphicsItem par véhicule :
    // c'est ce qui rend l'affichage fluide même avec 2000+ véhicules en mouvement simultané.
    const double scale = painter->transform().m11();
    const double radius = scale > 0.0 ? (3.0 / scale) : 3.0; // 3 px écran, quel que soit le zoom
    const double margin = radius * 2.0;

    painter->setRenderHint(QPainter::Antialiasing, true);

    // --- Étape 4 : dessiner les arêtes d'interférences ---
    if (m_showInterferenceEdges) {
        const auto& interferenceGraph = m_simulation.interferenceGraph();
        if (interferenceGraph.edgeCount() > 0) {
            QPen edgePen(QColor(255, 180, 0, 180)); // Jaune orangé foncé semi-transparent
            edgePen.setWidthF(1.5);
            edgePen.setCosmetic(true);
            painter->setPen(edgePen);

            for (const InterferenceEdge& edge : interferenceGraph.edges()) {
                const Vehicle& va = vehicles[edge.vehicleA];
                const Vehicle& vb = vehicles[edge.vehicleB];

                // Culling : ne dessine que si au moins un des véhicules est visible
                if ((va.x < rect.left() - margin || va.x > rect.right() + margin ||
                     va.y < rect.top() - margin || va.y > rect.bottom() + margin) &&
                    (vb.x < rect.left() - margin || vb.x > rect.right() + margin ||
                     vb.y < rect.top() - margin || vb.y > rect.bottom() + margin))
                    continue;

                painter->drawLine(QPointF(va.x, va.y), QPointF(vb.x, vb.y));
            }
        }
    }

    // Dessiner les véhicules
    painter->setPen(Qt::NoPen);

    const bool useColoring = m_simulation.coloringEnabled();
    const auto& vehicleColors = m_simulation.vehicleColors();
    const int colorCount = m_simulation.coloringResult().colorCount;

    for (size_t i = 0; i < vehicles.size(); ++i) {
        const Vehicle& v = vehicles[i];

        // Ne dessine pas les véhicules hors de la zone visible à l'écran (culling)
        if (v.x < rect.left() - margin || v.x > rect.right() + margin ||
            v.y < rect.top() - margin || v.y > rect.bottom() + margin)
            continue;

        // Utiliser la couleur de coloration si activée, sinon bleu par défaut
        if (useColoring && i < vehicleColors.size() && vehicleColors[i] >= 0) {
            painter->setBrush(GraphColoring::colorForIndex(vehicleColors[i], colorCount));
        } else {
            painter->setBrush(QColor(70, 170, 255));
        }

        painter->drawEllipse(QPointF(v.x, v.y), radius, radius);
    }
}