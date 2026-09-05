#pragma once

#include "IHotspotController.h"
#include <QThreadPool>
#include <QTimer>

class WindowsHotspotController : public IHotspotController {
    Q_OBJECT
public:
    explicit WindowsHotspotController(QObject* parent = nullptr);
    ~WindowsHotspotController() override;

    void startHotspot(const QString& ssid, const QString& passphrase) override;
    void stopHotspot() override;
    HotspotState state() const override;
    QString ssid() const override;
    int connectedClientCount() const override;
    void openWindowsSettings() override;

private slots:
    void pollStatus();

private:
    void updateState(HotspotState newState, const QString& details = QString());

    HotspotState m_state{HotspotState::Stopped};
    QString m_ssid;
    QString m_passphrase;
    int m_clientCount{0};
    QTimer* m_pollTimer{nullptr};
};
