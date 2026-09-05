#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>
#include "DeviceRecord.h"
#include "DeviceStore.h"
#include "../network/Protocol.h"

class DeviceManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceManager(QObject* parent = nullptr);
    ~DeviceManager() override;

    void initialize();
    void save();

    // Device querying
    QList<DeviceRecord> allDevices() const;
    std::optional<DeviceRecord> getDevice(const QString& normalizedMac) const;
    int onlineCount() const;
    int totalCount() const;

    // Triggered when broadcast is sent
    void prepareForBroadcast(const QString& messageId);

public slots:
    void handleHello(const Protocol::HelloPacket& hello, const QHostAddress& ip, quint16 port);
    void handleAck(const Protocol::AckPacket& ack, const QHostAddress& ip, quint16 port);

signals:
    void deviceListChanged();
    void deviceUpdated(const DeviceRecord& dev);
    void broadcastSummary(const QString& messageId, int totalTargeted, int ackReceived, int ackErrors, int ackTimeouts);

private slots:
    void onAckTimeout();
    void onHeartbeatCheck();

private:
    QString allocateFallbackId(const QString& mac);

    QMap<QString, DeviceRecord> m_devices; // Key: Normalized MAC (Current session devices only)
    QMap<QString, QString> m_knownIdByMac; // Historical MAC -> ID lookup
    DeviceStore m_store;
    QTimer* m_ackTimer{nullptr};
    QTimer* m_heartbeatTimer{nullptr};

    QString m_activeBroadcastMessageId;
    bool m_broadcastPending{false};
};
