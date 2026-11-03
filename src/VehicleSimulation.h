#pragma once

#include "RoadGraph.h"
#include "Vehicle.h"
#include "InterferenceGraph.h"
#include "GraphColoring.h"
#include <array>
#include <random>
#include <vector>

struct SimulationStats {
    double elapsedSeconds = 0;
    double graphTimeMs = 0;
    double updateTimeMs = 0;
    size_t channelChanges = 0;
};

struct ColoringComparison {
    ColoringResult greedy, welshPowell;
    size_t vehicles = 0, edges = 0;
    double timeSeconds = 0;
    bool valid = true;
};

class VehicleSimulation {
public:
    void setGraph(const RoadGraph* graph);
    void setProjection(const WebMercatorProjection* proj) { m_projection = proj; }
    void spawn(int count);
    void clear();
    void tick(double dtSeconds);
    void setRunning(bool running) { m_running = running; }
    bool running() const { return m_running; }
    void setSpeedMultiplier(double mult);
    double speedMultiplier() const { return m_speedMultiplier; }
    void setTransmissionRadius(double meters);
    double transmissionRadius() const { return m_transmissionRadius; }

    // Bounds are in scene coordinates. Segments crossing the boundary are clipped,
    // and vehicles leaving it are reinserted on a road inside the selected area.
    bool setBounds(double minX, double minY, double maxX, double maxY);
    void clearBounds();
    bool hasBounds() const { return m_bounded; }
    std::vector<Vehicle>& vehicles() { return m_vehicles; }
    const std::vector<Vehicle>& vehicles() const { return m_vehicles; }
    const InterferenceGraph& interferenceGraph() const { return m_interferenceGraph; }
    void updateInterferenceGraph();
    void setColoringAlgorithm(ColoringAlgorithm algo);
    ColoringAlgorithm coloringAlgorithm() const { return m_coloringAlgorithm; }
    void setColoringEnabled(bool enabled);
    bool coloringEnabled() const { return m_coloringEnabled; }
    const ColoringResult& coloringResult() const { return m_coloringResult; }
    const std::vector<int>& vehicleColors() const { return m_coloringResult.colors; }
    const SimulationStats& stats() const { return m_stats; }
    ColoringComparison compareColorings();

private:
    struct Span { double begin = 0, end = 0; };
    void rebuildSpans();
    void placeOnRandomEdge(Vehicle& v);
    void advance(Vehicle& v, double distanceMeters);
    void updateScreenPosition(Vehicle& v) const;

    const RoadGraph* m_graph = nullptr;
    const WebMercatorProjection* m_projection = nullptr;
    std::vector<Vehicle> m_vehicles;
    std::vector<Span> m_spans;
    std::vector<size_t> m_spawnEdges;
    std::vector<double> m_spawnWeights;
    bool m_bounded = false;
    std::array<double, 4> m_bounds{};
    bool m_running = false;
    double m_speedMultiplier = 1;
    double m_transmissionRadius = 150;
    std::mt19937 m_rng{std::random_device{}()};
    InterferenceGraph m_interferenceGraph;
    GraphColoring m_graphColoring;
    ColoringAlgorithm m_coloringAlgorithm = ColoringAlgorithm::Greedy;
    bool m_coloringEnabled = false;
    ColoringResult m_coloringResult;
    SimulationStats m_stats;
};
