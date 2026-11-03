#pragma once

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QObject>
#include <QString>
#include <QPointer>
#include <QSet>

#include "WebMercator.h"

class QNetworkAccessManager;
class QNetworkReply;

// Télécharge (et met en cache sur disque) les tuiles OSM couvrant une zone géographique,
// et les affiche comme fond de carte dans la scène, alignées avec le rendu vectoriel des routes.
class TileLayer : public QObject
{
    Q_OBJECT

public:
    explicit TileLayer(QGraphicsScene *scene, QObject *parent = nullptr);
    ~TileLayer() override;

    // Charge les tuiles couvrant la zone. Retourne false si la zone est trop grande
    // (trop de tuiles à charger) : le rendu vectoriel reste alors disponible seul.
    bool loadArea(double minLat, double maxLat, double minLon, double maxLon,
                  const WebMercatorProjection &proj);

    void clear();
    void setVisible(bool visible);

private:
    void requestTile(int tx, int ty);
    void placeTile(int tx, int ty, const QPixmap &pixmap);
    QString cacheFilePath(int tx, int ty) const;

    QPointer<QGraphicsScene> m_scene;
    QNetworkAccessManager *m_network;
    WebMercatorProjection m_proj;
    QList<QGraphicsPixmapItem *> m_items;
    QSet<QNetworkReply*> m_pending;
    quint64 m_generation = 0;
    bool m_visible = true;

    static constexpr int kMaxTiles = 300; // garde-fou contre les zones trop grandes
};
