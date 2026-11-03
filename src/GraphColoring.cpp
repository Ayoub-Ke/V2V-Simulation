#include "GraphColoring.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

GraphColoring::GraphColoring() = default;

std::vector<std::vector<size_t>> GraphColoring::buildAdjacency(
    size_t count, const std::vector<InterferenceEdge>& edges) const
{
    std::vector<std::vector<size_t>> adjacency(count);
    std::vector<size_t> degree(count, 0);
    for (const auto& e : edges)
        if (e.vehicleA < count && e.vehicleB < count && e.vehicleA != e.vehicleB) {
            ++degree[e.vehicleA];
            ++degree[e.vehicleB];
        }
    for (size_t i = 0; i < count; ++i) adjacency[i].reserve(degree[i]);
    for (const auto& e : edges)
        if (e.vehicleA < count && e.vehicleB < count && e.vehicleA != e.vehicleB) {
            adjacency[e.vehicleA].push_back(e.vehicleB);
            adjacency[e.vehicleB].push_back(e.vehicleA);
        }
    return adjacency;
}

ColoringResult GraphColoring::color(size_t count, const std::vector<InterferenceEdge>& edges,
                                    ColoringAlgorithm algorithm)
{
    const auto start = std::chrono::steady_clock::now();
    ColoringResult result;
    result.colors.assign(count, -1);
    const auto adjacency = buildAdjacency(count, edges);
    std::vector<size_t> order(count);
    std::iota(order.begin(), order.end(), 0);
    if (algorithm == ColoringAlgorithm::WelshPowell)
        std::sort(order.begin(), order.end(), [&adjacency](size_t a, size_t b) {
            if (adjacency[a].size() != adjacency[b].size())
                return adjacency[a].size() > adjacency[b].size();
            return a < b; // Deterministic ties avoid gratuitous channel changes.
        });
    // Timestamp marks avoid allocating a hash table for every vertex.
    std::vector<size_t> marks(count, count);
    for (size_t v : order) {
        for (size_t neighbor : adjacency[v]) {
            const int c = result.colors[neighbor];
            if (c >= 0) marks[size_t(c)] = v;
        }
        size_t c = 0;
        while (c < count && marks[c] == v) ++c;
        result.colors[v] = int(c);
        result.colorCount = std::max(result.colorCount, int(c) + 1);
    }
    result.computationTimeMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return result;
}

bool GraphColoring::isValid(size_t count, const std::vector<InterferenceEdge>& edges,
                            const ColoringResult& result)
{
    if (result.colors.size() != count) return false;
    if (!count) return result.colorCount == 0 && edges.empty();
    for (int color : result.colors)
        if (color < 0 || color >= result.colorCount) return false;
    for (const auto& e : edges)
        if (e.vehicleA >= count || e.vehicleB >= count ||
            result.colors[e.vehicleA] == result.colors[e.vehicleB]) return false;
    return true;
}

QColor GraphColoring::colorForIndex(int index, int totalColors)
{
    if (index < 0 || totalColors <= 0) return QColor("#8b9aab");
    // A channel keeps the same display color when the total channel count changes.
    const double hue = std::fmod(0.48 + index * 0.618033988749895, 1.0);
    return QColor::fromHsvF(hue, 0.72, 0.78);
}
