#include "AppSettings.h"
#include <QCoreApplication>

AppSettings& AppSettings::instance() {
    static AppSettings inst;
    return inst;
}

AppSettings::AppSettings() {
    load();
}

QString AppSettings::ssid() const { return m_ssid; }
void AppSettings::setSsid(const QString& ssid) { m_ssid = ssid; }

QString AppSettings::passphrase() const { return m_passphrase; }
void AppSettings::setPassphrase(const QString& pass) { m_passphrase = pass; }

bool AppSettings::autoStartHotspot() const { return m_autoStartHotspot; }
void AppSettings::setAutoStartHotspot(bool autoStart) { m_autoStartHotspot = autoStart; }

bool AppSettings::autoStopHotspotOnExit() const { return m_autoStopHotspotOnExit; }
void AppSettings::setAutoStopHotspotOnExit(bool autoStop) { m_autoStopHotspotOnExit = autoStop; }

quint16 AppSettings::udpPort() const { return m_udpPort; }
void AppSettings::setUdpPort(quint16 port) { m_udpPort = port; }

QString AppSettings::broadcastAddress() const { return m_broadcastAddress; }
void AppSettings::setBroadcastAddress(const QString& addr) { m_broadcastAddress = addr; }

int AppSettings::ackTimeoutMs() const { return m_ackTimeoutMs; }
void AppSettings::setAckTimeoutMs(int ms) { m_ackTimeoutMs = ms; }

int AppSettings::offlineThresholdSec() const { return m_offlineThresholdSec; }
void AppSettings::setOfflineThresholdSec(int sec) { m_offlineThresholdSec = sec; }

void AppSettings::save() {
    QSettings settings("ESP32App", "HotspotController");
    settings.setValue("hotspot/ssid", m_ssid);
    settings.setValue("hotspot/passphrase", m_passphrase);
    settings.setValue("hotspot/autoStart", m_autoStartHotspot);
    settings.setValue("hotspot/autoStopOnExit", m_autoStopHotspotOnExit);
    settings.setValue("network/udpPort", m_udpPort);
    settings.setValue("network/broadcastAddress", m_broadcastAddress);
    settings.setValue("network/ackTimeoutMs", m_ackTimeoutMs);
    settings.setValue("network/offlineThresholdSec", m_offlineThresholdSec);
}

void AppSettings::load() {
    QSettings settings("ESP32App", "HotspotController");
    m_ssid = settings.value("hotspot/ssid", "ESP32_Host").toString();
    m_passphrase = settings.value("hotspot/passphrase", "12345678").toString();
    m_autoStartHotspot = settings.value("hotspot/autoStart", true).toBool();
    m_autoStopHotspotOnExit = settings.value("hotspot/autoStopOnExit", true).toBool();
    m_udpPort = static_cast<quint16>(settings.value("network/udpPort", 4210).toUInt());
    m_broadcastAddress = settings.value("network/broadcastAddress", "").toString();
    m_ackTimeoutMs = settings.value("network/ackTimeoutMs", 3000).toInt();
    m_offlineThresholdSec = settings.value("network/offlineThresholdSec", 20).toInt();
}
