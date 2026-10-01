#pragma once

#include "RoadGraph.h"
#include "Vehicle.h"

#include <random>
#include <vector>

// Fait avancer une population de véhicules le long du graphe routier.
// Chaque véhicule, une fois à une intersection, choisit aléatoirement une route de sortie
// (en respectant les sens uniques déjà encodés dans le graphe), en évitant si possible
// de faire immédiatement demi-tour. C'est un flux de circulation continu, pas un calcul
// d'itinéraire A -> B : suffisant pour peupler le réseau et, plus tard, générer le graphe
// d'interférences dynamique (Étape 4).
class VehicleSimulation {
public:
    void setGraph(const RoadGraph *graph) { m_graph = graph; }

    void spawn(int count);
    void clear() { m_vehicles.clear(); }

    // Fait avancer la simulation de dtSeconds (temps réel), multiplié par le facteur de vitesse.
    void tick(double dtSeconds);

    void setRunning(bool running) { m_running = running; }
    bool running() const { return m_running; }

    void setSpeedMultiplier(double mult) { m_speedMultiplier = mult; }
    double speedMultiplier() const { return m_speedMultiplier; }

    std::vector<Vehicle> &vehicles() { return m_vehicles; }
    const std::vector<Vehicle> &vehicles() const { return m_vehicles; }

private:
    void placeOnRandomEdge(Vehicle &v);
    void advance(Vehicle &v, double distanceMeters);
    void updateScreenPosition(Vehicle &v) const;

    const RoadGraph *m_graph = nullptr;
    std::vector<Vehicle> m_vehicles;
    bool m_running = false;
    double m_speedMultiplier = 1.0;
    std::mt19937 m_rng{std::random_device{}()};
};
