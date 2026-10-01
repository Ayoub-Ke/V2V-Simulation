#include "RoadGraph.h"

#include <algorithm>
#include <cmath>

namespace {

const std::vector<size_t> kEmpty;

// Distance réelle entre deux points GPS (formule de haversine), en mètres.
// Indépendante de la projection d'affichage : reste correcte quel que soit le zoom choisi.
double haversineMeters(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kEarthRadius = 6371000.0;
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

    const double dLat = (lat2 - lat1) * kDegToRad;
    const double dLon = (lon2 - lon1) * kDegToRad;

    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                    + std::cos(lat1 * kDegToRad) * std::cos(lat2 * kDegToRad)
                    * std::sin(dLon / 2) * std::sin(dLon / 2);
    const double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return kEarthRadius * c;
}

} // namespace

void RoadGraph::clear()
{
    m_nodes.clear();
    m_edges.clear();
    m_adjacency.clear();
}

void RoadGraph::addEdge(int64_t from, int64_t to, double length, bool oneway, int64_t wayId)
{
    GraphEdge edge;
    edge.from = from;
    edge.to = to;
    edge.length = length;
    edge.oneway = oneway;
    edge.wayId = wayId;

    m_edges.push_back(edge);
    m_adjacency[from].push_back(m_edges.size() - 1);

    if (!oneway) {
        GraphEdge reverse;
        reverse.from = to;
        reverse.to = from;
        reverse.length = length;
        reverse.oneway = oneway;
        reverse.wayId = wayId;

        m_edges.push_back(reverse);
        m_adjacency[to].push_back(m_edges.size() - 1);
    }
}

void RoadGraph::build(const OsmData &data, const WebMercatorProjection &proj)
{
    clear();

    // 1) Nœuds du graphe : position d'affichage (écran) uniquement
    for (const OsmWay &way : data.ways) {
        for (int64_t ref : way.nodeRefs) {
            if (m_nodes.count(ref))
                continue;

            auto it = data.nodes.find(ref);
            if (it == data.nodes.end())
                continue;

            GraphNode node;
            node.osmId = ref;
            proj.project(it->second.lat, it->second.lon, node.x, node.y);
            m_nodes.emplace(ref, node);
        }
    }

    // 2) Arêtes : une par segment consécutif, distance réelle calculée depuis les coordonnées GPS
    for (const OsmWay &way : data.ways) {
        for (size_t i = 0; i + 1 < way.nodeRefs.size(); ++i) {
            int64_t a = way.nodeRefs[i];
            int64_t b = way.nodeRefs[i + 1];

            auto itA = data.nodes.find(a);
            auto itB = data.nodes.find(b);
            if (itA == data.nodes.end() || itB == data.nodes.end())
                continue;

            const double length = haversineMeters(itA->second.lat, itA->second.lon,
                                                    itB->second.lat, itB->second.lon);

            addEdge(a, b, length, way.oneway, way.id);
        }
    }
}

const std::vector<size_t> &RoadGraph::edgesFrom(int64_t nodeId) const
{
    auto it = m_adjacency.find(nodeId);
    return it != m_adjacency.end() ? it->second : kEmpty;
}

int RoadGraph::degree(int64_t nodeId) const
{
    std::vector<int64_t> wayIds;

    for (const GraphEdge &e : m_edges) {
        if (e.from == nodeId || e.to == nodeId)
            wayIds.push_back(e.wayId);
    }

    std::sort(wayIds.begin(), wayIds.end());
    wayIds.erase(std::unique(wayIds.begin(), wayIds.end()), wayIds.end());
    return static_cast<int>(wayIds.size());
}
