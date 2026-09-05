#pragma once

#include <QObject>
#include <QString>

enum class HotspotState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Failed,
    Unsupported
};

inline QString hotspotStateToString(HotspotState state) {
    switch (state) {
        case HotspotState::Stopped:     return "已停止 (Stopped)";
        case HotspotState::Starting:    return "啟動中 (Starting...)";
        case HotspotState::Running:     return "已啟動 (Running)";
        case HotspotState::Stopping:    return "停止中 (Stopping...)";
        case HotspotState::Failed:      return "啟動失敗 (Failed)";
        case HotspotState::Unsupported: return "不支援 (Unsupported)";
    }
    return "未知 (Unknown)";
}

class IHotspotController : public QObject {
    Q_OBJECT
public:
    explicit IHotspotController(QObject* parent = nullptr) : QObject(parent) {}
    ~IHotspotController() override = default;

    virtual void startHotspot(const QString& ssid, const QString& passphrase) = 0;
    virtual void stopHotspot() = 0;
    virtual HotspotState state() const = 0;
    virtual QString ssid() const = 0;
    virtual int connectedClientCount() const = 0;
    virtual void openWindowsSettings() = 0;

signals:
    void stateChanged(HotspotState newState, const QString& details);
    void clientCountChanged(int count);
    void errorOccurred(const QString& errorMessage);
};
