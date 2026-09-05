#pragma once

#include "IHotspotController.h"
#include <QTimer>

class MockHotspotController : public IHotspotController {
    Q_OBJECT
public:
    explicit MockHotspotController(QObject* parent = nullptr);

    void startHotspot(const QString& ssid, const QString& passphrase) override;
    void stopHotspot() override;
    HotspotState state() const override;
    QString ssid() const override;
    int connectedClientCount() const override;
    void openWindowsSettings() override;

    // Simulation helpers
    void setSimulatedClientCount(int count);

private:
    HotspotState m_state{HotspotState::Stopped};
    QString m_ssid{"MOCK_HOTSPOT_2G"};
    int m_clients{0};
};
