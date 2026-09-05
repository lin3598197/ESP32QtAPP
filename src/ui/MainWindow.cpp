#include "MainWindow.h"
#include "../app/AppController.h"
#include "../common/AppSettings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSplitter>
#include <QCloseEvent>
#include <QDateTime>

MainWindow::MainWindow(AppController* controller, QWidget* parent)
    : QMainWindow(parent), m_controller(controller) {
    setupUi();
    setupStyles();

    // Connect LogService to console
    connect(&LogService::instance(), &LogService::logAppended, this, &MainWindow::onLogAppended);

    // Initial state setup
    onHotspotStateChanged(m_controller->hotspotState(), "初始化完成");
    onDeviceListChanged();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    setWindowTitle("ESP32 Windows Hotspot & Key Broadcast Manager");
    resize(1050, 720);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // 1. Hotspot Control Group
    auto* grpHotspot = new QGroupBox("Windows 行動熱點控制 (Mobile Hotspot - 2.4 GHz)", this);
    auto* layHotspot = new QGridLayout(grpHotspot);

    auto* lblStatusPrompt = new QLabel("熱點狀態:", this);
    m_lblHotspotStatusBadge = new QLabel("檢查中...", this);
    m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 3px 8px; border-radius: 4px; background-color: #bdc3c7; color: #2c3e50;");

    m_lblHotspotClients = new QLabel("連線客戶端: 0", this);

    auto* lblSsidPrompt = new QLabel("SSID:", this);
    m_txtSsid = new QLineEdit(AppSettings::instance().ssid(), this);
    m_txtSsid->setPlaceholderText("熱點名稱");

    auto* lblPassPrompt = new QLabel("密碼:", this);
    m_txtPassphrase = new QLineEdit(AppSettings::instance().passphrase(), this);
    m_txtPassphrase->setEchoMode(QLineEdit::Password);
    m_txtPassphrase->setPlaceholderText("至少 8 碼密碼");

    auto* chkShowPass = new QCheckBox("顯示密碼", this);
    connect(chkShowPass, &QCheckBox::toggled, this, &MainWindow::onTogglePasswordVisibility);

    m_btnToggleHotspot = new QPushButton("啟動熱點", this);
    m_btnToggleHotspot->setStyleSheet("font-weight: bold; background-color: #2980b9; color: white; padding: 6px 14px; border-radius: 4px;");
    connect(m_btnToggleHotspot, &QPushButton::clicked, this, &MainWindow::onToggleHotspotClicked);

    m_btnOpenSettings = new QPushButton("開啟 Windows 設定", this);
    connect(m_btnOpenSettings, &QPushButton::clicked, this, &MainWindow::onOpenSettingsClicked);

    m_chkAutoStopHotspot = new QCheckBox("APP 關閉時自動關閉熱點", this);
    m_chkAutoStopHotspot->setChecked(AppSettings::instance().autoStopHotspotOnExit());

    layHotspot->addWidget(lblStatusPrompt, 0, 0);
    layHotspot->addWidget(m_lblHotspotStatusBadge, 0, 1);
    layHotspot->addWidget(m_lblHotspotClients, 0, 2);
    layHotspot->addWidget(m_btnToggleHotspot, 0, 3);
    layHotspot->addWidget(m_btnOpenSettings, 0, 4);

    layHotspot->addWidget(lblSsidPrompt, 1, 0);
    layHotspot->addWidget(m_txtSsid, 1, 1);
    layHotspot->addWidget(lblPassPrompt, 1, 2);
    layHotspot->addWidget(m_txtPassphrase, 1, 3);
    layHotspot->addWidget(chkShowPass, 1, 4);

    layHotspot->addWidget(m_chkAutoStopHotspot, 2, 0, 1, 3);

    mainLayout->addWidget(grpHotspot);

    // 2. Broadcast Key Control Group
    auto* grpBroadcast = new QGroupBox("Key 廣播控制 (UDP Port 4210)", this);
    auto* layBroadcast = new QHBoxLayout(grpBroadcast);

    auto* lblKeyPrompt = new QLabel("Chunk Key:", this);
    lblKeyPrompt->setStyleSheet("font-weight: bold;");

    m_txtChunkKey = new QLineEdit(this);
    m_txtChunkKey->setPlaceholderText("請輸入要廣播給所有 ESP32 的 Key (例如: SECRET_KEY_2026)");

    m_btnBroadcastKey = new QPushButton("廣播 Key (一次)", this);
    m_btnBroadcastKey->setStyleSheet("font-weight: bold; background-color: #27ae60; color: white; padding: 8px 18px; border-radius: 4px; font-size: 13px;");
    connect(m_btnBroadcastKey, &QPushButton::clicked, this, &MainWindow::onBroadcastKeyClicked);

    m_lblBroadcastSummary = new QLabel("待命中", this);
    m_lblBroadcastSummary->setStyleSheet("color: #7f8c8d; font-style: italic;");

    layBroadcast->addWidget(lblKeyPrompt);
    layBroadcast->addWidget(m_txtChunkKey, 3);
    layBroadcast->addWidget(m_btnBroadcastKey, 1);
    layBroadcast->addWidget(m_lblBroadcastSummary, 2);

    mainLayout->addWidget(grpBroadcast);

    // 3. Middle Area: Splitter with Device Table and Logs Console
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // Device Table Panel
    auto* wgtDevice = new QWidget(this);
    auto* layDevice = new QVBoxLayout(wgtDevice);
    layDevice->setContentsMargins(0, 0, 0, 0);

    auto* layDeviceHeader = new QHBoxLayout();
    auto* lblTableTitle = new QLabel("已連線 / 已註冊 ESP32 裝置清單", this);
    lblTableTitle->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_lblDeviceStats = new QLabel("在線: 0 | 總計: 0", this);
    m_lblDeviceStats->setStyleSheet("color: #2980b9; font-weight: bold;");
    layDeviceHeader->addWidget(lblTableTitle);
    layDeviceHeader->addStretch();
    layDeviceHeader->addWidget(m_lblDeviceStats);
    layDevice->addLayout(layDeviceHeader);

    m_deviceTable = new QTableWidget(this);
    m_deviceTable->setColumnCount(7);
    QStringList headers;
    headers << "狀態" << "裝置 ID" << "MAC 位址" << "IP : Port" << "最後活動" << "最後 ACK / 狀態訊息" << "韌體版本";
    m_deviceTable->setHorizontalHeaderLabels(headers);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setAlternatingRowColors(true);
    layDevice->addWidget(m_deviceTable);

    splitter->addWidget(wgtDevice);

    // System Logs Console
    auto* wgtLog = new QWidget(this);
    auto* layLog = new QVBoxLayout(wgtLog);
    layLog->setContentsMargins(0, 0, 0, 0);

    auto* layLogHeader = new QHBoxLayout();
    auto* lblLogTitle = new QLabel("系統事件與 UDP 封包日誌", this);
    lblLogTitle->setStyleSheet("font-weight: bold;");
    m_btnClearLog = new QPushButton("清除日誌", this);
    connect(m_btnClearLog, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    layLogHeader->addWidget(lblLogTitle);
    layLogHeader->addStretch();
    layLogHeader->addWidget(m_btnClearLog);
    layLog->addLayout(layLogHeader);

    m_txtLogConsole = new QTextEdit(this);
    m_txtLogConsole->setReadOnly(true);
    m_txtLogConsole->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: Consolas, monospace; font-size: 11px;");
    layLog->addWidget(m_txtLogConsole);

    splitter->addWidget(wgtLog);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(splitter);
}

void MainWindow::setupStyles() {
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f5f6fa;
            color: #2f3542;
        }
        QWidget {
            color: #2f3542;
            font-size: 13px;
        }
        QLabel {
            color: #2f3542;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #dcdde1;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 12px;
            background-color: #ffffff;
            color: #2f3542;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
            color: #2c3e50;
            font-weight: bold;
        }
        QLineEdit {
            padding: 6px 8px;
            border: 1px solid #bdc3c7;
            border-radius: 4px;
            background-color: #ffffff;
            color: #2f3542;
            selection-background-color: #2980b9;
            selection-color: #ffffff;
        }
        QLineEdit:focus {
            border: 1px solid #3498db;
            color: #2f3542;
        }
        QLineEdit:disabled {
            background-color: #f1f2f6;
            color: #747d8c;
        }
        QCheckBox {
            color: #2f3542;
        }
        QPushButton {
            border-radius: 4px;
        }
        QTableWidget {
            border: 1px solid #dcdde1;
            border-radius: 4px;
            background-color: #ffffff;
            alternate-background-color: #f8f9fa;
            gridline-color: #ecf0f1;
            color: #2f3542;
            selection-background-color: #2980b9;
            selection-color: #ffffff;
        }
        QTableWidget::item {
            color: #2f3542;
            padding: 5px 8px;
        }
        QTableWidget::item:selected {
            background-color: #2980b9;
            color: #ffffff;
        }
        QTableWidget::item:hover:!selected {
            background-color: #edf2f7;
            color: #2f3542;
        }
        QHeaderView::section {
            background-color: #f1f2f6;
            padding: 6px 8px;
            border: none;
            border-bottom: 1px solid #dcdde1;
            border-right: 1px solid #e4e7eb;
            font-weight: bold;
            color: #2f3542;
        }
    )");
}

void MainWindow::onTogglePasswordVisibility(bool checked) {
    m_txtPassphrase->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void MainWindow::onToggleHotspotClicked() {
    if (!m_controller) return;

    HotspotState currState = m_controller->hotspotState();
    if (currState == HotspotState::Running) {
        m_controller->stopHotspot();
    } else {
        QString ssid = m_txtSsid->text().trimmed();
        QString pass = m_txtPassphrase->text();
        if (ssid.isEmpty()) {
            QMessageBox::warning(this, "驗證失敗", "請輸入熱點 SSID！");
            return;
        }
        if (pass.length() < 8) {
            QMessageBox::warning(this, "驗證失敗", "熱點密碼至少需要 8 個字元！");
            return;
        }
        m_controller->startHotspot(ssid, pass);
    }
}

void MainWindow::onOpenSettingsClicked() {
    if (m_controller) {
        m_controller->openWindowsSettings();
    }
}

void MainWindow::onBroadcastKeyClicked() {
    QString key = m_txtChunkKey->text().trimmed();
    if (key.isEmpty()) {
        QMessageBox::warning(this, "輸入錯誤", "請先在輸入框設定欲廣播之 chunk_key！");
        return;
    }

    if (!m_controller) return;

    // Prevent double clicking
    m_btnBroadcastKey->setEnabled(false);
    QTimer::singleShot(2000, this, [this]() {
        m_btnBroadcastKey->setEnabled(true);
    });

    m_lblBroadcastSummary->setText("廣播封包已發送，等待 ACK 回覆...");
    m_controller->broadcastKey(key);
}

void MainWindow::onClearLogClicked() {
    m_txtLogConsole->clear();
}

void MainWindow::onHotspotStateChanged(HotspotState state, const QString& details) {
    Q_UNUSED(details);
    switch (state) {
        case HotspotState::Running:
            m_lblHotspotStatusBadge->setText("已啟動 (2.4GHz)");
            m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 4px 10px; border-radius: 4px; background-color: #27ae60; color: white;");
            m_btnToggleHotspot->setText("停止熱點");
            m_btnToggleHotspot->setStyleSheet("font-weight: bold; background-color: #e74c3c; color: white; padding: 6px 14px; border-radius: 4px;");
            m_txtSsid->setEnabled(false);
            m_txtPassphrase->setEnabled(false);
            break;
        case HotspotState::Starting:
            m_lblHotspotStatusBadge->setText("啟動中...");
            m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 4px 10px; border-radius: 4px; background-color: #f39c12; color: white;");
            m_btnToggleHotspot->setEnabled(false);
            break;
        case HotspotState::Stopping:
            m_lblHotspotStatusBadge->setText("停止中...");
            m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 4px 10px; border-radius: 4px; background-color: #f39c12; color: white;");
            m_btnToggleHotspot->setEnabled(false);
            break;
        case HotspotState::Stopped:
            m_lblHotspotStatusBadge->setText("已停止");
            m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 4px 10px; border-radius: 4px; background-color: #bdc3c7; color: #2c3e50;");
            m_btnToggleHotspot->setText("啟動熱點");
            m_btnToggleHotspot->setStyleSheet("font-weight: bold; background-color: #2980b9; color: white; padding: 6px 14px; border-radius: 4px;");
            m_btnToggleHotspot->setEnabled(true);
            m_txtSsid->setEnabled(true);
            m_txtPassphrase->setEnabled(true);
            break;
        case HotspotState::Failed:
            m_lblHotspotStatusBadge->setText("啟動失敗");
            m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 4px 10px; border-radius: 4px; background-color: #c0392b; color: white;");
            m_btnToggleHotspot->setText("重試啟動");
            m_btnToggleHotspot->setEnabled(true);
            m_txtSsid->setEnabled(true);
            m_txtPassphrase->setEnabled(true);
            break;
        case HotspotState::Unsupported:
            m_lblHotspotStatusBadge->setText("硬體不支援");
            m_lblHotspotStatusBadge->setStyleSheet("font-weight: bold; padding: 4px 10px; border-radius: 4px; background-color: #7f8c8d; color: white;");
            m_btnToggleHotspot->setEnabled(true);
            break;
    }
}

void MainWindow::onHotspotClientCountChanged(int count) {
    m_lblHotspotClients->setText(QString("熱點連線數: %1").arg(count));
}

QWidget* MainWindow::createStatusBadgeWidget(DeviceStatus status) {
    auto* container = new QWidget();
    container->setStyleSheet("background: transparent;");
    auto* lay = new QHBoxLayout(container);
    lay->setContentsMargins(4, 2, 4, 2);

    auto* badge = new QLabel(deviceStatusToString(status));
    badge->setAlignment(Qt::AlignCenter);

    QString colorBg;
    QString colorFg = "white";

    switch (status) {
        case DeviceStatus::Online:
            colorBg = "#27ae60"; // Green
            break;
        case DeviceStatus::WaitingAck:
            colorBg = "#2980b9"; // Blue
            break;
        case DeviceStatus::KeyReceived:
            colorBg = "#16a085"; // Emerald Teal
            break;
        case DeviceStatus::Timeout:
            colorBg = "#e67e22"; // Orange
            break;
        case DeviceStatus::Error:
            colorBg = "#c0392b"; // Red
            break;
        case DeviceStatus::Offline:
            colorBg = "#7f8c8d"; // Gray
            break;
    }

    badge->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; border-radius: 3px; padding: 2px 6px;")
        .arg(colorBg, colorFg));

    lay->addWidget(badge);
    return container;
}

void MainWindow::updateDeviceTableRow(int row, const DeviceRecord& dev) {
    m_deviceTable->setCellWidget(row, 0, createStatusBadgeWidget(dev.status));
    m_deviceTable->setItem(row, 1, new QTableWidgetItem(dev.id));
    m_deviceTable->setItem(row, 2, new QTableWidgetItem(dev.mac));
    m_deviceTable->setItem(row, 3, new QTableWidgetItem(QString("%1:%2").arg(dev.address.toString()).arg(dev.port)));
    m_deviceTable->setItem(row, 4, new QTableWidgetItem(dev.lastSeen.isValid() ? dev.lastSeen.toString("HH:mm:ss") : "--"));
    m_deviceTable->setItem(row, 5, new QTableWidgetItem(dev.lastMessage));
    m_deviceTable->setItem(row, 6, new QTableWidgetItem(dev.firmware.isEmpty() ? "--" : dev.firmware));
}

void MainWindow::onDeviceListChanged() {
    if (!m_controller) return;

    QList<DeviceRecord> list = m_controller->allDevices();
    m_deviceTable->setRowCount(list.size());

    for (int i = 0; i < list.size(); ++i) {
        updateDeviceTableRow(i, list[i]);
    }

    m_lblDeviceStats->setText(QString("在線: %1 | 總註冊: %2")
        .arg(m_controller->onlineDeviceCount()).arg(list.size()));
}

void MainWindow::onDeviceUpdated(const DeviceRecord& dev) {
    // Search row by MAC
    for (int r = 0; r < m_deviceTable->rowCount(); ++r) {
        auto* item = m_deviceTable->item(r, 2); // Column 2 is MAC
        if (item && item->text() == dev.mac) {
            updateDeviceTableRow(r, dev);
            if (m_controller) {
                m_lblDeviceStats->setText(QString("在線: %1 | 總註冊: %2")
                    .arg(m_controller->onlineDeviceCount()).arg(m_deviceTable->rowCount()));
            }
            return;
        }
    }
    // If not found in rows, full refresh
    onDeviceListChanged();
}

void MainWindow::onBroadcastSummary(const QString& messageId, int totalTargeted, int ackReceived, int ackErrors, int ackTimeouts) {
    QString summaryText = QString("廣播結果 (ID: %1) -> 目標: %2, 成功: %3, 失敗: %4, 逾時: %5")
        .arg(messageId.left(8)).arg(totalTargeted).arg(ackReceived).arg(ackErrors).arg(ackTimeouts);

    m_lblBroadcastSummary->setText(summaryText);
    m_lblBroadcastSummary->setStyleSheet(ackTimeouts == 0 && ackErrors == 0 ? "color: #27ae60; font-weight: bold;" : "color: #e67e22; font-weight: bold;");
}

void MainWindow::onLogAppended(LogLevel level, const QString& timestamp, const QString& module, const QString& message) {
    QString color = "#d4d4d4";
    switch (level) {
        case LogLevel::Info:   color = "#3498db"; break;
        case LogLevel::Warn:   color = "#f39c12"; break;
        case LogLevel::Error:  color = "#e74c3c"; break;
        case LogLevel::Packet: color = "#2ecc71"; break;
        case LogLevel::Debug:  color = "#95a5a6"; break;
    }

    QString html = QString("<span style='color: #7f8c8d;'>[%1]</span> <span style='color: %2; font-weight: bold;'>[%3]</span> %4")
        .arg(timestamp, color, module, message.toHtmlEscaped());

    m_txtLogConsole->append(html);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Save settings from UI
    AppSettings::instance().setSsid(m_txtSsid->text());
    AppSettings::instance().setPassphrase(m_txtPassphrase->text());
    AppSettings::instance().setAutoStopHotspotOnExit(m_chkAutoStopHotspot->isChecked());
    AppSettings::instance().save();

    if (m_controller) {
        m_controller->shutdown();
    }
    event->accept();
}
