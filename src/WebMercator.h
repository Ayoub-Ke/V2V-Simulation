#pragma once

#include <cmath>

// Projection Web Mercator (celle utilisée par les tuiles OSM/Google/Bing).
// zoom fixe + origine locale (pour garder des coordonnées de scène petites et précises
// au lieu de manipuler directement les coordonnées "monde" qui grandissent avec le zoom).
struct WebMercatorProjection {
    static constexpr double kTileSize = 256.0;
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kDegToRad = kPi / 180.0;

    int zoom = 16;
    double originPx = 0.0;
    double originPy = 0.0;

    WebMercatorProjection() = default;

    WebMercatorProjection(int z, double refLat, double refLon) : zoom(z)
    {
        projectRaw(refLat, refLon, zoom, originPx, originPy);
    }

    // Coordonnées GPS -> coordonnées "monde" en pixels, à un zoom donné (avant recentrage)
    static void projectRaw(double lat, double lon, int zoom, double &px, double &py)
    {
        const double mapSize = kTileSize * std::pow(2.0, zoom);
        const double x = (lon + 180.0) / 360.0;
        const double sinLat = std::sin(lat * kDegToRad);
        const double y = 0.5 - std::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * kPi);
        px = x * mapSize;
        py = y * mapSize;
    }

    // Coordonnées GPS -> coordonnées de scène (recentrées autour de l'origine choisie)
    void project(double lat, double lon, double &x, double &y) const
    {
        double px = 0.0, py = 0.0;
        projectRaw(lat, lon, zoom, px, py);
        x = px - originPx;
        y = py - originPy;
    }

    // Coordonnées de scène -> GPS (utile pour convertir une sélection à l'écran en lat/lon)
    void unproject(double sceneX, double sceneY, double &lat, double &lon) const
    {
        const double worldX = sceneX + originPx;
        const double worldY = sceneY + originPy;
        const double mapSize = kTileSize * std::pow(2.0, zoom);

        lon = worldX / mapSize * 360.0 - 180.0;
        const double n = kPi - 2.0 * kPi * worldY / mapSize;
        lat = std::atan(std::sinh(n)) * 180.0 / kPi;
    }
};

// Choisit le niveau de zoom le plus élevé possible tel que la zone géographique
// tienne dans une largeur/hauteur cible en pixels (évite de télécharger un nombre
// de tuiles déraisonnable pour une grande zone).
inline int chooseZoomForBounds(double minLat, double maxLat, double minLon, double maxLon,
                                double targetPixels = 1400.0)
{
    for (int z = 18; z >= 10; --z) {
        double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
        WebMercatorProjection::projectRaw(maxLat, minLon, z, x0, y0);
        WebMercatorProjection::projectRaw(minLat, maxLon, z, x1, y1);

        if (std::abs(x1 - x0) <= targetPixels && std::abs(y1 - y0) <= targetPixels)
            return z;
    }
    return 10;
}
