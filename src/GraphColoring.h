#pragma once

#include <QColor>

#include <cstdint>
#include <vector>
#include <unordered_map>

#include "InterferenceGraph.h"

// Énumération des algorithmes de coloration disponibles
enum class ColoringAlgorithm {
    Greedy,      // Coloration gloutonne naïve
    WelshPowell  // Welsh-Powell : tri par degré puis coloration gloutonne
};

// Résultat d'une coloration de graphe
struct ColoringResult {
    std::vector<int> colors;  // colors[i] = couleur du véhicule i (0, 1, 2, ...)
    int colorCount = 0;        // Nombre total de couleurs utilisées
    double computationTimeMs = 0.0;  // Temps de calcul en millisecondes
};

// Classe pour la coloration de graphes d'interférences (Étape 5)
class GraphColoring {
public:
    GraphColoring();

    // Applique un algorithme de coloration sur le graphe d'interférences
    // vertexCount : nombre de véhicules (sommets)
    // edges : arêtes d'interférences
    // algorithm : algorithme à utiliser
    ColoringResult color(size_t vertexCount,
                         const std::vector<InterferenceEdge>& edges,
                         ColoringAlgorithm algorithm);

    // Palette de couleurs pour l'affichage (génère des couleurs distinctes)
    static QColor colorForIndex(int index, int totalColors);

private:
    // Algorithme gloutonne : parcourt les sommets dans l'ordre, attribue la plus petite couleur disponible
    ColoringResult greedyColoring(size_t vertexCount,
                                  const std::vector<InterferenceEdge>& edges);

    // Algorithme Welsh-Powell : trie les sommets par degré décroissant, puis coloration gloutonne
    ColoringResult welshPowellColoring(size_t vertexCount,
                                       const std::vector<InterferenceEdge>& edges);

    // Construit la liste d'adjacence à partir des arêtes
    std::vector<std::vector<size_t>> buildAdjacency(size_t vertexCount,
                                                     const std::vector<InterferenceEdge>& edges) const;
};
