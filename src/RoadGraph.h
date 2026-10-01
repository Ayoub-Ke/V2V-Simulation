#pragma once

#include "OsmParser.h"
#include "WebMercator.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

// Un nœud du graphe = un point du réseau routier (intersection ou simple point de forme).
// x/y sont des coordonnées d'affichage (pixels de scène, alignées sur les tuiles) : elles ne
// servent qu'au rendu, jamais aux calculs de distance réelle.
struct GraphNode {
    int64_t osmId = 0;
    double x = 0.0;
    double y = 0.0;
};

// Une arête du graphe = un tronçon de route reliant deux nœuds consécutifs d'un même 'way' OSM.
struct GraphEdge {
    int64_t from = 0;
    int64_t to = 0;
    double length = 0.0;     // distance réelle en mètres (calcul GPS, indépendant du zoom d'affichage)
    bool oneway = false;     // vrai : circulable uniquement de 'from' vers 'to'
    int64_t wayId = 0;
};

class RoadGraph {
public:
    // Construit le graphe à partir des routes OSM. 'proj' ne sert qu'à positionner
    // les nœuds à l'écran ; les distances sont calculées indépendamment (GPS réel).
    void build(const OsmData &data, const WebMercatorProjection &proj);

    void clear();

    const std::unordered_map<int64_t, GraphNode> &nodes() const { return m_nodes; }
    const std::vector<GraphEdge> &edges() const { return m_edges; }

    const std::vector<size_t> &edgesFrom(int64_t nodeId) const;

    // Nombre de routes différentes connectées à un nœud : >= 3 => intersection réelle
    int degree(int64_t nodeId) const;

    size_t nodeCount() const { return m_nodes.size(); }
    size_t edgeCount() const { return m_edges.size(); }

private:
    void addEdge(int64_t from, int64_t to, double length, bool oneway, int64_t wayId);

    std::unordered_map<int64_t, GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
    std::unordered_map<int64_t, std::vector<size_t>> m_adjacency;
};
