#include "UdpManager.h"
#include "../common/LogService.h"
#include "../common/AppSettings.h"
#include <QNetworkAddressEntry>
#include <QDateTime>

UdpManager::UdpManager(QObject* parent) : QObject(parent) {
    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpManager::onReadyRead);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &UdpManager::onSocketError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &UdpManager::onSocketError);
#endif
}

UdpManager::~UdpManager() {
    stop();
}

bool UdpManager::start(quint16 port) {
    m_port = port;
    if (m_socket->isOpen()) {
        m_socket->close();
    }

    refreshNetworkInterfaces();

    bool success = m_socket->bind(QHostAddress::AnyIPv4, m_port,
                                  QUdpSocket::DontShareAddress);
    if (success) {
        LogService::instance().info("UDP", QString("Socket successfully bound to port %1").arg(m_port));
        emit socketBound(m_port);
    } else {
        QString err = m_socket->errorString();
        LogService::instance().error("UDP", QString("Failed to bind UDP socket to port %1: %2").arg(m_port).arg(err));
        emit socketErrorOccurred(err);
    }
    return success;
}

void UdpManager::stop() {
    if (m_socket && m_socket->isOpen()) {
        m_socket->close();
        LogService::instance().info("UDP", "Socket closed");
    }
}

bool UdpManager::isBound() const {
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

void UdpManager::refreshNetworkInterfaces() {
    // 1. Check if user specified a manual broadcast address
    QString manualBroadcast = AppSettings::instance().broadcastAddress();
    if (!manualBroadcast.isEmpty()) {
        m_broadcastAddress = QHostAddress(manualBroadcast);
        LogService::instance().info("UDP", QString("Using manually configured broadcast address: %1").arg(manualBroadcast));
        return;
    }

    // 2. Search network interfaces for Windows Hotspot adapter (typically 192.168.137.x subnet)
    QHostAddress foundDirectedBroadcast(QHostAddress::Broadcast);
    QHostAddress foundLocalIp(QHostAddress::Null);
    bool hotspotSubnetFound = false;

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        // Filter out loopback or inactive
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QString ipStr = entry.ip().toString();
                // Check for default Windows Mobile Hotspot IP
                if (ipStr.startsWith("192.168.137.")) {
                    foundLocalIp = entry.ip();
                    if (!entry.broadcast().isNull()) {
                        foundDirectedBroadcast = entry.broadcast();
                    } else {
                        foundDirectedBroadcast = QHostAddress("192.168.137.255");
                    }
                    hotspotSubnetFound = true;
                    LogService::instance().info("UDP", QString("Identified Windows Mobile Hotspot interface: %1 (IP: %2, Broadcast: %3)")
                        .arg(iface.humanReadableName(), ipStr, foundDirectedBroadcast.toString()));
                    break;
                } else if (!hotspotSubnetFound && !entry.broadcast().isNull() && entry.broadcast() != QHostAddress::Broadcast) {
                    // Store as fallback candidate
                    foundLocalIp = entry.ip();
                    foundDirectedBroadcast = entry.broadcast();
                }
            }
        }
        if (hotspotSubnetFound) break;
    }

    m_localAddress = foundLocalIp;
    m_broadcastAddress = foundDirectedBroadcast;
    LogService::instance().info("UDP", QString("Active broadcast target: %1 (Local adapter IP: %2)")
        .arg(m_broadcastAddress.toString(), m_localAddress.toString()));
}

QHostAddress UdpManager::detectedBroadcastAddress() const {
    return m_broadcastAddress;
}

QHostAddress UdpManager::detectedLocalAddress() const {
    return m_localAddress;
}

bool UdpManager::broadcastKey(const QString& chunkKey, const QString& messageId) {
    if (!isBound()) {
        LogService::instance().error("UDP", "Cannot broadcast key: UDP socket is not bound!");
        return false;
    }

    Protocol::SetKeyPacket packet;
    packet.messageId = messageId;
    packet.target = "all";
    packet.chunkKey = chunkKey;
    packet.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QByteArray data = packet.toJson();

    // Re-verify network interfaces in case hotspot was just started
    refreshNetworkInterfaces();

    qint64 bytesWritten = m_socket->writeDatagram(data, m_broadcastAddress, m_port);
    if (bytesWritten > 0) {
        LogService::instance().packet("UDP", QString("BROADCAST -> %1:%2 | ID: %3 | Key: %4 (%5 bytes)")
            .arg(m_broadcastAddress.toString()).arg(m_port).arg(messageId).arg(chunkKey).arg(bytesWritten));
        emit broadcastSent(messageId, m_broadcastAddress, m_port);
        return true;
    } else {
        LogService::instance().error("UDP", QString("Failed to send broadcast datagram: %1").arg(m_socket->errorString()));
        return false;
    }
}

void UdpManager::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress senderAddress;
        quint16 senderPort = 0;

        m_socket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        QJsonObject obj;
        Protocol::PacketType ptype = Protocol::detectPacketType(datagram, obj);

        if (ptype == Protocol::PacketType::Hello) {
            auto helloOpt = Protocol::HelloPacket::fromJson(obj);
            if (helloOpt.has_value()) {
                LogService::instance().packet("UDP", QString("RECV HELLO <- %1:%2 | MAC: %3 | ID: %4 | FW: %5")
                    .arg(senderAddress.toString()).arg(senderPort).arg(helloOpt->mac, helloOpt->id, helloOpt->firmware));
                emit helloReceived(helloOpt.value(), senderAddress, senderPort);
            } else {
                LogService::instance().warn("UDP", QString("Malformed Hello packet from %1:%2: %3")
                    .arg(senderAddress.toString()).arg(senderPort).arg(QString::fromUtf8(datagram)));
            }
        } else if (ptype == Protocol::PacketType::KeyAck) {
            auto ackOpt = Protocol::AckPacket::fromJson(obj);
            if (ackOpt.has_value()) {
                LogService::instance().packet("UDP", QString("RECV ACK <- %1:%2 | MsgID: %3 | MAC: %4 | Status: %5 (%6)")
                    .arg(senderAddress.toString()).arg(senderPort).arg(ackOpt->messageId, ackOpt->mac, ackOpt->status, ackOpt->message));
                emit ackReceived(ackOpt.value(), senderAddress, senderPort);
            } else {
                LogService::instance().warn("UDP", QString("Malformed Ack packet from %1:%2: %3")
                    .arg(senderAddress.toString()).arg(senderPort).arg(QString::fromUtf8(datagram)));
            }
        } else if (ptype == Protocol::PacketType::SetKey) {
            // Received own broadcast or another controller, ignore
            LogService::instance().debug("UDP", QString("Ignoring self/foreign SetKey broadcast from %1:%2").arg(senderAddress.toString()).arg(senderPort));
        } else {
            LogService::instance().warn("UDP", QString("Unknown or non-JSON datagram from %1:%2: %3")
                .arg(senderAddress.toString()).arg(senderPort).arg(QString::fromUtf8(datagram)));
        }
    }
}

void UdpManager::onSocketError(QAbstractSocket::SocketError errCode) {
    Q_UNUSED(errCode);
    QString errorMsg = m_socket->errorString();
    LogService::instance().error("UDP", QString("Socket error occurred: %1").arg(errorMsg));
    emit socketErrorOccurred(errorMsg);
}
