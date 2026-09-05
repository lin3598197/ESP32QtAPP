#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include "app/AppController.h"
#include "ui/MainWindow.h"
#include "common/LogService.h"

int main(int argc, char *argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ESP32QtApp");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ESP32Host");

    QCommandLineParser parser;
    parser.setApplicationDescription("ESP32 Windows Hotspot & Key Broadcast Manager");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption mockOption(QStringList() << "m" << "mock", "Run with mock hotspot controller for testing without Wi-Fi adapter");
    parser.addOption(mockOption);
    parser.process(app);

    bool useMock = parser.isSet(mockOption);

    LogService::instance().info("MAIN", "=== ESP32 Host Application Starting ===");

    AppController controller(useMock);
    MainWindow mainWindow(&controller);

    // Connect controller signals to UI
    QObject::connect(&controller, &AppController::hotspotStateChanged, &mainWindow, &MainWindow::onHotspotStateChanged);
    QObject::connect(&controller, &AppController::hotspotClientCountChanged, &mainWindow, &MainWindow::onHotspotClientCountChanged);
    QObject::connect(&controller, &AppController::deviceListChanged, &mainWindow, &MainWindow::onDeviceListChanged);
    QObject::connect(&controller, &AppController::deviceUpdated, &mainWindow, &MainWindow::onDeviceUpdated);
    QObject::connect(&controller, &AppController::broadcastSummary, &mainWindow, &MainWindow::onBroadcastSummary);

    mainWindow.show();
    mainWindow.raise();
    mainWindow.activateWindow();

    // Start controller core (binds UDP socket, restores device store, auto-starts hotspot)
    controller.start();

    return app.exec();
}
