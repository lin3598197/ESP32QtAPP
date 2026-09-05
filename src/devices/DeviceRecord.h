#pragma once

#include <QString>
#include <QHostAddress>
#include <QDateTime>
#include <QJsonObject>

enum class DeviceStatus {
    Online,
    WaitingAck,
    KeyReceived,
    Timeout,
    Error,
    Offline
};

inline QString deviceStatusToString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::Online:      return "ONLINE";
        case DeviceStatus::WaitingAck:  return "WAITING_ACK";
        case DeviceStatus::KeyReceived: return "KEY_RECEIVED";
        case DeviceStatus::Timeout:     return "TIMEOUT";
        case DeviceStatus::Error:       return "ERROR";
        case DeviceStatus::Offline:     return "OFFLINE";
    }
    return "UNKNOWN";
}

struct DeviceRecord {
    QString mac;                // Normalized "AA:BB:CC:DD:EE:01"
    QString id;                 // Device ID (reported by ESP32 or persisted)
    QHostAddress address;       // IPv4
    quint16 port{4210};
    QString firmware;           // e.g. "1.0.0"
    QDateTime lastSeen;
    QString lastMessageId;
    DeviceStatus status{DeviceStatus::Offline};
    QString lastMessage;        // ACK message or error explanation

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["mac"] = mac;
        obj["id"] = id;
        obj["address"] = address.toString();
        obj["port"] = port;
        obj["firmware"] = firmware;
        obj["lastSeen"] = lastSeen.toString(Qt::ISODate);
        obj["lastMessageId"] = lastMessageId;
        obj["status"] = deviceStatusToString(status);
        obj["lastMessage"] = lastMessage;
        return obj;
    }

    static DeviceRecord fromJson(const QJsonObject& obj) {
        DeviceRecord r;
        r.mac = obj.value("mac").toString();
        r.id = obj.value("id").toString();
        r.address = QHostAddress(obj.value("address").toString());
        r.port = static_cast<quint16>(obj.value("port").toInt(4210));
        r.firmware = obj.value("firmware").toString();
        r.lastSeen = QDateTime::fromString(obj.value("lastSeen").toString(), Qt::ISODate);
        r.lastMessageId = obj.value("lastMessageId").toString();
        r.lastMessage = obj.value("lastMessage").toString();
        r.status = DeviceStatus::Offline; // Defaults to offline when loaded from disk
        return r;
    }
};
