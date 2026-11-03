#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include "MapView.h"
#include "HistoryChart.h"

#ifndef DATA_DIR
#define DATA_DIR "."
#endif

namespace {
QLabel* label(const QString& text, const char* name = nullptr)
{
    auto* widget = new QLabel(text);
    if (name) widget->setObjectName(name);
    return widget;
}
QPushButton* button(const QString& text, const char* name = nullptr)
{
    auto* widget = new QPushButton(text);
    widget->setCursor(Qt::PointingHandCursor);
    if (name) widget->setObjectName(name);
    return widget;
}
QVBoxLayout* card(QVBoxLayout* parent, const QString& title)
{
    auto* frame = new QFrame;
    frame->setObjectName("controlCard");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);
    layout->addWidget(label(title, "sectionTitle"));
    parent->addWidget(frame);
    return layout;
}
QLabel* metric(QHBoxLayout* row, const QString& title, const QString& note)
{
    auto* frame = new QFrame;
    frame->setObjectName("metricCard");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 13, 18, 13);
    layout->setSpacing(3);
    layout->addWidget(label(title, "metricTitle"));
    auto* value = label("—", "metricValue");
    layout->addWidget(value);
    layout->addWidget(label(note, "mutedSmall"));
    row->addWidget(frame, 1);
    return value;
}
QString dataDirectory()
{
    for (const auto& candidate : {QCoreApplication::applicationDirPath() + "/data",
             QCoreApplication::applicationDirPath() + "/../data", QDir::currentPath() + "/data",
             QString(DATA_DIR)})
        if (QFileInfo::exists(candidate + "/mulhouse.osm")) return candidate;
    return QString(DATA_DIR);
}
const char* style = R"(
QWidget { font-family: "Inter", "Segoe UI", "DejaVu Sans"; font-size: 12px; color: #213948; }
QMainWindow, QWidget#appShell { background: #f3f6f8; }
QFrame#header { background: white; border-bottom: 1px solid #e1e8ed; }
QLabel#brand { color: #0c8877; font-size: 27px; font-weight: 800; letter-spacing: -1px; }
QLabel#brandName { color: #243c4a; font-size: 19px; font-weight: 600; }
QLabel#muted, QLabel#mutedSmall { color: #7c909d; }
QLabel#mutedSmall { font-size: 10px; }
QLabel#sectionTitle { color: #728995; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
QLabel#panelTitle { font-size: 13px; font-weight: 700; color: #213948; }
QFrame#controlCard, QFrame#metricCard, QFrame#mapCard, QFrame#analysisCard {
    background: white; border: 1px solid #e0e8ed; border-radius: 12px;
}
QLabel#metricTitle { color: #728995; font-size: 10px; font-weight: 600; }
QLabel#metricValue { color: #173c48; font-size: 26px; font-weight: 600; }
QLabel#dataset { background: #edf5f4; color: #267b70; padding: 7px 12px; border-radius: 7px; font-size: 11px; }
QLabel#liveBadge { background: #edf3f6; color: #718592; padding: 6px 10px; border-radius: 6px; font-size: 10px; font-weight: 700; }
QPushButton { background: white; border: 1px solid #dbe5eb; border-radius: 7px; padding: 9px 12px; font-weight: 600; }
QPushButton:hover { background: #edf8f5; border-color: #9bcec3; }
QPushButton:pressed { background: #dbeee9; }
QPushButton:disabled { color: #a4b4be; background: #f6f8fa; border-color: #e9eef2; }
QPushButton#primary { color: white; background: #0b8d78; border-color: #0b8d78; }
QPushButton#primary:hover { background: #087b69; }
QPushButton#playButton { color: white; background: #1d3c4a; border: 0; padding: 12px; }
QPushButton#playButton:checked { color: #0a7664; background: #e1f3ed; }
QPushButton#zoneButton:checked { color: #087b69; background: #e1f3ed; border-color: #79c6b4; }
QPushButton#mapButton { padding: 5px 10px; min-height: 22px; }
QSpinBox, QComboBox { background: #f7f9fb; border: 1px solid #dfe8ee; border-radius: 7px; padding: 8px; min-height: 18px; }
QSpinBox:focus, QComboBox:focus { border-color: #61b8a6; }
QComboBox::drop-down { border: 0; width: 24px; }
QComboBox::down-arrow { image: url(:/icons/down.xpm); width: 9px; height: 6px; }
QSpinBox { padding-right: 25px; }
QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 22px; border: 0; }
QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 22px; border: 0; }
QSpinBox::up-arrow { image: url(:/icons/up.xpm); width: 9px; height: 6px; }
QSpinBox::down-arrow { image: url(:/icons/down.xpm); width: 9px; height: 6px; }
QComboBox QAbstractItemView { background: white; selection-background-color: #e0f3ed; selection-color: #174f43; padding: 5px; }
QCheckBox { spacing: 8px; padding: 3px 0; }
QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #c7d7df; border-radius: 4px; background: #f8fafb; }
QCheckBox::indicator:checked { background: #0b8d78; border-color: #0b8d78; image: url(:/icons/check.xpm); }
QScrollArea { background: transparent; border: 0; }
QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
QScrollBar::handle:vertical { background: #cbd9e0; border-radius: 3px; min-height: 24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QTableWidget { background: white; border: 0; gridline-color: #edf2f5; selection-background-color: #edf7f4; }
QHeaderView::section { background: #f7f9fa; border: 0; color: #7b8f9c; font-size: 10px; padding: 7px 2px; }
QTableWidget::item { padding: 5px; }
QStatusBar { background: white; border-top: 1px solid #e1e8ed; color: #7b909d; font-size: 10px; }
QStatusBar::item { border: 0; }
QToolTip { color: #1e3947; background: white; border: 1px solid #dce6ed; padding: 6px; }
)";
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    app.setApplicationName("V2V Studio");
    app.setOrganizationName("UHA");
    app.setStyle("Fusion");
    app.setStyleSheet(QString::fromUtf8(style));
    const auto args = app.arguments();
    const bool offline = args.contains("--offline");
    const int screenshotIndex = args.indexOf("--screenshot");
    const QString screenshotPath = screenshotIndex >= 0 && screenshotIndex + 1 < args.size()
        ? args[screenshotIndex + 1] : QString();

    QMainWindow window;
    window.setWindowTitle("V2V Studio — Simulation & analyse des communications");
    window.resize(args.contains("--compact") ? 1140 : 1440, args.contains("--compact") ? 780 : 900);
    window.setMinimumSize(1120, 740);
    auto* shell = new QWidget;
    shell->setObjectName("appShell");
    window.setCentralWidget(shell);
    auto* root = new QVBoxLayout(shell);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QFrame;
    header->setObjectName("header");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(26, 17, 26, 17);
    headerLayout->setSpacing(14);
    headerLayout->addWidget(label("V2V", "brand"));
    headerLayout->addWidget(label("studio", "brandName"));
    auto* subtitle = label("Simulation de réseaux véhiculaires", "muted");
    headerLayout->addSpacing(12);
    headerLayout->addWidget(subtitle);
    headerLayout->addStretch();
    auto* dataset = label("Aucune carte", "dataset");
    headerLayout->addWidget(dataset);
    auto* open = button("Ouvrir une carte");
    open->setToolTip("Charger un fichier OpenStreetMap · Ctrl+O");
    headerLayout->addWidget(open);
    auto* exportButton = button("Exporter les mesures", "primary");
    exportButton->setToolTip("Historique et comparaison des méthodes au format CSV · Ctrl+S");
    headerLayout->addWidget(exportButton);
    root->addWidget(header);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(20, 20, 20, 18);
    body->setSpacing(20);
    root->addLayout(body, 1);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(280);
    auto* sidebar = new QWidget;
    sidebar->setObjectName("sideShell");
    sidebar->setStyleSheet("QWidget#sideShell { background: transparent; }");
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(0, 0, 4, 0);
    side->setSpacing(14);
    scroll->setWidget(sidebar);
    body->addWidget(scroll);

    auto* simulation = card(side, "01  SIMULATION");
    simulation->addWidget(label("Véhicules sur le réseau", "panelTitle"));
    auto* count = new QSpinBox;
    count->setObjectName("vehicleCount");
    count->setRange(1, 5000); count->setSingleStep(100); count->setValue(2000);
    count->setGroupSeparatorShown(true);
    simulation->addWidget(count);
    auto* generationRow = new QHBoxLayout;
    auto* generate = button("Générer");
    auto* reset = button("Réinitialiser");
    reset->setToolTip("Replacer les véhicules et remettre le temps à zéro · R");
    generationRow->addWidget(generate); generationRow->addWidget(reset);
    simulation->addLayout(generationRow);
    auto* play = button("Démarrer la simulation", "playButton");
    play->setCheckable(true); play->setToolTip("Démarrer ou mettre en pause · Espace");
    simulation->addWidget(play);
    auto* speedRow = new QHBoxLayout;
    speedRow->addWidget(label("Vitesse du temps", "muted"));
    auto* speed = new QComboBox;
    for (double value : {0.125, 0.25, 0.5, 1., 2., 4., 8.})
        speed->addItem(QString::number(value, 'g', 3) + " ×", value);
    speed->setCurrentIndex(3);
    speedRow->addWidget(speed, 1);
    simulation->addLayout(speedRow);

    auto* radio = card(side, "02  ALLOCATION RADIO");
    radio->addWidget(label("Stratégie de coloration", "panelTitle"));
    auto* algorithm = new QComboBox;
    algorithm->addItem("Glouton", int(ColoringAlgorithm::Greedy));
    algorithm->addItem("Welsh–Powell", int(ColoringAlgorithm::WelshPowell));
    algorithm->setCurrentIndex(1);
    radio->addWidget(algorithm);
    auto* coloring = new QCheckBox("Colorer les canaux radio");
    coloring->setChecked(true); radio->addWidget(coloring);
    auto* rangeRow = new QHBoxLayout;
    rangeRow->addWidget(label("Portée V2V", "muted"));
    auto* range = new QSpinBox;
    range->setRange(1, 500); range->setSingleStep(25); range->setSuffix(" m"); range->setValue(150);
    rangeRow->addWidget(range, 1); radio->addLayout(rangeRow);
    auto* radioHint = label("Deux véhicules reliés utilisent des canaux différents.", "mutedSmall");
    radioHint->setWordWrap(true); radio->addWidget(radioHint);

    auto* terrain = card(side, "03  TERRITOIRE & AFFICHAGE");
    auto* select = button("Sélectionner une zone", "zoneButton");
    select->setCheckable(true);
    select->setToolTip("Dessinez un rectangle sur la carte pour limiter les déplacements.");
    terrain->addWidget(select);
    auto* clearZone = button("Revenir au réseau complet");
    terrain->addWidget(clearZone);
    auto* zoneHint = label("Tout le réseau est accessible.", "mutedSmall");
    zoneHint->setWordWrap(true); terrain->addWidget(zoneHint);
    auto* edges = new QCheckBox("Liens d’interférence");
    edges->setToolTip("Afficher toutes les connexions. Pour isoler les liens d’un véhicule, cliquez dessus.");
    auto* intersections = new QCheckBox("Intersections routières");
    auto* basemap = new QCheckBox("Fond OpenStreetMap");
    basemap->setChecked(!offline);
    terrain->addWidget(edges); terrain->addWidget(intersections); terrain->addWidget(basemap);
    auto* help = label("EXPLORER LA CARTE", "sectionTitle");
    side->addWidget(help);
    auto* instructions = label("Glisser pour déplacer · molette pour zoomer. Cliquez sur un véhicule pour voir sa portée et ses connexions.", "mutedSmall");
    instructions->setWordWrap(true); instructions->setMargin(2); side->addWidget(instructions);
    side->addStretch();

    auto* workspace = new QVBoxLayout;
    workspace->setSpacing(14);
    body->addLayout(workspace, 1);
    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(12);
    auto* vehicleMetric = metric(metrics, "VÉHICULES", "Population simulée");
    auto* edgeMetric = metric(metrics, "CONNEXIONS", "Interférences potentielles");
    auto* colorMetric = metric(metrics, "CANAUX RADIO", "Allocation des fréquences");
    auto* timeMetric = metric(metrics, "MISE À JOUR", "Déplacement + graphe + coloration");
    workspace->addLayout(metrics);

    auto* mapCard = new QFrame;
    mapCard->setObjectName("mapCard");
    auto* mapLayout = new QVBoxLayout(mapCard);
    mapLayout->setContentsMargins(1, 1, 1, 1);
    mapLayout->setSpacing(0);
    auto* mapHeader = new QHBoxLayout;
    mapHeader->setContentsMargins(18, 10, 12, 10);
    mapHeader->addWidget(label("Réseau routier", "panelTitle"));
    auto* live = label("EN PAUSE", "liveBadge");
    mapHeader->addWidget(live); mapHeader->addStretch();
    auto* map = new MapView;
    map->setObjectName("mapView");
    auto* zoomOut = button("−", "mapButton");
    auto* zoomIn = button("+", "mapButton");
    auto* fit = button("Cadrer", "mapButton");
    zoomOut->setToolTip("Dézoomer"); zoomIn->setToolTip("Zoomer"); fit->setToolTip("Voir toute la carte · F");
    mapHeader->addWidget(zoomOut); mapHeader->addWidget(zoomIn); mapHeader->addWidget(fit);
    mapLayout->addLayout(mapHeader);
    mapLayout->addWidget(map, 1);
    workspace->addWidget(mapCard, 1);

    auto* analysisRow = new QHBoxLayout;
    analysisRow->setSpacing(14);
    auto* chartCard = new QFrame;
    chartCard->setObjectName("analysisCard"); chartCard->setFixedHeight(194);
    auto* chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(18, 12, 12, 6); chartLayout->setSpacing(3);
    auto* chartTitle = new QHBoxLayout;
    chartTitle->addWidget(label("Évolution des canaux", "panelTitle"));
    chartTitle->addStretch(); chartTitle->addWidget(label("●  Nombre de canaux", "mutedSmall"));
    chartLayout->addLayout(chartTitle);
    auto* stability = label("Aucune réaffectation mesurée", "mutedSmall");
    chartLayout->addWidget(stability);
    auto* chart = new HistoryChart(map);
    chartLayout->addWidget(chart, 1);
    analysisRow->addWidget(chartCard, 1);
    auto* comparisonCard = new QFrame;
    comparisonCard->setObjectName("analysisCard"); comparisonCard->setFixedHeight(194);
    auto* comparisonLayout = new QVBoxLayout(comparisonCard);
    comparisonLayout->setContentsMargins(16, 10, 16, 10); comparisonLayout->setSpacing(5);
    auto* comparisonHeader = new QHBoxLayout;
    comparisonHeader->addWidget(label("Comparer les méthodes", "panelTitle")); comparisonHeader->addStretch();
    auto* compare = button("Comparer", "mapButton"); comparisonHeader->addWidget(compare);
    comparisonLayout->addLayout(comparisonHeader);
    auto* snapshot = label("Même graphe · même instant", "mutedSmall");
    comparisonLayout->addWidget(snapshot);
    auto* table = new QTableWidget(2, 3);
    table->setHorizontalHeaderLabels({"MÉTHODE", "CANAUX", "TEMPS"});
    table->verticalHeader()->hide(); table->setShowGrid(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int row = 0; row < 2; ++row) {
        table->setRowHeight(row, 30);
        for (int col = 0; col < 3; ++col) {
            auto* item = new QTableWidgetItem(col == 0 ? (row == 0 ? "Glouton" : "Welsh–Powell") : "—");
            if (col) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(row, col, item);
        }
    }
    comparisonLayout->addWidget(table, 1);
    analysisRow->addWidget(comparisonCard, 1);
    workspace->addLayout(analysisRow);

    auto* networkStatus = label("Aucune carte chargée", "mutedSmall");
    window.statusBar()->addWidget(networkStatus, 1);
    auto* elapsed = label("00:00 · temps simulé", "mutedSmall");
    elapsed->setMargin(4); window.statusBar()->addPermanentWidget(elapsed);
    auto* credit = label("UHA · Réseaux mobiles", "mutedSmall");
    credit->setMargin(4); window.statusBar()->addPermanentWidget(credit);

    auto refresh = [=]() {
        const auto& stats = map->simulationStats();
        const bool running = map->simulationRunning();
        QSignalBlocker blocker(play);
        play->setChecked(running);
        play->setText(running ? "Mettre en pause" : "Démarrer la simulation");
        play->setEnabled(map->vehicleCount() > 0);
        reset->setEnabled(map->vehicleCount() > 0);
        generate->setEnabled(map->roadGraph().edgeCount() > 0);
        select->setEnabled(map->roadGraph().edgeCount() > 0);
        clearZone->setEnabled(map->hasSelectedZone());
        exportButton->setEnabled(map->vehicleCount() > 0);
        compare->setEnabled(map->vehicleCount() > 0);
        live->setText(running ? "●  EN DIRECT" : "EN PAUSE");
        live->setStyleSheet(running ? "background:#e0f4eb;color:#08765c;" : "");
        vehicleMetric->setText(QLocale().toString(map->vehicleCount()));
        edgeMetric->setText(QLocale().toString(qulonglong(map->interferenceCount())));
        colorMetric->setText(map->simulationColoringEnabled() ? QString::number(map->coloringResult().colorCount) : "—");
        timeMetric->setText(QString::number(stats.updateTimeMs, 'f', 1) + " ms");
        stability->setText(QString("%1 réaffectations au dernier calcul · graphe %2 ms")
            .arg(qulonglong(stats.channelChanges)).arg(stats.graphTimeMs, 0, 'f', 1));
        const int seconds = int(stats.elapsedSeconds);
        elapsed->setText(QString("%1:%2 · temps simulé").arg(seconds / 60, 2, 10, QChar('0'))
            .arg(seconds % 60, 2, 10, QChar('0')));
        zoneHint->setText(map->hasSelectedZone() ? "Zone active : véhicules confinés à la sélection." : "Tout le réseau est accessible.");
        chart->update();
    };
    QElapsedTimer refreshClock; refreshClock.start();
    QObject::connect(map, &MapView::statisticsUpdated, &window, [&]() {
        if (!map->simulationRunning() || refreshClock.elapsed() >= 150) { refresh(); refreshClock.restart(); }
    });
    QObject::connect(map, &MapView::comparisonUpdated, &window, [=]() {
        const auto& result = map->comparison();
        for (int row = 0; row < 2; ++row) {
            const auto& r = row == 0 ? result.greedy : result.welshPowell;
            table->item(row, 1)->setText(result.vehicles ? QString::number(r.colorCount) : "—");
            table->item(row, 2)->setText(result.vehicles ? QString::number(r.computationTimeMs, 'f', 2) + " ms" : "—");
            table->item(row, 1)->setForeground(QColor("#0b8d78"));
        }
        snapshot->setText(result.vehicles ? QString("Instantané à %1 s · %2 véhicules · %3")
            .arg(result.timeSeconds, 0, 'f', 1).arg(qulonglong(result.vehicles))
            .arg(result.valid ? "colorations valides" : "conflit détecté") : "Même graphe · même instant");
    });
    QObject::connect(map, &MapView::statusMessage, &window, [&window](const QString& message) {
        window.statusBar()->showMessage(message, 6500);
    });
    QObject::connect(map, &MapView::graphBuilt, &window, [=](int nodes, int segments, int junctions) {
        networkStatus->setText(QString("%1 nœuds · %2 segments orientés · %3 intersections")
            .arg(QLocale().toString(nodes)).arg(QLocale().toString(segments)).arg(junctions));
    });
    QObject::connect(map, &MapView::tilesUnavailable, &window, [&window]() {
        window.statusBar()->showMessage("Fond de carte indisponible pour cette zone : le réseau routier reste affiché.", 6000);
    });
    QObject::connect(map, &MapView::selectionModeChanged, select, &QPushButton::setChecked);
    QObject::connect(map, &MapView::zoneSelected, &window, [&window](double, double, double, double) {
        window.statusBar()->showMessage("Zone appliquée. Les véhicules restent à l’intérieur de la sélection.", 5000);
    });
    QObject::connect(play, &QPushButton::toggled, map, &MapView::setSimulationRunning);
    QObject::connect(generate, &QPushButton::clicked, &window, [=]() { map->spawnVehicles(count->value()); map->compareColorings(); refresh(); });
    QObject::connect(reset, &QPushButton::clicked, map, &MapView::resetSimulation);
    QObject::connect(speed, QOverload<int>::of(&QComboBox::currentIndexChanged), &window, [=](int) { map->setSimulationSpeed(speed->currentData().toDouble()); });
    QObject::connect(algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), &window, [=](int) {
        map->setSimulationColoringAlgorithm(ColoringAlgorithm(algorithm->currentData().toInt()));
    });
    QObject::connect(coloring, &QCheckBox::toggled, map, &MapView::setSimulationColoringEnabled);
    QObject::connect(range, QOverload<int>::of(&QSpinBox::valueChanged), map, &MapView::setTransmissionRadius);
    QObject::connect(select, &QPushButton::toggled, map, &MapView::setSelectionMode);
    QObject::connect(clearZone, &QPushButton::clicked, map, &MapView::clearSimulationZone);
    QObject::connect(edges, &QCheckBox::toggled, map, &MapView::setShowInterferenceEdges);
    QObject::connect(intersections, &QCheckBox::toggled, map, &MapView::setShowIntersections);
    QObject::connect(basemap, &QCheckBox::toggled, map, &MapView::setShowBasemap);
    QObject::connect(zoomIn, &QPushButton::clicked, map, &MapView::zoomIn);
    QObject::connect(zoomOut, &QPushButton::clicked, map, &MapView::zoomOut);
    QObject::connect(fit, &QPushButton::clicked, map, &MapView::fitMap);
    QObject::connect(compare, &QPushButton::clicked, map, &MapView::compareColorings);

    auto load = [&](const QString& path) {
        QString error;
        if (!map->loadOsmFile(path, &error)) {
            if (!screenshotPath.isEmpty()) { qCritical("OSM: %s", qPrintable(error)); app.exit(2); }
            else QMessageBox::warning(&window, "Carte non chargée", error);
            return;
        }
        dataset->setText(QFileInfo(path).completeBaseName() + " · OSM");
        map->spawnVehicles(count->value());
        map->compareColorings();
        refresh();
    };
    QObject::connect(open, &QPushButton::clicked, &window, [&]() {
        const auto path = QFileDialog::getOpenFileName(&window, "Ouvrir une carte", dataDirectory(), "Données OpenStreetMap (*.osm)");
        if (!path.isEmpty()) load(path);
    });
    QObject::connect(exportButton, &QPushButton::clicked, &window, [&]() {
        const auto path = QFileDialog::getSaveFileName(&window, "Exporter les mesures", "v2v-mesures.csv", "Fichier CSV (*.csv)");
        if (path.isEmpty()) return;
        map->compareColorings();
        QString error;
        if (!map->exportCsv(path, &error)) QMessageBox::warning(&window, "Export impossible", error);
        else window.statusBar()->showMessage("Historique et comparaison exportés : " + path, 6000);
    });
    auto shortcut = [&](const QKeySequence& keys, QPushButton* target) {
        auto* key = new QShortcut(keys, &window);
        QObject::connect(key, &QShortcut::activated, target, &QPushButton::click);
    };
    shortcut(QKeySequence::Open, open); shortcut(QKeySequence::Save, exportButton);
    shortcut(QKeySequence(Qt::Key_Space), play); shortcut(QKeySequence(Qt::Key_R), reset);
    shortcut(QKeySequence(Qt::Key_F), fit);
    map->setShowBasemap(!offline);
    map->setSimulationColoringAlgorithm(ColoringAlgorithm::WelshPowell);
    map->setSimulationColoringEnabled(true);
    refresh();
    window.show();
    QTimer::singleShot(0, &window, [&]() {
        const auto path = dataDirectory() + "/mulhouse.osm";
        if (QFileInfo::exists(path)) load(path);
        else window.statusBar()->showMessage("Ouvrez un fichier OSM pour commencer.");
        if (!screenshotPath.isEmpty()) {
            map->setSimulationRunning(true);
            QTimer::singleShot(1600, &window, [&]() {
                const bool saved = window.grab().save(screenshotPath);
                app.exit(saved ? 0 : 3);
            });
        }
    });
    return app.exec();
}
