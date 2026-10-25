#include "GraphColoring.h"

#include <QColor>

#include <algorithm>
#include <chrono>
#include <unordered_set>

GraphColoring::GraphColoring()
{
}

ColoringResult GraphColoring::color(size_t vertexCount,
                                     const std::vector<InterferenceEdge>& edges,
                                     ColoringAlgorithm algorithm)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    ColoringResult result;

    if (vertexCount == 0) {
        result.colorCount = 0;
        return result;
    }

    switch (algorithm) {
    case ColoringAlgorithm::Greedy:
        result = greedyColoring(vertexCount, edges);
        break;
    case ColoringAlgorithm::WelshPowell:
        result = welshPowellColoring(vertexCount, edges);
        break;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.computationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
}

std::vector<std::vector<size_t>> GraphColoring::buildAdjacency(
    size_t vertexCount,
    const std::vector<InterferenceEdge>& edges) const
{
    std::vector<std::vector<size_t>> adjacency(vertexCount);

    for (const InterferenceEdge& edge : edges) {
        if (edge.vehicleA < vertexCount && edge.vehicleB < vertexCount) {
            adjacency[edge.vehicleA].push_back(edge.vehicleB);
            adjacency[edge.vehicleB].push_back(edge.vehicleA);
        }
    }

    return adjacency;
}

ColoringResult GraphColoring::greedyColoring(size_t vertexCount,
                                              const std::vector<InterferenceEdge>& edges)
{
    ColoringResult result;
    result.colors.assign(vertexCount, -1);

    auto adjacency = buildAdjacency(vertexCount, edges);

    // Coloration gloutonne : parcourt les sommets dans l'ordre 0, 1, 2, ...
    for (size_t v = 0; v < vertexCount; ++v) {
        // Trouver les couleurs déjà utilisées par les voisins
        std::unordered_set<int> neighborColors;
        for (size_t neighbor : adjacency[v]) {
            if (result.colors[neighbor] != -1) {
                neighborColors.insert(result.colors[neighbor]);
            }
        }

        // Attribuer la plus petite couleur disponible
        int color = 0;
        while (neighborColors.count(color) > 0) {
            ++color;
        }
        result.colors[v] = color;
    }

    // Compter le nombre de couleurs utilisées
    result.colorCount = *std::max_element(result.colors.begin(), result.colors.end()) + 1;

    return result;
}

ColoringResult GraphColoring::welshPowellColoring(size_t vertexCount,
                                                   const std::vector<InterferenceEdge>& edges)
{
    ColoringResult result;
    result.colors.assign(vertexCount, -1);

    auto adjacency = buildAdjacency(vertexCount, edges);

    // Étape 1 : trier les sommets par degré décroissant
    std::vector<size_t> vertices(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        vertices[i] = i;
    }

    std::sort(vertices.begin(), vertices.end(),
        [&adjacency](size_t a, size_t b) {
            return adjacency[a].size() > adjacency[b].size();
        });

    // Étape 2 : coloration gloutonne sur les sommets triés
    for (size_t v : vertices) {
        // Trouver les couleurs déjà utilisées par les voisins
        std::unordered_set<int> neighborColors;
        for (size_t neighbor : adjacency[v]) {
            if (result.colors[neighbor] != -1) {
                neighborColors.insert(result.colors[neighbor]);
            }
        }

        // Attribuer la plus petite couleur disponible
        int color = 0;
        while (neighborColors.count(color) > 0) {
            ++color;
        }
        result.colors[v] = color;
    }

    // Compter le nombre de couleurs utilisées
    result.colorCount = *std::max_element(result.colors.begin(), result.colors.end()) + 1;

    return result;
}

QColor GraphColoring::colorForIndex(int index, int totalColors)
{
    // Génère des couleurs distinctes en utilisant HSV
    // Hue varie de 0 à 360 de manière uniforme
    // Saturation et valeur fixes pour des couleurs vives

    if (totalColors <= 0)
        return QColor(200, 200, 200);

    const double hue = (index * 360.0) / totalColors;
    return QColor::fromHsvF(hue / 360.0, 0.85, 0.9);
}
