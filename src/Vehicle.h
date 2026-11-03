#pragma once

#include <cstdint>
#include <cstddef>

// Un véhicule circulant sur le graphe routier (Étape 3).
struct Vehicle {
    int64_t previousNode = 0;          // nœud dont il vient sur l'arête actuelle (évite les demi-tours)
    std::size_t edgeIndex = 0;         // index dans RoadGraph::edges()
    double progress = 0.0;             // position sur l'arête actuelle, 0.0 (début) à 1.0 (fin)

    double speed = 12.0;               // vitesse réelle, mètres/seconde
    double direction = 0.0;            // cap actuel, radians (recalculé à chaque déplacement)
    double transmissionRadius = 150.0; // portée V2V, mètres (réutilisé à l'Étape 4)

    double x = 0.0;                    // position écran (coordonnées de scène), mise à jour chaque tick
    double y = 0.0;
};
