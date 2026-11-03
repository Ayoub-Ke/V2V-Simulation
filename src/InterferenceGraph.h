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
    // nodePositions : conservé pour compatibilité ; les positions viennent des véhicules
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
    std::vector<InterferenceEdge> m_edges;
    size_t m_vertexCount = 0;
    bool m_useSpatialHashing = true;
};
