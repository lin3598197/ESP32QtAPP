#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include "Protocol.h"

class UdpManager : public QObject {
    Q_OBJECT
public:
    explicit UdpManager(QObject* parent = nullptr);
    ~UdpManager() override;

    bool start(quint16 port = Protocol::DEFAULT_UDP_PORT);
    void stop();
    bool isBound() const;

    // Send single broadcast packet
    bool broadcastKey(const QString& chunkKey, const QString& messageId);

    // Get current effective broadcast address and local hotspot IP
    QHostAddress detectedBroadcastAddress() const;
    QHostAddress detectedLocalAddress() const;

    // Refresh network interface discovery
    void refreshNetworkInterfaces();

signals:
    void helloReceived(const Protocol::HelloPacket& hello, const QHostAddress& senderIp, quint16 senderPort);
    void ackReceived(const Protocol::AckPacket& ack, const QHostAddress& senderIp, quint16 senderPort);
    void broadcastSent(const QString& messageId, const QHostAddress& targetAddr, quint16 targetPort);
    void socketBound(quint16 port);
    void socketErrorOccurred(const QString& errorMsg);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError errCode);

private:
    QUdpSocket* m_socket{nullptr};
    quint16 m_port{Protocol::DEFAULT_UDP_PORT};
    QHostAddress m_broadcastAddress{QHostAddress::Broadcast};
    QHostAddress m_localAddress{QHostAddress::Null};
};
