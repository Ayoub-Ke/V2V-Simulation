#include "VehicleSimulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace { using Clock = std::chrono::steady_clock; }

void VehicleSimulation::clear()
{
    m_vehicles.clear();
    m_interferenceGraph.clear();
    m_coloringResult = {};
    m_stats = {};
}

void VehicleSimulation::setGraph(const RoadGraph* graph)
{
    clear();
    m_graph = graph;
    m_bounded = false;
    rebuildSpans();
}

void VehicleSimulation::rebuildSpans()
{
    m_spans.clear();
    m_spawnEdges.clear();
    m_spawnWeights.clear();
    if (!m_graph) return;
    m_spans.resize(m_graph->edgeCount());
    double total = 0;
    for (size_t i = 0; i < m_graph->edgeCount(); ++i) {
        const auto& edge = m_graph->edges()[i];
        if (!std::isfinite(edge.length) || edge.length <= 0) continue;
        const auto a = m_graph->nodes().find(edge.from), b = m_graph->nodes().find(edge.to);
        if (a == m_graph->nodes().end() || b == m_graph->nodes().end()) continue;
        double first = 0, last = 1;
        auto clip = [&](double origin, double delta, double low, double high) {
            if (std::abs(delta) < 1e-12) return origin >= low && origin <= high;
            double p = (low - origin) / delta, q = (high - origin) / delta;
            if (p > q) std::swap(p, q);
            first = std::max(first, p);
            last = std::min(last, q);
            return first < last;
        };
        if (m_bounded && (!clip(a->second.x, b->second.x - a->second.x, m_bounds[0], m_bounds[2]) ||
                          !clip(a->second.y, b->second.y - a->second.y, m_bounds[1], m_bounds[3]))) continue;
        if (last - first < 1e-9) continue;
        m_spans[i] = {first, last};
        m_spawnEdges.push_back(i);
        total += edge.length * (last - first);
        m_spawnWeights.push_back(total);
    }
}

bool VehicleSimulation::setBounds(double minX, double minY, double maxX, double maxY)
{
    if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) ||
        !std::isfinite(maxY) || minX >= maxX || minY >= maxY) return false;
    const auto old = m_bounds;
    const bool wasBounded = m_bounded;
    m_bounds = {minX, minY, maxX, maxY};
    m_bounded = true;
    rebuildSpans();
    if (m_spawnEdges.empty()) {
        m_bounds = old;
        m_bounded = wasBounded;
        rebuildSpans();
        return false;
    }
    spawn(int(m_vehicles.size()));
    return true;
}

void VehicleSimulation::clearBounds()
{
    m_bounded = false;
    spawn(int(m_vehicles.size()));
}

void VehicleSimulation::setSpeedMultiplier(double mult)
{
    if (std::isfinite(mult)) m_speedMultiplier = std::clamp(mult, 0.125, 8.0);
}

void VehicleSimulation::setTransmissionRadius(double meters)
{
    if (!std::isfinite(meters)) return;
    m_transmissionRadius = std::clamp(meters, 1.0, 500.0);
    for (auto& vehicle : m_vehicles) vehicle.transmissionRadius = m_transmissionRadius;
    updateInterferenceGraph();
}

void VehicleSimulation::placeOnRandomEdge(Vehicle& v)
{
    if (!m_graph || m_spawnEdges.empty()) return;
    const double weight = std::uniform_real_distribution<double>(0, m_spawnWeights.back())(m_rng);
    auto it = std::upper_bound(m_spawnWeights.begin(), m_spawnWeights.end(), weight);
    const auto index = std::min(size_t(it - m_spawnWeights.begin()), m_spawnEdges.size() - 1);
    v.edgeIndex = m_spawnEdges[index];
    v.previousNode = m_graph->edges()[v.edgeIndex].from;
    const auto span = m_spans[v.edgeIndex];
    v.progress = std::uniform_real_distribution<double>(span.begin, span.end)(m_rng);
    v.speed = std::uniform_real_distribution<double>(8, 14)(m_rng);
    v.transmissionRadius = m_transmissionRadius;
    updateScreenPosition(v);
}

void VehicleSimulation::spawn(int count)
{
    clear();
    rebuildSpans();
    if (!m_graph || m_spawnEdges.empty() || count <= 0) return;
    m_vehicles.resize(size_t(std::min(count, 5000)));
    for (auto& v : m_vehicles) placeOnRandomEdge(v);
    updateInterferenceGraph();
}

void VehicleSimulation::updateScreenPosition(Vehicle& v) const
{
    if (!m_graph || v.edgeIndex >= m_graph->edgeCount()) return;
    const auto& edge = m_graph->edges()[v.edgeIndex];
    const auto a = m_graph->nodes().find(edge.from), b = m_graph->nodes().find(edge.to);
    if (a == m_graph->nodes().end() || b == m_graph->nodes().end()) return;
    v.x = a->second.x + (b->second.x - a->second.x) * v.progress;
    v.y = a->second.y + (b->second.y - a->second.y) * v.progress;
    v.direction = std::atan2(b->second.y - a->second.y, b->second.x - a->second.x);
}

void VehicleSimulation::advance(Vehicle& v, double distanceMeters)
{
    if (!m_graph || v.edgeIndex >= m_spans.size()) { placeOnRandomEdge(v); return; }
    if (!std::isfinite(v.progress) || !std::isfinite(distanceMeters)) { placeOnRandomEdge(v); return; }
    double remaining = std::max(0.0, distanceMeters);
    for (int safety = 0; remaining > 0 && safety < 100000; ++safety) {
        const auto& edge = m_graph->edges()[v.edgeIndex];
        const auto span = m_spans[v.edgeIndex];
        if (span.end <= span.begin) { placeOnRandomEdge(v); return; }
        v.progress = std::clamp(v.progress, span.begin, span.end);
        const double available = (span.end - v.progress) * edge.length;
        if (remaining < available) { v.progress += remaining / edge.length; break; }
        remaining -= available;
        if (span.end < 1 - 1e-10) { placeOnRandomEdge(v); return; }
        std::vector<size_t> candidates;
        for (auto idx : m_graph->edgesFrom(edge.to))
            if (m_spans[idx].end > m_spans[idx].begin && m_spans[idx].begin <= 1e-10 &&
                m_graph->edges()[idx].to != edge.from) candidates.push_back(idx);
        if (candidates.empty())
            for (auto idx : m_graph->edgesFrom(edge.to))
                if (m_spans[idx].end > m_spans[idx].begin && m_spans[idx].begin <= 1e-10)
                    candidates.push_back(idx);
        if (candidates.empty()) { placeOnRandomEdge(v); return; }
        v.previousNode = edge.to;
        v.edgeIndex = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(m_rng)];
        v.progress = m_spans[v.edgeIndex].begin;
    }
    updateScreenPosition(v);
}

void VehicleSimulation::tick(double dtSeconds)
{
    if (!m_running || !m_graph || !std::isfinite(dtSeconds) || dtSeconds < 0) return;
    const auto start = Clock::now();
    const double dt = std::min(dtSeconds, 10.0) * m_speedMultiplier;
    for (auto& v : m_vehicles) advance(v, v.speed * dt);
    m_stats.elapsedSeconds += dt;
    updateInterferenceGraph();
    m_stats.updateTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void VehicleSimulation::setColoringAlgorithm(ColoringAlgorithm algo)
{
    if (algo != ColoringAlgorithm::Greedy && algo != ColoringAlgorithm::WelshPowell) return;
    m_coloringAlgorithm = algo;
    updateInterferenceGraph();
}

void VehicleSimulation::setColoringEnabled(bool enabled)
{
    m_coloringEnabled = enabled;
    updateInterferenceGraph();
}

void VehicleSimulation::updateInterferenceGraph()
{
    const auto start = Clock::now();
    if (!m_graph || !m_projection) {
        m_interferenceGraph.clear(); m_coloringResult = {}; return;
    }
    double maxRange = 0;
    for (const auto& v : m_vehicles)
        if (std::isfinite(v.transmissionRadius)) maxRange = std::max(maxRange, v.transmissionRadius);
    m_interferenceGraph.build(m_vehicles, m_graph->nodes(), *m_projection, maxRange);
    m_stats.graphTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    m_stats.channelChanges = 0;
    if (m_coloringEnabled) {
        auto result = m_graphColoring.color(m_vehicles.size(), m_interferenceGraph.edges(), m_coloringAlgorithm);
        if (m_coloringResult.colors.size() == result.colors.size())
            for (size_t i = 0; i < result.colors.size(); ++i)
                m_stats.channelChanges += result.colors[i] != m_coloringResult.colors[i];
        m_coloringResult = std::move(result);
    } else m_coloringResult = {};
    m_stats.updateTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

ColoringComparison VehicleSimulation::compareColorings()
{
    ColoringComparison comparison;
    comparison.vehicles = m_vehicles.size();
    comparison.edges = m_interferenceGraph.edgeCount();
    comparison.timeSeconds = m_stats.elapsedSeconds;
    comparison.greedy = m_graphColoring.color(comparison.vehicles, m_interferenceGraph.edges(), ColoringAlgorithm::Greedy);
    comparison.welshPowell = m_graphColoring.color(comparison.vehicles, m_interferenceGraph.edges(), ColoringAlgorithm::WelshPowell);
    comparison.valid = GraphColoring::isValid(comparison.vehicles, m_interferenceGraph.edges(), comparison.greedy) &&
                       GraphColoring::isValid(comparison.vehicles, m_interferenceGraph.edges(), comparison.welshPowell);
    return comparison;
}
