#include <QApplication>
#include <QMainWindow>
#include "MapView.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("V2V Simulation - Etape 1 : Visualisation");
    window.resize(1000, 700);

    MapView *mapView = new MapView(&window);
    window.setCentralWidget(mapView);

    window.show();

    return app.exec();
}
