#pragma once

#include <QObject>
#include <memory>
#include <QUuid>
#include "../hotspot/IHotspotController.h"
#include "../network/UdpManager.h"
#include "../devices/DeviceManager.h"

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(bool useMockHotspot = false, QObject* parent = nullptr);
    ~AppController() override;

    void start();
    void shutdown();

    // Hotspot control
    void startHotspot(const QString& ssid, const QString& passphrase);
    void stopHotspot();
    void openWindowsSettings();
    HotspotState hotspotState() const;
    QString hotspotSsid() const;
    int hotspotClientCount() const;

    // UDP & Broadcast Key
    bool broadcastKey(const QString& chunkKey);

    // Device querying
    QList<DeviceRecord> allDevices() const;
    int onlineDeviceCount() const;
    int totalDeviceCount() const;

signals:
    // Forwarded to UI
    void hotspotStateChanged(HotspotState state, const QString& details);
    void hotspotClientCountChanged(int count);
    void deviceListChanged();
    void deviceUpdated(const DeviceRecord& dev);
    void broadcastSummary(const QString& messageId, int totalTargeted, int ackReceived, int ackErrors, int ackTimeouts);

private:
    std::unique_ptr<IHotspotController> m_hotspot;
    std::unique_ptr<UdpManager> m_udp;
    std::unique_ptr<DeviceManager> m_devices;
};
