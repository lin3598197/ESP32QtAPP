#include "MockHotspotController.h"
#include "../common/LogService.h"
#include <QDesktopServices>
#include <QUrl>

MockHotspotController::MockHotspotController(QObject* parent)
    : IHotspotController(parent) {}

void MockHotspotController::startHotspot(const QString& ssid, const QString& passphrase) {
    Q_UNUSED(passphrase);
    m_ssid = ssid.isEmpty() ? "MOCK_HOTSPOT_2G" : ssid;
    m_state = HotspotState::Starting;
    emit stateChanged(m_state, "模擬熱點啟動中...");

    QTimer::singleShot(500, this, [this]() {
        m_state = HotspotState::Running;
        LogService::instance().info("MOCK_HOTSPOT", QString("模擬熱點已啟動: SSID=%1 (2.4GHz)").arg(m_ssid));
        emit stateChanged(m_state, "模擬熱點已啟動 (Running)");
    });
}

void MockHotspotController::stopHotspot() {
    m_state = HotspotState::Stopping;
    emit stateChanged(m_state, "模擬熱點停止中...");

    QTimer::singleShot(300, this, [this]() {
        m_state = HotspotState::Stopped;
        m_clients = 0;
        LogService::instance().info("MOCK_HOTSPOT", "模擬熱點已停止");
        emit stateChanged(m_state, "已停止 (Stopped)");
        emit clientCountChanged(0);
    });
}

HotspotState MockHotspotController::state() const {
    return m_state;
}

QString MockHotspotController::ssid() const {
    return m_ssid;
}

int MockHotspotController::connectedClientCount() const {
    return m_clients;
}

void MockHotspotController::openWindowsSettings() {
    QDesktopServices::openUrl(QUrl("ms-settings:network-mobilehotspot"));
}

void MockHotspotController::setSimulatedClientCount(int count) {
    m_clients = count;
    emit clientCountChanged(count);
}
