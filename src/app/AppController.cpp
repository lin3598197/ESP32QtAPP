#include "AppController.h"
#include "../hotspot/WindowsHotspotController.h"
#include "../hotspot/MockHotspotController.h"
#include "../common/LogService.h"
#include "../common/AppSettings.h"

AppController::AppController(bool useMockHotspot, QObject* parent)
    : QObject(parent) {
    if (useMockHotspot) {
        LogService::instance().info("APP", "Initializing with MockHotspotController");
        m_hotspot = std::make_unique<MockHotspotController>();
    } else {
        LogService::instance().info("APP", "Initializing with WindowsHotspotController (WinRT Tethering / Settings fallback)");
        m_hotspot = std::make_unique<WindowsHotspotController>();
    }

    m_udp = std::make_unique<UdpManager>();
    m_devices = std::make_unique<DeviceManager>();

    // Connect Hotspot signals
    connect(m_hotspot.get(), &IHotspotController::stateChanged, this, &AppController::hotspotStateChanged);
    connect(m_hotspot.get(), &IHotspotController::clientCountChanged, this, &AppController::hotspotClientCountChanged);

    // Connect UDP signals to DeviceManager
    connect(m_udp.get(), &UdpManager::helloReceived, m_devices.get(), &DeviceManager::handleHello);
    connect(m_udp.get(), &UdpManager::ackReceived, m_devices.get(), &DeviceManager::handleAck);

    // Connect DeviceManager signals to UI
    connect(m_devices.get(), &DeviceManager::deviceListChanged, this, &AppController::deviceListChanged);
    connect(m_devices.get(), &DeviceManager::deviceUpdated, this, &AppController::deviceUpdated);
    connect(m_devices.get(), &DeviceManager::broadcastSummary, this, &AppController::broadcastSummary);
}

AppController::~AppController() {
    shutdown();
}

void AppController::start() {
    LogService::instance().info("APP", "Starting ESP32 Host Controller Application...");

    // 1. Initialize Device Store
    m_devices->initialize();

    // 2. Start UDP Socket on configured port
    quint16 port = AppSettings::instance().udpPort();
    m_udp->start(port);

    // 3. Auto-start Windows Hotspot if configured
    if (AppSettings::instance().autoStartHotspot()) {
        QString ssid = AppSettings::instance().ssid();
        QString pass = AppSettings::instance().passphrase();
        LogService::instance().info("APP", QString("Auto-starting hotspot with SSID: %1").arg(ssid));
        startHotspot(ssid, pass);
    }
}

void AppController::shutdown() {
    LogService::instance().info("APP", "Shutting down application...");

    // 1. Stop UDP
    if (m_udp) {
        m_udp->stop();
    }

    // 2. Save Devices
    if (m_devices) {
        m_devices->save();
    }

    // 3. Stop Hotspot if enabled
    if (AppSettings::instance().autoStopHotspotOnExit() && m_hotspot) {
        LogService::instance().info("APP", "Auto-stopping Windows Mobile Hotspot on exit...");
        m_hotspot->stopHotspot();
    }
}

void AppController::startHotspot(const QString& ssid, const QString& passphrase) {
    if (m_hotspot) {
        m_hotspot->startHotspot(ssid, passphrase);
    }
}

void AppController::stopHotspot() {
    if (m_hotspot) {
        m_hotspot->stopHotspot();
    }
}

void AppController::openWindowsSettings() {
    if (m_hotspot) {
        m_hotspot->openWindowsSettings();
    }
}

HotspotState AppController::hotspotState() const {
    return m_hotspot ? m_hotspot->state() : HotspotState::Stopped;
}

QString AppController::hotspotSsid() const {
    return m_hotspot ? m_hotspot->ssid() : QString();
}

int AppController::hotspotClientCount() const {
    return m_hotspot ? m_hotspot->connectedClientCount() : 0;
}

bool AppController::broadcastKey(const QString& chunkKey) {
    if (!m_udp || !m_devices) return false;

    // Generate unique UUID v4 message_id
    QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    LogService::instance().info("APP", QString("Initiating key broadcast: MessageId=%1, Key=%2")
        .arg(messageId, chunkKey));

    // Prepare devices state for broadcast (transitions to WAITING_ACK and arms timeout)
    m_devices->prepareForBroadcast(messageId);

    // Send single broadcast datagram via UDP 4210
    return m_udp->broadcastKey(chunkKey, messageId);
}

QList<DeviceRecord> AppController::allDevices() const {
    return m_devices ? m_devices->allDevices() : QList<DeviceRecord>();
}

int AppController::onlineDeviceCount() const {
    return m_devices ? m_devices->onlineCount() : 0;
}

int AppController::totalDeviceCount() const {
    return m_devices ? m_devices->totalCount() : 0;
}
