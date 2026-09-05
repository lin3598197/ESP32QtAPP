#include "DeviceManager.h"
#include "../common/LogService.h"
#include "../common/AppSettings.h"

DeviceManager::DeviceManager(QObject* parent) : QObject(parent) {
    m_ackTimer = new QTimer(this);
    m_ackTimer->setSingleShot(true);
    connect(m_ackTimer, &QTimer::timeout, this, &DeviceManager::onAckTimeout);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(4000); // Check every 4 seconds
    connect(m_heartbeatTimer, &QTimer::timeout, this, &DeviceManager::onHeartbeatCheck);
}

DeviceManager::~DeviceManager() {
    save();
}

void DeviceManager::initialize() {
    m_devices.clear(); // Current session starts completely empty (no past history)
    QMap<QString, DeviceRecord> historical;
    m_store.load(historical);
    for (auto it = historical.begin(); it != historical.end(); ++it) {
        if (!it.value().id.isEmpty()) {
            m_knownIdByMac[it.key()] = it.value().id;
        }
    }
    m_heartbeatTimer->start();
    emit deviceListChanged();
}

void DeviceManager::save() {
    QMap<QString, DeviceRecord> toSave;
    for (auto it = m_knownIdByMac.begin(); it != m_knownIdByMac.end(); ++it) {
        DeviceRecord rec;
        rec.mac = it.key();
        rec.id = it.value();
        rec.status = DeviceStatus::Offline;
        toSave[it.key()] = rec;
    }
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        toSave[it.key()] = it.value();
    }
    m_store.save(toSave);
}

QList<DeviceRecord> DeviceManager::allDevices() const {
    return m_devices.values();
}

std::optional<DeviceRecord> DeviceManager::getDevice(const QString& normalizedMac) const {
    auto it = m_devices.find(normalizedMac);
    if (it != m_devices.end()) {
        return it.value();
    }
    return std::nullopt;
}

int DeviceManager::onlineCount() const {
    int count = 0;
    for (const auto& dev : m_devices) {
        if (dev.status != DeviceStatus::Offline) {
            count++;
        }
    }
    return count;
}

int DeviceManager::totalCount() const {
    return m_devices.size();
}

QString DeviceManager::allocateFallbackId(const QString& mac) {
    // If no ID provided, format as ESP32-XXXXXX using last 6 chars of MAC
    QString clean = mac;
    clean.remove(':');
    if (clean.length() >= 6) {
        return QString("ESP32-%1").arg(clean.right(6));
    }
    return QString("ESP32-%1").arg(QString::number(m_devices.size() + 1).rightJustified(3, '0'));
}

void DeviceManager::handleHello(const Protocol::HelloPacket& hello, const QHostAddress& ip, quint16 port) {
    QString mac = hello.mac;
    if (mac.isEmpty()) {
        LogService::instance().warn("DEV", "Rejected Hello packet with empty/invalid MAC");
        return;
    }

    bool isNew = !m_devices.contains(mac);
    DeviceRecord& dev = m_devices[mac];

    dev.mac = mac;
    dev.address = ip;
    dev.port = port;
    dev.lastSeen = QDateTime::currentDateTime();

    if (!hello.firmware.isEmpty()) {
        dev.firmware = hello.firmware;
    }

    // ID logic: Per user requirement:
    // "ID會由ESP32發送UDP到電腦，透過封包的MAC確認這個ID是來自哪台裝置"
    if (!hello.id.isEmpty()) {
        if (dev.id != hello.id) {
            LogService::instance().info("DEV", QString("Device [%1] updated ID from '%2' to '%3'")
                .arg(mac, dev.id, hello.id));
            dev.id = hello.id;
            m_knownIdByMac[mac] = hello.id;
        }
    } else if (dev.id.isEmpty()) {
        if (m_knownIdByMac.contains(mac)) {
            dev.id = m_knownIdByMac[mac];
        } else {
            dev.id = allocateFallbackId(mac);
            m_knownIdByMac[mac] = dev.id;
        }
    }

    bool wasNotOnline = (dev.status != DeviceStatus::Online);
    dev.status = DeviceStatus::Online;
    dev.lastMessage = "Online";

    if (isNew) {
        LogService::instance().info("DEV", QString("Device connected in this session: MAC=%1, ID=%2, IP=%3:%4")
            .arg(mac, dev.id, ip.toString()).arg(port));
        save();
        emit deviceListChanged();
    } else {
        if (wasNotOnline) {
            LogService::instance().info("DEV", QString("Device [%1 | %2] resumed ONLINE (%3:%4)")
                .arg(dev.id, dev.mac, ip.toString()).arg(port));
        }
        emit deviceUpdated(dev);
    }
}

void DeviceManager::prepareForBroadcast(const QString& messageId) {
    m_activeBroadcastMessageId = messageId;
    m_broadcastPending = true;

    // Transition all currently non-offline devices to WAITING_ACK
    for (auto& dev : m_devices) {
        if (dev.status != DeviceStatus::Offline) {
            dev.status = DeviceStatus::WaitingAck;
            dev.lastMessageId = messageId;
            dev.lastMessage = "Waiting for ACK...";
            emit deviceUpdated(dev);
        }
    }

    int timeoutMs = AppSettings::instance().ackTimeoutMs();
    m_ackTimer->stop();
    m_ackTimer->start(timeoutMs);

    LogService::instance().info("DEV", QString("Broadcast [%1] armed. Waiting %2 ms for ACKs...")
        .arg(messageId).arg(timeoutMs));
}

void DeviceManager::handleAck(const Protocol::AckPacket& ack, const QHostAddress& ip, quint16 port) {
    Q_UNUSED(port);
    QString mac = ack.mac;
    if (!m_devices.contains(mac)) {
        LogService::instance().warn("DEV", QString("Received ACK from unregistered MAC: %1 (MsgID: %2)")
            .arg(mac, ack.messageId));
        return;
    }

    DeviceRecord& dev = m_devices[mac];
    dev.address = ip;
    dev.lastSeen = QDateTime::currentDateTime();
    dev.lastMessageId = ack.messageId;

    if (ack.isOk()) {
        dev.status = DeviceStatus::KeyReceived;
        dev.lastMessage = ack.message.isEmpty() ? "ACK received" : ack.message;
        LogService::instance().info("DEV", QString("Device [%1 | %2] ACK SUCCESS: %3")
            .arg(dev.id, mac, dev.lastMessage));
    } else {
        dev.status = DeviceStatus::Error;
        dev.lastMessage = QString("Error [%1]: %2").arg(ack.errorCode, ack.message);
        LogService::instance().warn("DEV", QString("Device [%1 | %2] ACK ERROR: %3")
            .arg(dev.id, mac, dev.lastMessage));
    }

    emit deviceUpdated(dev);

    // Check if all waiting devices have replied
    if (m_broadcastPending) {
        bool anyWaiting = false;
        for (const auto& d : m_devices) {
            if (d.status == DeviceStatus::WaitingAck) {
                anyWaiting = true;
                break;
            }
        }
        if (!anyWaiting) {
            m_ackTimer->stop();
            onAckTimeout(); // All finished early
        }
    }
}

void DeviceManager::onAckTimeout() {
    if (!m_broadcastPending) return;
    m_broadcastPending = false;

    int totalTargeted = 0;
    int ackReceived = 0;
    int ackErrors = 0;
    int ackTimeouts = 0;

    for (auto& dev : m_devices) {
        if (dev.lastMessageId == m_activeBroadcastMessageId) {
            totalTargeted++;
            if (dev.status == DeviceStatus::WaitingAck) {
                dev.status = DeviceStatus::Timeout;
                dev.lastMessage = "ACK Timeout (no response)";
                ackTimeouts++;
                emit deviceUpdated(dev);
            } else if (dev.status == DeviceStatus::KeyReceived) {
                ackReceived++;
            } else if (dev.status == DeviceStatus::Error) {
                ackErrors++;
            }
        }
    }

    LogService::instance().info("DEV", QString("Broadcast [%1] completed. Target: %2, OK: %3, Error: %4, Timeout: %5")
        .arg(m_activeBroadcastMessageId).arg(totalTargeted).arg(ackReceived).arg(ackErrors).arg(ackTimeouts));

    emit broadcastSummary(m_activeBroadcastMessageId, totalTargeted, ackReceived, ackErrors, ackTimeouts);
}

void DeviceManager::onHeartbeatCheck() {
    QDateTime now = QDateTime::currentDateTime();
    int thresholdSec = AppSettings::instance().offlineThresholdSec();
    bool changed = false;

    for (auto& dev : m_devices) {
        if (dev.status != DeviceStatus::Offline) {
            qint64 elapsed = dev.lastSeen.secsTo(now);
            if (elapsed > thresholdSec) {
                dev.status = DeviceStatus::Offline;
                dev.lastMessage = QString("Offline (no hello for %1s)").arg(elapsed);
                changed = true;
                LogService::instance().info("DEV", QString("Device [%1 | %2] marked OFFLINE").arg(dev.id, dev.mac));
                emit deviceUpdated(dev);
            }
        }
    }

    if (changed) {
        emit deviceListChanged();
    }
}
