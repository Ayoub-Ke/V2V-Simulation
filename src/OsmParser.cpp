#include "OsmParser.h"

#include <pugixml.hpp>

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace {

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
        node.lat = n.attribute("lat").as_double();
        node.lon = n.attribute("lon").as_double();
        out.nodes.emplace(n.attribute("id").as_llong(), node);
    }

    // 2) Les routes carrossables (ways avec un tag highway=*)
    for (pugi::xml_node w : root.children("way")) {
        OsmWay way;
        way.id = w.attribute("id").as_llong();

        for (pugi::xml_node tag : w.children("tag")) {
            const std::string k = tag.attribute("k").as_string();
            const std::string v = tag.attribute("v").as_string();

            if (k == "highway") {
                way.highway = v;
            }
            else if (k == "oneway") {
                way.oneway = (v == "yes" || v == "true" || v == "1");
            }
            else if (k == "junction" && v == "roundabout") {
                way.oneway = true;
            }
        }

        if (!isDrivableHighway(way.highway))
            continue;

        for (pugi::xml_node nd : w.children("nd"))
            way.nodeRefs.push_back(nd.attribute("ref").as_llong());

        if (way.nodeRefs.size() < 2)
            continue;

        out.ways.push_back(std::move(way));
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