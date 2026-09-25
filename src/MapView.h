#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent>

class MapView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MapView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void setupTestScene();

    QGraphicsScene *m_scene;
    double m_zoomFactor = 1.15;
};
