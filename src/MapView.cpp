#include "MapView.h"
#include <QPainter>

MapView::MapView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    // Pan : clic-glisser pour déplacer la carte
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    setupTestScene();
}

void MapView::wheelEvent(QWheelEvent *event)
{
    // Zoom / dézoom à la molette, centré sous le curseur
    if (event->angleDelta().y() > 0) {
        scale(m_zoomFactor, m_zoomFactor);
    } else {
        scale(1.0 / m_zoomFactor, 1.0 / m_zoomFactor);
    }
}

void MapView::setupTestScene()
{
    // Placeholder : quelques segments pour simuler un réseau routier.
    // À remplacer par les routes chargées depuis les données OSM.
    m_scene->addLine(0, 0, 200, 0, QPen(Qt::black, 2));
    m_scene->addLine(200, 0, 200, 150, QPen(Qt::black, 2));
    m_scene->addLine(200, 150, 400, 150, QPen(Qt::black, 2));
    m_scene->addLine(0, 0, 0, 300, QPen(Qt::black, 2));

    m_scene->setSceneRect(-50, -50, 600, 500);
}
