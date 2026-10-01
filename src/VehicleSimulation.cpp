#include "VehicleSimulation.h"

#include <algorithm>
#include <cmath>

void VehicleSimulation::placeOnRandomEdge(Vehicle &v)
{
    const auto &edges = m_graph->edges();
    if (edges.empty())
        return;

    std::uniform_int_distribution<size_t> pickEdge(0, edges.size() - 1);
    v.edgeIndex = pickEdge(m_rng);
    v.previousNode = edges[v.edgeIndex].from;

    std::uniform_real_distribution<double> progressDist(0.0, 1.0);
    v.progress = progressDist(m_rng);

    std::uniform_real_distribution<double> speedDist(8.0, 14.0); // ~29 à 50 km/h
    v.speed = speedDist(m_rng);

    updateScreenPosition(v);
}

void VehicleSimulation::spawn(int count)
{
    m_vehicles.clear();
    if (!m_graph || m_graph->edges().empty() || count <= 0)
        return;

    m_vehicles.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Vehicle v;
        placeOnRandomEdge(v);
        m_vehicles.push_back(v);
    }
}

void VehicleSimulation::updateScreenPosition(Vehicle &v) const
{
    const GraphEdge &edge = m_graph->edges()[v.edgeIndex];
    auto itFrom = m_graph->nodes().find(edge.from);
    auto itTo = m_graph->nodes().find(edge.to);
    if (itFrom == m_graph->nodes().end() || itTo == m_graph->nodes().end())
        return;

    const GraphNode &a = itFrom->second;
    const GraphNode &b = itTo->second;

    v.x = a.x + (b.x - a.x) * v.progress;
    v.y = a.y + (b.y - a.y) * v.progress;
    v.direction = std::atan2(b.y - a.y, b.x - a.x);
}

void VehicleSimulation::advance(Vehicle &v, double distanceMeters)
{
    const GraphEdge *edge = &m_graph->edges()[v.edgeIndex];
    double remaining = distanceMeters;

    // Avance le long de l'arête courante ; si la distance dépasse ce qu'il reste sur l'arête,
    // passe à l'arête suivante (choisie à l'intersection) et continue avec la distance restante.
    int safety = 0; // évite une boucle infinie sur un micro-graphe dégénéré
    while (remaining > 0.0 && safety < 8) {
        ++safety;
        const double edgeLen = std::max(edge->length, 0.1);
        const double remainingOnEdge = (1.0 - v.progress) * edgeLen;

        if (remaining < remainingOnEdge) {
            v.progress += remaining / edgeLen;
            remaining = 0.0;
        } else {
            remaining -= remainingOnEdge;
            const int64_t arrivedNode = edge->to;

            const auto &nextEdges = m_graph->edgesFrom(arrivedNode);
            if (nextEdges.empty()) {
                // Cul-de-sac (aucune sortie, ex: sens unique qui se termine) : replacé ailleurs
                placeOnRandomEdge(v);
                return;
            }

            // Évite de faire systématiquement demi-tour quand une autre route est possible
            std::vector<size_t> candidates;
            for (size_t idx : nextEdges) {
                if (nextEdges.size() == 1 || m_graph->edges()[idx].to != v.previousNode)
                    candidates.push_back(idx);
            }
            if (candidates.empty())
                candidates = nextEdges;

            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            v.edgeIndex = candidates[pick(m_rng)];
            v.previousNode = arrivedNode;
            v.progress = 0.0;
            edge = &m_graph->edges()[v.edgeIndex];
        }
    }

    updateScreenPosition(v);
}

void VehicleSimulation::tick(double dtSeconds)
{
    if (!m_running || !m_graph)
        return;

    const double dt = dtSeconds * m_speedMultiplier;
    for (Vehicle &v : m_vehicles)
        advance(v, v.speed * dt);
}
