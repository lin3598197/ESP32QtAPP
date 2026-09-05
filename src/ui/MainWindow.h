#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QTimer>
#include "../devices/DeviceRecord.h"
#include "../hotspot/IHotspotController.h"
#include "../common/LogService.h"

class AppController;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppController* controller, QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    // Slot handlers for controller events
    void onHotspotStateChanged(HotspotState state, const QString& details);
    void onHotspotClientCountChanged(int count);
    void onDeviceListChanged();
    void onDeviceUpdated(const DeviceRecord& dev);
    void onBroadcastSummary(const QString& messageId, int totalTargeted, int ackReceived, int ackErrors, int ackTimeouts);
    void onLogAppended(LogLevel level, const QString& timestamp, const QString& module, const QString& message);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onToggleHotspotClicked();
    void onOpenSettingsClicked();
    void onBroadcastKeyClicked();
    void onClearLogClicked();
    void onTogglePasswordVisibility(bool checked);

private:
    void setupUi();
    void setupStyles();
    void updateDeviceTableRow(int row, const DeviceRecord& dev);
    QWidget* createStatusBadgeWidget(DeviceStatus status);

    AppController* m_controller{nullptr};

    // Hotspot UI Controls
    QLabel* m_lblHotspotStatusBadge{nullptr};
    QLabel* m_lblHotspotClients{nullptr};
    QLineEdit* m_txtSsid{nullptr};
    QLineEdit* m_txtPassphrase{nullptr};
    QPushButton* m_btnToggleHotspot{nullptr};
    QPushButton* m_btnOpenSettings{nullptr};
    QCheckBox* m_chkAutoStopHotspot{nullptr};

    // Broadcast Key Controls
    QLineEdit* m_txtChunkKey{nullptr};
    QPushButton* m_btnBroadcastKey{nullptr};
    QLabel* m_lblBroadcastSummary{nullptr};

    // Device Table
    QTableWidget* m_deviceTable{nullptr};
    QLabel* m_lblDeviceStats{nullptr};

    // Logs
    QTextEdit* m_txtLogConsole{nullptr};
    QPushButton* m_btnClearLog{nullptr};
};
