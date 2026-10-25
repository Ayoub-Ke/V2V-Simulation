#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "Vehicle.h"
#include "RoadGraph.h"
#include "WebMercator.h"

// Représente une arête d'interférence entre deux véhicules
struct InterferenceEdge {
    size_t vehicleA;  // Index du premier véhicule dans VehicleSimulation::vehicles()
    size_t vehicleB;  // Index du second véhicule
    double distance;  // Distance réelle en mètres
};

// Graphe dynamique d'interférences (Étape 4)
// Les sommets sont les véhicules, les arêtes indiquent une interférence potentielle
class InterferenceGraph {
public:
    InterferenceGraph();

    void clear();

    // Construit le graphe d'interférences à partir des positions des véhicules
    // vehicles : vecteur de véhicules avec leurs positions (x,y en pixels écran)
    // nodePositions : positions GPS réelles des nœuds du graphe routier (pour calcul de distance)
    // projection : projection WebMercator pour convertir pixels <-> GPS
    // maxRange : rayon de transmission en mètres (si distance < maxRange, il y a interférence)
    void build(const std::vector<Vehicle>& vehicles,
               const std::unordered_map<int64_t, GraphNode>& nodePositions,
               const WebMercatorProjection& projection,
               double maxRange);

    // Accès aux arêtes d'interférence
    const std::vector<InterferenceEdge>& edges() const { return m_edges; }

    // Statistiques
    size_t edgeCount() const { return m_edges.size(); }
    size_t vertexCount() const { return m_vertexCount; }

    // Pour optimisation : spatial hashing pour éviter O(n²) naïf
    void setSpatialHashingEnabled(bool enabled) { m_useSpatialHashing = enabled; }

private:
    // Version naïve O(n²) - simple mais lente pour beaucoup de véhicules
    void buildNaive(const std::vector<Vehicle>& vehicles,
                    const std::unordered_map<int64_t, GraphNode>& nodePositions,
                    const WebMercatorProjection& projection,
                    double maxRange);

    // Version optimisée avec spatial hashing O(n) en pratique
    void buildWithSpatialHash(const std::vector<Vehicle>& vehicles,
                              const std::unordered_map<int64_t, GraphNode>& nodePositions,
                              const WebMercatorProjection& projection,
                              double maxRange);

    // Convertit position écran (pixels) en coordonnées GPS, puis calcule distance réelle
    double realDistance(double x1, double y1, double x2, double y2,
                        const WebMercatorProjection& projection) const;

    std::vector<InterferenceEdge> m_edges;
    size_t m_vertexCount = 0;
    bool m_useSpatialHashing = true;
};
