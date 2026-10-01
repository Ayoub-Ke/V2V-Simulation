#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "MapView.h"

#ifndef DATA_DIR
#define DATA_DIR "."
#endif

namespace {

    QWidget* buildLegend(QWidget* parent)
    {
        struct Entry { QColor color; QString label; };
        const Entry entries[] = {
            { QColor(232, 90, 90),  "Axe principal" },
            { QColor(250, 166, 68), "Route primaire" },
            { QColor(240, 214, 120),"Route secondaire" },
            { QColor(150, 155, 165),"Rue locale" },
            { QColor(255, 82, 82),  "Intersection" },
        };

        auto* box = new QWidget(parent);
        box->setObjectName("legendBox");
        box->setStyleSheet(
            "#legendBox { background-color: rgba(24, 26, 31, 230); border: 1px solid rgba(255,255,255,20); border-radius: 8px; }"
            "QLabel { color: #dcdce0; font-size: 11px; }");

        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(4);

        for (const Entry& e : entries) {
            auto* row = new QWidget(box);
            auto* rowLayout = new QGridLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setHorizontalSpacing(6);

            auto* swatch = new QLabel(row);
            swatch->setFixedSize(10, 10);
            swatch->setStyleSheet(QString("background-color: %1; border-radius: 5px;")
                .arg(e.color.name()));

            auto* text = new QLabel(e.label, row);

            rowLayout->addWidget(swatch, 0, 0);
            rowLayout->addWidget(text, 0, 1);
            layout->addWidget(row);
        }

        box->adjustSize();
        return box;
    }

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");

    QMainWindow window;
    window.setWindowTitle("V2V Simulation - Etape 1/2 : Visualisation + Graphe routier");
    window.resize(1150, 780);
    window.setStyleSheet(
        "QMainWindow { background-color: #121418; }"

        "QMenuBar { background-color: #181a1f; color: #d6d6da; border-bottom: 1px solid #2a2d35; padding: 2px 4px; }"
        "QMenuBar::item { padding: 5px 12px; background: transparent; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #262931; }"
        "QMenu { background-color: #1c1e24; color: #e0e0e4; border: 1px solid #2f323b; padding: 4px; }"
        "QMenu::item { padding: 6px 20px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #2e4a38; }"

        "QStatusBar { background-color: #181a1f; color: #9a9aa0; border-top: 1px solid #2a2d35; font-size: 12px; }"

        "QToolBar { background-color: #181a1f; border: none; border-bottom: 1px solid #2a2d35; spacing: 2px; padding: 6px 10px; }"
        "QToolBar QLabel { color: #6f7178; font-size: 11px; }"

        "QToolButton { color: #d6d6da; background-color: transparent; border: 1px solid transparent;"
        "              border-radius: 5px; padding: 6px 14px; font-size: 12px; }"
        "QToolButton:hover { background-color: #22252c; border: 1px solid #2f323b; }"
        "QToolButton:pressed { background-color: #1c1e24; }"
        "QToolButton:checked { background-color: #2e4a38; color: #b6f2c8; border: 1px solid #3f6b4d; }");

    MapView* mapView = new MapView(&window);
    window.setCentralWidget(mapView);

    // --- Barre d'outils --------------------------------------------------
    QToolBar* toolbar = window.addToolBar("Navigation");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    QLabel* brand = new QLabel("V2V Simulation", toolbar);
    QFont brandFont = brand->font();
    brandFont.setBold(true);
    brandFont.setPointSize(brandFont.pointSize() + 1);
    brand->setStyleSheet("color: #e8e8ea; padding-right: 16px;");
    brand->setFont(brandFont);
    toolbar->addWidget(brand);

    QAction* zoomOutAction = toolbar->addAction(QString::fromUtf8("\u2212  Zoom"));
    QAction* zoomInAction = toolbar->addAction(QString::fromUtf8("+  Zoom"));

    toolbar->addSeparator();

    QAction* selectZoneAction = toolbar->addAction(QString::fromUtf8("\u25AD  S\u00e9lectionner une zone"));
    selectZoneAction->setCheckable(true);

    QObject::connect(zoomInAction, &QAction::triggered, mapView, &MapView::zoomIn);
    QObject::connect(zoomOutAction, &QAction::triggered, mapView, &MapView::zoomOut);
    QObject::connect(selectZoneAction, &QAction::toggled, mapView, &MapView::setSelectionMode);
    // Le mode se désactive tout seul une fois le rectangle tracé : on désynchronise le bouton
    QObject::connect(mapView, &MapView::selectionModeChanged, selectZoneAction, [selectZoneAction](bool on) {
        if (selectZoneAction->isChecked() != on) {
            selectZoneAction->blockSignals(true);
            selectZoneAction->setChecked(on);
            selectZoneAction->blockSignals(false);
        }
        });

    // --- Barre d'outils : simulation des véhicules (Étape 3) -------------
    window.addToolBarBreak();
    QToolBar* simToolbar = window.addToolBar("Simulation");
    simToolbar->setMovable(false);
    simToolbar->setFloatable(false);

    QLabel* countLabel = new QLabel("Véhicules :", simToolbar);
    simToolbar->addWidget(countLabel);

    QSpinBox* vehicleCountSpin = new QSpinBox(simToolbar);
    vehicleCountSpin->setRange(1, 5000);
    vehicleCountSpin->setValue(500);
    vehicleCountSpin->setSingleStep(100);
    vehicleCountSpin->setStyleSheet(
        "QSpinBox { background-color: #1c1e24; color: #e0e0e4; border: 1px solid #2f323b;"
        "           border-radius: 4px; padding: 4px 6px; }");
    simToolbar->addWidget(vehicleCountSpin);

    QAction* spawnAction = simToolbar->addAction("Générer");
    simToolbar->addSeparator();

    QAction* playPauseAction = simToolbar->addAction(QString::fromUtf8("\u25B6  Démarrer"));
    playPauseAction->setCheckable(true);

    QAction* slowerAction = simToolbar->addAction(QString::fromUtf8("\u00ab  Ralentir"));
    QAction* fasterAction = simToolbar->addAction(QString::fromUtf8("Accélérer  \u00bb"));

    QLabel* speedLabel = new QLabel("1.0x", simToolbar);
    speedLabel->setStyleSheet("color: #9a9aa0; padding: 0 8px;");
    simToolbar->addWidget(speedLabel);

    simToolbar->addSeparator();
    QAction* resetAction = simToolbar->addAction(QString::fromUtf8("\u21BB  Réinitialiser"));

    double speedMultiplier = 1.0;
    auto updateSpeedLabel = [speedLabel, &speedMultiplier]() {
        speedLabel->setText(QString::number(speedMultiplier, 'g', 3) + "x");
        };

    QObject::connect(spawnAction, &QAction::triggered, &window, [mapView, vehicleCountSpin, &window]() {
        mapView->spawnVehicles(vehicleCountSpin->value());
        window.statusBar()->showMessage(
            QString("%1 v\u00e9hicules g\u00e9n\u00e9r\u00e9s sur le r\u00e9seau").arg(mapView->vehicleCount()));
        });

    QObject::connect(playPauseAction, &QAction::toggled, &window,
        [mapView, playPauseAction](bool running) {
            mapView->setSimulationRunning(running);
            playPauseAction->setText(running ? QString::fromUtf8("\u23F8  Pause")
                : QString::fromUtf8("\u25B6  Démarrer"));
        });

    QObject::connect(fasterAction, &QAction::triggered, &window,
        [mapView, &speedMultiplier, updateSpeedLabel]() {
            speedMultiplier = std::min(speedMultiplier * 2.0, 8.0);
            mapView->setSimulationSpeed(speedMultiplier);
            updateSpeedLabel();
        });

    QObject::connect(slowerAction, &QAction::triggered, &window,
        [mapView, &speedMultiplier, updateSpeedLabel]() {
            speedMultiplier = std::max(speedMultiplier / 2.0, 0.125);
            mapView->setSimulationSpeed(speedMultiplier);
            updateSpeedLabel();
        });

    QObject::connect(resetAction, &QAction::triggered, mapView, &MapView::resetSimulation);

    // --- Overlays flottants sur la carte ----------------------------------
    QWidget* legend = buildLegend(mapView);

    QLabel* attribution = new QLabel(QString::fromUtf8("\u00a9 OpenStreetMap contributors"), mapView);
    attribution->setStyleSheet(
        "background-color: rgba(20, 20, 24, 190); color: #b8b8bd; font-size: 10px;"
        "padding: 3px 7px; border-radius: 4px; border: 1px solid rgba(255,255,255,15);");
    attribution->adjustSize();

    auto repositionOverlays = [mapView, legend, attribution]() {
        legend->move(12, mapView->height() - legend->height() - 12);
        attribution->move(mapView->width() - attribution->width() - 10,
            mapView->height() - attribution->height() - 10);
        };
    repositionOverlays();
    legend->show();
    attribution->show();

    // Reposition à chaque redimensionnement de la vue (corrige le chevauchement au démarrage)
    QObject::connect(mapView, &MapView::viewResized, mapView, repositionOverlays);

    QObject::connect(mapView, &MapView::graphBuilt, &window,
        [&window](int nodes, int edges, int intersections) {
            window.statusBar()->showMessage(
                QString("Graphe : %1 noeuds, %2 ar\u00eates, %3 intersections")
                .arg(nodes).arg(edges).arg(intersections));
        });

    QObject::connect(mapView, &MapView::tilesUnavailable, &window, [&window]() {
        window.statusBar()->showMessage(
            "Zone trop grande pour charger le fond de carte (routes affich\u00e9es quand m\u00eame)", 5000);
        });

    QObject::connect(mapView, &MapView::zoneSelected, &window,
        [&window](double minLat, double maxLat, double minLon, double maxLon) {
            window.statusBar()->showMessage(
                QString("Zone de simulation s\u00e9lectionn\u00e9e : [%1, %2] x [%3, %4]")
                .arg(minLat, 0, 'f', 5).arg(maxLat, 0, 'f', 5)
                .arg(minLon, 0, 'f', 5).arg(maxLon, 0, 'f', 5));
        });

    QAction* openAction = window.menuBar()->addMenu("Fichier")->addAction("Ouvrir un fichier OSM...");
    openAction->setShortcut(QKeySequence::Open);
    QObject::connect(openAction, &QAction::triggered, &window, [&window, mapView]() {
        const QString path = QFileDialog::getOpenFileName(
            &window, "Ouvrir un fichier OSM", DATA_DIR,
            "Fichiers OSM (*.osm);;Tous les fichiers (*)");
        if (path.isEmpty())
            return;

        QString error;
        if (!mapView->loadOsmFile(path, &error))
            QMessageBox::warning(&window, "Erreur de chargement", error);
        });

    window.show();

    const QString defaultFile = QString(DATA_DIR) + "/mulhouse.osm";
    if (QFileInfo::exists(defaultFile)) {
        QString error;
        if (!mapView->loadOsmFile(defaultFile, &error))
            QMessageBox::warning(&window, "Erreur de chargement", error);
    }
    else {
        window.statusBar()->showMessage("Aucune carte charg\u00e9e : Fichier > Ouvrir un fichier OSM...");
    }

    return app.exec();
}