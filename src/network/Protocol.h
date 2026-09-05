#pragma once

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <optional>

namespace Protocol {

constexpr const char* PROTOCOL_NAME = "esp32-control";
constexpr int PROTOCOL_VERSION = 1;
constexpr quint16 DEFAULT_UDP_PORT = 4210;

// Normalize MAC address to "AA:BB:CC:DD:EE:FF" format
// Returns empty QString if invalid
QString normalizeMac(const QString& rawMac);

struct HelloPacket {
    QString protocol{PROTOCOL_NAME};
    int version{PROTOCOL_VERSION};
    QString type{"hello"};
    QString mac;
    QString id;
    QString firmware{"1.0.0"};

    QByteArray toJson() const;
    static std::optional<HelloPacket> fromJson(const QJsonObject& obj);
};

struct SetKeyPacket {
    QString protocol{PROTOCOL_NAME};
    int version{PROTOCOL_VERSION};
    QString type{"set_key"};
    QString messageId;
    QString target{"all"};
    QString chunkKey;
    QString timestamp;

    QByteArray toJson() const;
    static std::optional<SetKeyPacket> fromJson(const QJsonObject& obj);
};

struct AckPacket {
    QString protocol{PROTOCOL_NAME};
    int version{PROTOCOL_VERSION};
    QString type{"key_ack"};
    QString messageId;
    QString mac;
    QString id;
    QString status; // "ok" or "error"
    QString message;
    QString errorCode;

    bool isOk() const { return status.compare("ok", Qt::CaseInsensitive) == 0; }
    QByteArray toJson() const;
    static std::optional<AckPacket> fromJson(const QJsonObject& obj);
};

enum class PacketType {
    Unknown,
    Hello,
    SetKey,
    KeyAck
};

PacketType detectPacketType(const QByteArray& data, QJsonObject& outObj);

} // namespace Protocol
