#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Un point OSM (coordonnées GPS en degrés)
struct OsmNode {
    double lat = 0.0;
    double lon = 0.0;
};

// Une route OSM : suite ordonnée de noeuds
struct OsmWay {
    int64_t id = 0;
    std::string highway;          // motorway, primary, residential, ...
    bool oneway = false;
    std::vector<int64_t> nodeRefs;
};

// Résultat du chargement d'un fichier .osm
struct OsmData {
    std::unordered_map<int64_t, OsmNode> nodes;
    std::vector<OsmWay> ways;

    double minLat = 0.0, maxLat = 0.0;
    double minLon = 0.0, maxLon = 0.0;
};

// Catégorie visuelle d'une route, du plus important au moins important.
enum class RoadCategory {
    Motorway,
    Primary,
    Secondary,
    Local
};

class OsmParser {
public:
    static bool load(const std::string &path, OsmData &out, std::string &error);
    static bool isDrivableHighway(const std::string &highway);
    static RoadCategory categoryOf(const std::string &highway);
};
