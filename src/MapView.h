#pragma once

#include <QElapsedTimer>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QString>
#include <QTimer>
#include <QWheelEvent>

#include "RoadGraph.h"
#include "TileLayer.h"
#include "VehicleSimulation.h"
#include "WebMercator.h"

class MapView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MapView(QWidget* parent = nullptr);

    bool loadOsmFile(const QString& path, QString* errorMessage = nullptr);

    const RoadGraph& roadGraph() const { return m_graph; }

    void zoomIn();
    void zoomOut();

    void setSelectionMode(bool on);
    bool selectionMode() const { return m_selectionModeActive; }
    bool hasSelectedZone() const { return m_zoneItem != nullptr; }

    // --- Étape 3 : simulation des véhicules ---
    void spawnVehicles(int count);
    void setSimulationRunning(bool running);
    bool simulationRunning() const { return m_simulation.running(); }
    void setSimulationSpeed(double multiplier);
    double simulationSpeed() const { return m_simulation.speedMultiplier(); }
    void resetSimulation();
    int vehicleCount() const { return static_cast<int>(m_simulation.vehicles().size()); }

    // --- Étape 4 : affichage des arêtes d'interférences ---
    void setShowInterferenceEdges(bool show);
    bool showInterferenceEdges() const { return m_showInterferenceEdges; }

    // --- Étape 5 : coloration du graphe ---
    void setSimulationColoringAlgorithm(ColoringAlgorithm algo);
    void setSimulationColoringEnabled(bool enabled);
    bool simulationColoringEnabled() const { return m_simulation.coloringEnabled(); }

signals:
    void mapLoaded(int wayCount, int nodeCount);
    void graphBuilt(int nodeCount, int edgeCount, int intersectionCount);
    void tilesUnavailable();
    void zoneSelected(double minLat, double maxLat, double minLon, double maxLon);
    void selectionModeChanged(bool active);
    void viewResized();
    void coloringStatsUpdated(int colorCount, double timeMs);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private slots:
    void onSimulationTick();

private:
    void drawIntersections();

    QGraphicsScene* m_scene;
    RoadGraph m_graph;
    TileLayer* m_tileLayer;
    WebMercatorProjection m_proj;
    double m_zoomFactor = 1.15;

    bool m_selectionModeActive = false;
    bool m_drawingZone = false;
    QPointF m_zoneStartScenePos;
    QGraphicsRectItem* m_zoneItem = nullptr;

    bool m_showInterferenceEdges = true;

    VehicleSimulation m_simulation;
    QTimer* m_simTimer;
    QElapsedTimer m_simClock;
};