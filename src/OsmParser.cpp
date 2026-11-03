#include "OsmParser.h"

#include <pugixml.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <limits>
#include <unordered_set>

namespace {

    bool coordinate(pugi::xml_attribute attribute, double& value)
    {
        if (!attribute) return false;
        char* end = nullptr;
        errno = 0;
        const char* text = attribute.value();
        value = std::strtod(text, &end);
        return end != text && *end == '\0' && errno != ERANGE && std::isfinite(value);
    }

    const std::unordered_set<std::string> kDrivable = {
        "motorway", "motorway_link", "trunk", "trunk_link",
        "primary", "primary_link", "secondary", "secondary_link",
        "tertiary", "tertiary_link", "unclassified", "residential",
        "living_street", "service"
    };

} // namespace

bool OsmParser::isDrivableHighway(const std::string& highway)
{
    return kDrivable.count(highway) > 0;
}

RoadCategory OsmParser::categoryOf(const std::string& highway)
{
    if (highway == "motorway" || highway == "motorway_link" ||
        highway == "trunk" || highway == "trunk_link")
        return RoadCategory::Motorway;

    if (highway == "primary" || highway == "primary_link")
        return RoadCategory::Primary;

    if (highway == "secondary" || highway == "secondary_link" ||
        highway == "tertiary" || highway == "tertiary_link")
        return RoadCategory::Secondary;

    return RoadCategory::Local;
}

bool OsmParser::load(const std::string& path, OsmData& out, std::string& error)
{
    out = OsmData{};
    error.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(path.c_str());
    if (!result) {
        error = std::string("Lecture du fichier impossible : ") + result.description();
        return false;
    }

    pugi::xml_node root = doc.child("osm");
    if (!root) {
        error = "Fichier invalide : balise <osm> introuvable";
        return false;
    }

    // 1) Tous les noeuds (id -> lat/lon)
    for (pugi::xml_node n : root.children("node")) {
        OsmNode node;
        if (!n.attribute("id") || !coordinate(n.attribute("lat"), node.lat) ||
            !coordinate(n.attribute("lon"), node.lon) ||
            std::abs(node.lat) > 85.05112878 || std::abs(node.lon) > 180) {
            error = "Coordonnées OSM absentes ou hors des limites de la carte Mercator";
            out = OsmData{};
            return false;
        }
        out.nodes.emplace(n.attribute("id").as_llong(), node);
    }

    // 2) Les routes carrossables (ways avec un tag highway=*)
    for (pugi::xml_node w : root.children("way")) {
        OsmWay way;
        way.id = w.attribute("id").as_llong();
        std::string oneway;
        bool roundabout = false;

        for (pugi::xml_node tag : w.children("tag")) {
            const std::string k = tag.attribute("k").as_string();
            const std::string v = tag.attribute("v").as_string();

            if (k == "highway") {
                way.highway = v;
            }
            else if (k == "oneway") {
                oneway = v;
            }
            else if (k == "junction" && v == "roundabout") {
                roundabout = true;
            }
        }

        if (!isDrivableHighway(way.highway))
            continue;

        for (pugi::xml_node nd : w.children("nd"))
            way.nodeRefs.push_back(nd.attribute("ref").as_llong());

        way.oneway = oneway == "yes" || oneway == "true" || oneway == "1" || oneway == "-1"
                     || (oneway.empty() && (roundabout || way.highway == "motorway"));
        if (oneway == "-1") std::reverse(way.nodeRefs.begin(), way.nodeRefs.end());

        if (way.nodeRefs.size() < 2)
            continue;

        bool usable = false;
        for (size_t i = 1; i < way.nodeRefs.size(); ++i) {
            auto a = out.nodes.find(way.nodeRefs[i - 1]);
            auto b = out.nodes.find(way.nodeRefs[i]);
            if (a != out.nodes.end() && b != out.nodes.end() &&
                (a->second.lat != b->second.lat || a->second.lon != b->second.lon)) {
                usable = true;
                break;
            }
        }
        if (usable) out.ways.push_back(std::move(way));
    }

    if (out.ways.empty()) {
        error = "Aucune route carrossable trouvée dans ce fichier";
        return false;
    }

    // 3) Emprise géographique des noeuds réellement utilisés
    out.minLat = out.minLon = std::numeric_limits<double>::max();
    out.maxLat = out.maxLon = std::numeric_limits<double>::lowest();

    for (const OsmWay& way : out.ways) {
        for (int64_t ref : way.nodeRefs) {
            auto it = out.nodes.find(ref);
            if (it == out.nodes.end())
                continue;
            out.minLat = std::min(out.minLat, it->second.lat);
            out.maxLat = std::max(out.maxLat, it->second.lat);
            out.minLon = std::min(out.minLon, it->second.lon);
            out.maxLon = std::max(out.maxLon, it->second.lon);
        }
    }

    return true;
}
