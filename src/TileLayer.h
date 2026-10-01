#pragma once

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QObject>
#include <QString>

#include "WebMercator.h"

class QNetworkAccessManager;

// Télécharge (et met en cache sur disque) les tuiles OSM couvrant une zone géographique,
// et les affiche comme fond de carte dans la scène, alignées avec le rendu vectoriel des routes.
class TileLayer : public QObject
{
    Q_OBJECT

public:
    explicit TileLayer(QGraphicsScene *scene, QObject *parent = nullptr);

    // Charge les tuiles couvrant la zone. Retourne false si la zone est trop grande
    // (trop de tuiles à charger) : le rendu vectoriel reste alors disponible seul.
    bool loadArea(double minLat, double maxLat, double minLon, double maxLon,
                  const WebMercatorProjection &proj);

    void clear();

private:
    void requestTile(int tx, int ty);
    void placeTile(int tx, int ty, const QPixmap &pixmap);
    QString cacheFilePath(int tx, int ty) const;

    QGraphicsScene *m_scene;
    QNetworkAccessManager *m_network;
    WebMercatorProjection m_proj;
    QList<QGraphicsPixmapItem *> m_items;

    static constexpr int kMaxTiles = 300; // garde-fou contre les zones trop grandes
};
