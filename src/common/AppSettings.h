#pragma once

#include <QString>
#include <QSettings>

class AppSettings {
public:
    static AppSettings& instance();

    // Hotspot settings
    QString ssid() const;
    void setSsid(const QString& ssid);

    QString passphrase() const;
    void setPassphrase(const QString& pass);

    bool autoStartHotspot() const;
    void setAutoStartHotspot(bool autoStart);

    bool autoStopHotspotOnExit() const;
    void setAutoStopHotspotOnExit(bool autoStop);

    // Network settings
    quint16 udpPort() const;
    void setUdpPort(quint16 port);

    QString broadcastAddress() const;
    void setBroadcastAddress(const QString& addr);

    // Timing settings
    int ackTimeoutMs() const;
    void setAckTimeoutMs(int ms);

    int offlineThresholdSec() const;
    void setOfflineThresholdSec(int sec);

    void save();
    void load();

private:
    AppSettings();
    ~AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    QString m_ssid{"ESP32_Host"};
    QString m_passphrase{"12345678"};
    bool m_autoStartHotspot{true};
    bool m_autoStopHotspotOnExit{true};
    quint16 m_udpPort{4210};
    QString m_broadcastAddress{""}; // Empty means auto-detect directed broadcast
    int m_ackTimeoutMs{3000};       // Default 3 seconds as requested
    int m_offlineThresholdSec{20};  // 20 seconds without hello = offline
};
