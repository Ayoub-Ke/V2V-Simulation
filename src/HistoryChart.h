#pragma once
#include <QWidget>
class MapView;

class HistoryChart : public QWidget {
public:
    explicit HistoryChart(const MapView* view, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    const MapView* m_view;
};
