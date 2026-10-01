#include "TileLayer.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QStandardPaths>
#include <QUrl>

#include <cmath>

TileLayer::TileLayer(QGraphicsScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_network(new QNetworkAccessManager(this))
{
}

void TileLayer::clear()
{
    for (QGraphicsPixmapItem *item : m_items) {
        m_scene->removeItem(item);
        delete item;
    }
    m_items.clear();
}

QString TileLayer::cacheFilePath(int tx, int ty) const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                       + QString("/osm-tiles/%1/%2").arg(m_proj.zoom).arg(tx);
    QDir().mkpath(dir);
    return dir + QString("/%1.png").arg(ty);
}

bool TileLayer::loadArea(double minLat, double maxLat, double minLon, double maxLon,
                          const WebMercatorProjection &proj)
{
    clear();
    m_proj = proj;

    double pxTL = 0.0, pyTL = 0.0, pxBR = 0.0, pyBR = 0.0;
    WebMercatorProjection::projectRaw(maxLat, minLon, m_proj.zoom, pxTL, pyTL);
    WebMercatorProjection::projectRaw(minLat, maxLon, m_proj.zoom, pxBR, pyBR);

    const int tileMinX = static_cast<int>(std::floor(pxTL / WebMercatorProjection::kTileSize));
    const int tileMaxX = static_cast<int>(std::floor(pxBR / WebMercatorProjection::kTileSize));
    const int tileMinY = static_cast<int>(std::floor(pyTL / WebMercatorProjection::kTileSize));
    const int tileMaxY = static_cast<int>(std::floor(pyBR / WebMercatorProjection::kTileSize));

    const int tileCount = (tileMaxX - tileMinX + 1) * (tileMaxY - tileMinY + 1);
    if (tileCount > kMaxTiles || tileCount <= 0)
        return false;

    for (int tx = tileMinX; tx <= tileMaxX; ++tx) {
        for (int ty = tileMinY; ty <= tileMaxY; ++ty)
            requestTile(tx, ty);
    }

    return true;
}

void TileLayer::requestTile(int tx, int ty)
{
    const QString cachePath = cacheFilePath(tx, ty);

    if (QFile::exists(cachePath)) {
        QPixmap pixmap(cachePath);
        if (!pixmap.isNull()) {
            placeTile(tx, ty, pixmap);
            return;
        }
    }

    const QString url = QString("https://tile.openstreetmap.org/%1/%2/%3.png")
                             .arg(m_proj.zoom).arg(tx).arg(ty);

    QNetworkRequest request{QUrl(url)};
    // Obligatoire : le serveur de tuiles OSM rejette les requêtes sans User-Agent identifiable.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                       QByteArray("V2VSimulation-UHA-ProjetEtudiant/1.0"));

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tx, ty, cachePath]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QByteArray data = reply->readAll();
        QPixmap pixmap;
        if (!pixmap.loadFromData(data))
            return;

        QFile file(cachePath);
        if (file.open(QIODevice::WriteOnly))
            file.write(data);

        placeTile(tx, ty, pixmap);
    });
}

void TileLayer::placeTile(int tx, int ty, const QPixmap &pixmap)
{
    const double worldX = tx * WebMercatorProjection::kTileSize;
    const double worldY = ty * WebMercatorProjection::kTileSize;
    const double sceneX = worldX - m_proj.originPx;
    const double sceneY = worldY - m_proj.originPy;

    QGraphicsPixmapItem *item = m_scene->addPixmap(pixmap);
    item->setPos(sceneX, sceneY);
    item->setZValue(-100); // toujours derrière les routes/graphe
    item->setTransformationMode(Qt::SmoothTransformation);
    m_items.append(item);
}
