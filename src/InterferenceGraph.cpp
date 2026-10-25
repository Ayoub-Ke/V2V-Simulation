#include "InterferenceGraph.h"
#include "Vehicle.h"
#include "RoadGraph.h"
#include "WebMercator.h"

#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>

namespace {

// Distance réelle entre deux points GPS (formule de haversine), en mètres
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

InterferenceGraph::InterferenceGraph()
{
}

void InterferenceGraph::clear()
{
    m_edges.clear();
    m_vertexCount = 0;
}

void InterferenceGraph::build(const std::vector<Vehicle>& vehicles,
                              const std::unordered_map<int64_t, GraphNode>& nodePositions,
                              const WebMercatorProjection& projection,
                              double maxRange)
{
    clear();
    m_vertexCount = vehicles.size();

    if (vehicles.empty() || maxRange <= 0.0)
        return;

    if (m_useSpatialHashing && vehicles.size() > 100) {
        buildWithSpatialHash(vehicles, nodePositions, projection, maxRange);
    } else {
        buildNaive(vehicles, nodePositions, projection, maxRange);
    }
}

void InterferenceGraph::buildNaive(const std::vector<Vehicle>& vehicles,
                                   const std::unordered_map<int64_t, GraphNode>& nodePositions,
                                   const WebMercatorProjection& projection,
                                   double maxRange)
{
    const size_t n = vehicles.size();

    // O(n²) : compare chaque paire de véhicules
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const double dist = realDistance(vehicles[i].x, vehicles[i].y,
                                             vehicles[j].x, vehicles[j].y,
                                             projection);

            if (dist < maxRange) {
                InterferenceEdge edge;
                edge.vehicleA = i;
                edge.vehicleB = j;
                edge.distance = dist;
                m_edges.push_back(edge);
            }
        }
    }
}

void InterferenceGraph::buildWithSpatialHash(const std::vector<Vehicle>& vehicles,
                                             const std::unordered_map<int64_t, GraphNode>& nodePositions,
                                             const WebMercatorProjection& projection,
                                             double maxRange)
{
    // Spatial hashing : divise l'espace en cellules de taille maxRange
    // Seuls les véhicules dans des cellules adjacentes sont comparés

    // Estimer la taille de cellule en pixels écran
    // On utilise une approximation : maxRange mètres ≈ maxRange pixels à un zoom moyen
    // Pour plus de précision, on pourrait convertir en utilisant la projection
    const double cellSize = maxRange * 2.0; // marge de sécurité

    // Trouver les bornes de la zone
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    for (const Vehicle& v : vehicles) {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
        maxX = std::max(maxX, v.x);
        maxY = std::max(maxY, v.y);
    }

    // Créer la grille de hachage (std::map au lieu de unordered_map pour éviter le besoin de hash personnalisé)
    using CellKey = std::pair<int, int>;
    std::map<CellKey, std::vector<size_t>> grid;

    auto cellKey = [&](double x, double y) -> CellKey {
        return CellKey(static_cast<int>(std::floor(x / cellSize)),
                       static_cast<int>(std::floor(y / cellSize)));
    };

    // Placer chaque véhicule dans sa cellule
    for (size_t i = 0; i < vehicles.size(); ++i) {
        const Vehicle& v = vehicles[i];
        CellKey key = cellKey(v.x, v.y);
        grid[key].push_back(i);
    }

    // Comparer uniquement les véhicules dans des cellules adjacentes
    const int neighborOffsets[9][2] = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1},
        {-1, 0}, {0, -1}, {-1, -1}, {1, -1}, {-1, 1}
    };

    for (const auto& cellPair : grid) {
        const CellKey& key = cellPair.first;
        const std::vector<size_t>& cellVehicles = cellPair.second;

        // Pour chaque cellule voisine
        for (int offset = 0; offset < 9; ++offset) {
            CellKey neighborKey(key.first + neighborOffsets[offset][0],
                                key.second + neighborOffsets[offset][1]);

            auto it = grid.find(neighborKey);
            if (it == grid.end())
                continue;

            const std::vector<size_t>& neighborVehicles = it->second;

            // Comparer les véhicules
            for (size_t i : cellVehicles) {
                for (size_t j : neighborVehicles) {
                    // Éviter les doublons et l'auto-comparaison
                    if (i >= j)
                        continue;

                    const double dist = realDistance(vehicles[i].x, vehicles[i].y,
                                                     vehicles[j].x, vehicles[j].y,
                                                     projection);

                    if (dist < maxRange) {
                        InterferenceEdge edge;
                        edge.vehicleA = i;
                        edge.vehicleB = j;
                        edge.distance = dist;
                        m_edges.push_back(edge);
                    }
                }
            }
        }
    }
}

double InterferenceGraph::realDistance(double x1, double y1, double x2, double y2,
                                       const WebMercatorProjection& projection) const
{
    // Convertir les positions écran en GPS
    double lat1 = 0.0, lon1 = 0.0, lat2 = 0.0, lon2 = 0.0;
    projection.unproject(x1, y1, lat1, lon1);
    projection.unproject(x2, y2, lat2, lon2);

    // Calculer la distance réelle
    return haversineMeters(lat1, lon1, lat2, lon2);
}
