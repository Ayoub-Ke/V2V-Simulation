#pragma once

#include <QElapsedTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include "RoadGraph.h"
#include "TileLayer.h"
#include "VehicleSimulation.h"

struct SimulationSample {
    SimulationStats stats;
    size_t vehicles = 0, edges = 0;
    int colors = 0;
    double coloringMs = 0;
    ColoringAlgorithm algorithm = ColoringAlgorithm::Greedy;
};

class MapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MapView(QWidget* parent = nullptr);
    ~MapView() override;
    bool loadOsmFile(const QString& path, QString* errorMessage = nullptr);
    const RoadGraph& roadGraph() const { return m_graph; }
    void zoomIn();
    void zoomOut();
    void fitMap();
    void setSelectionMode(bool on);
    bool selectionMode() const { return m_selectionModeActive; }
    bool hasSelectedZone() const { return !m_selectedZone.isEmpty(); }
    bool setSimulationZone(const QRectF& bounds);
    void clearSimulationZone();
    void spawnVehicles(int count);
    void setSimulationRunning(bool running);
    bool simulationRunning() const { return m_simulation.running(); }
    void setSimulationSpeed(double multiplier);
    double simulationSpeed() const { return m_simulation.speedMultiplier(); }
    void setTransmissionRadius(double meters);
    void resetSimulation();
    int vehicleCount() const { return int(m_simulation.vehicles().size()); }
    const std::vector<Vehicle>& vehicles() const { return m_simulation.vehicles(); }
    void setShowInterferenceEdges(bool show);
    bool showInterferenceEdges() const { return m_showInterferenceEdges; }
    void setShowIntersections(bool show);
    void setShowBasemap(bool show);
    void setSimulationColoringAlgorithm(ColoringAlgorithm algo);
    void setSimulationColoringEnabled(bool enabled);
    bool simulationColoringEnabled() const { return m_simulation.coloringEnabled(); }
    const SimulationStats& simulationStats() const { return m_simulation.stats(); }
    const ColoringResult& coloringResult() const { return m_simulation.coloringResult(); }
    size_t interferenceCount() const { return m_simulation.interferenceGraph().edgeCount(); }
    const std::vector<SimulationSample>& history() const { return m_history; }
    const ColoringComparison& comparison() const { return m_comparison; }
    void compareColorings();
    bool exportCsv(const QString& path, QString* errorMessage = nullptr) const;

signals:
    void mapLoaded(int wayCount, int nodeCount);
    void graphBuilt(int nodeCount, int edgeCount, int intersectionCount);
    void tilesUnavailable();
    void zoneSelected(double minLat, double maxLat, double minLon, double maxLon);
    void selectionModeChanged(bool active);
    void viewResized();
    void coloringStatsUpdated(int colorCount, double timeMs);
    void statisticsUpdated();
    void comparisonUpdated();
    void statusMessage(const QString& message);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    void onSimulationTick();
    void drawIntersections();
    void publishStatistics(bool forceSample = false);
    void restoreZoneItem();
    QGraphicsScene* m_scene;
    RoadGraph m_graph;
    TileLayer* m_tileLayer;
    WebMercatorProjection m_proj;
    VehicleSimulation m_simulation;
    QTimer* m_simTimer;
    QElapsedTimer m_simClock;
    QRectF m_mapBounds, m_selectedZone;
    QPointF m_zoneStartScenePos;
    QPoint m_clickStart;
    QGraphicsRectItem* m_zoneItem = nullptr;
    QList<QGraphicsItem*> m_intersections;
    std::vector<SimulationSample> m_history;
    ColoringComparison m_comparison;
    bool m_selectionModeActive = false, m_drawingZone = false;
    bool m_showInterferenceEdges = false, m_showIntersections = false;
    bool m_showBasemap = true;
    int m_selectedVehicle = -1;
    double m_minLat = 0, m_maxLat = 0, m_minLon = 0, m_maxLon = 0;
};
