#include "WindowsHotspotController.h"
#include "../common/LogService.h"
#include <QDesktopServices>
#include <QUrl>
#include <QMetaObject>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QtConcurrent/QtConcurrent>

namespace {

QString getHelperScriptPath() {
    QString appDir = QCoreApplication::applicationDirPath();
    // 1. Check <appDir>/scripts/hotspot_helper.ps1
    QString path1 = appDir + "/scripts/hotspot_helper.ps1";
    if (QFile::exists(path1)) return QDir::toNativeSeparators(path1);

    // 2. Check <appDir>/../scripts/hotspot_helper.ps1
    QString path2 = appDir + "/../scripts/hotspot_helper.ps1";
    if (QFile::exists(path2)) return QDir::toNativeSeparators(path2);

    return path1;
}

QString executePowerShellHelper(const QStringList& arguments, int timeoutMs = 12000) {
    QProcess process;
    QString scriptPath = getHelperScriptPath();

    QStringList params;
    params << "-ExecutionPolicy" << "Bypass" << "-NoProfile" << "-File" << scriptPath;
    params.append(arguments);

    process.start("powershell.exe", params);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        return QString("ERROR:Timeout");
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QString errorOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!errorOutput.isEmpty()) {
        LogService::instance().debug("HOTSPOT", QString("PS Stderr: %1").arg(errorOutput));
    }
    return output;
}

} // namespace

WindowsHotspotController::WindowsHotspotController(QObject* parent)
    : IHotspotController(parent) {
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(4000); // Poll status every 4 seconds
    connect(m_pollTimer, &QTimer::timeout, this, &WindowsHotspotController::pollStatus);
}

WindowsHotspotController::~WindowsHotspotController() {
    m_pollTimer->stop();
}

HotspotState WindowsHotspotController::state() const {
    return m_state;
}

QString WindowsHotspotController::ssid() const {
    return m_ssid;
}

int WindowsHotspotController::connectedClientCount() const {
    return m_clientCount;
}

void WindowsHotspotController::updateState(HotspotState newState, const QString& details) {
    if (m_state != newState) {
        m_state = newState;
        LogService::instance().info("HOTSPOT", QString("Hotspot state changed to %1 (%2)")
            .arg(hotspotStateToString(newState), details));
        emit stateChanged(newState, details);
    }
}

void WindowsHotspotController::openWindowsSettings() {
    LogService::instance().info("HOTSPOT", "Opening Windows Mobile Hotspot settings page (ms-settings:network-mobilehotspot)...");
    QDesktopServices::openUrl(QUrl("ms-settings:network-mobilehotspot"));
}

void WindowsHotspotController::startHotspot(const QString& ssid, const QString& passphrase) {
    m_ssid = ssid;
    m_passphrase = passphrase;
    updateState(HotspotState::Starting, "正在透過 Windows Runtime 啟動行動熱點 (2.4GHz)...");

    QtConcurrent::run([this, ssid, passphrase]() {
        QStringList args;
        args << "-Action" << "start" << "-Ssid" << ssid << "-Passphrase" << passphrase;

        QString output = executePowerShellHelper(args, 15000);
        LogService::instance().info("HOTSPOT", QString("WinRT Hotspot Output: %1").arg(output));

        QMetaObject::invokeMethod(this, [this, output, ssid]() {
            if (output.contains("RESULT:Success|On") || output.contains("|On|")) {
                updateState(HotspotState::Running, QString("熱點已啟動 (SSID: %1, 2.4GHz)").arg(ssid));
                m_pollTimer->start();
            } else if (output.contains("NO_PROFILE")) {
                updateState(HotspotState::Unsupported, "未找到可分享的網路連線設定檔。");
                emit errorOccurred("電腦目前無可分享的網際網路連線。");
            } else {
                updateState(HotspotState::Failed, QString("啟動失敗：%1").arg(output));
                emit errorOccurred(QString("熱點啟動失敗：%1").arg(output));
            }
        });
    });
}

void WindowsHotspotController::stopHotspot() {
    updateState(HotspotState::Stopping, "正在關閉行動熱點...");
    m_pollTimer->stop();

    QtConcurrent::run([this]() {
        QStringList args;
        args << "-Action" << "stop";

        QString output = executePowerShellHelper(args, 8000);
        LogService::instance().info("HOTSPOT", QString("Stop Hotspot Output: %1").arg(output));

        QMetaObject::invokeMethod(this, [this]() {
            updateState(HotspotState::Stopped, "行動熱點已關閉。");
            m_clientCount = 0;
            emit clientCountChanged(0);
        });
    });
}

void WindowsHotspotController::pollStatus() {
    QtConcurrent::run([this]() {
        QStringList args;
        args << "-Action" << "status";

        QString output = executePowerShellHelper(args, 4000);
        if (!output.contains("RESULT:")) return;

        // Example: RESULT:On|2|ESP32_Host
        QString raw = output.section("RESULT:", 1, 1).trimmed();
        QStringList parts = raw.split('|');
        if (parts.size() >= 2) {
            QString opState = parts[0];
            int clients = parts[1].toInt();
            QString effectiveSsid = parts.size() >= 3 ? parts[2] : m_ssid;

            QMetaObject::invokeMethod(this, [this, opState, clients, effectiveSsid]() {
                if (effectiveSsid != m_ssid && !effectiveSsid.isEmpty()) {
                    m_ssid = effectiveSsid;
                }

                if (m_clientCount != clients) {
                    m_clientCount = clients;
                    emit clientCountChanged(clients);
                }

                if (opState.compare("On", Qt::CaseInsensitive) == 0) {
                    if (m_state != HotspotState::Running) {
                        updateState(HotspotState::Running, "熱點運行中 (2.4GHz)");
                    }
                } else if (opState.compare("Off", Qt::CaseInsensitive) == 0) {
                    if (m_state == HotspotState::Running) {
                        updateState(HotspotState::Stopped, "熱點已被外部手動關閉。");
                    }
                }
            });
        }
    });
}
