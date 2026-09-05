#include "Protocol.h"
#include <QRegularExpression>
#include <QDateTime>

namespace Protocol {

QString normalizeMac(const QString& rawMac) {
    // 1. Remove '-', ':', spaces
    QString cleaned = rawMac;
    cleaned.remove('-').remove(':').remove(' ');
    // 2. To uppercase
    cleaned = cleaned.toUpper();
    // 3. Verify exactly 12 hex characters
    static const QRegularExpression hex12("^[0-9A-F]{12}$");
    if (!hex12.match(cleaned).hasMatch()) {
        return QString();
    }
    // 4. Insert ':' every 2 characters
    QString result;
    for (int i = 0; i < 12; i += 2) {
        if (i > 0) result.append(':');
        result.append(cleaned.mid(i, 2));
    }
    return result;
}

QByteArray HelloPacket::toJson() const {
    QJsonObject obj;
    obj["protocol"] = protocol;
    obj["version"] = version;
    obj["type"] = type;
    obj["mac"] = mac;
    obj["id"] = id;
    if (!firmware.isEmpty()) {
        obj["firmware"] = firmware;
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<HelloPacket> HelloPacket::fromJson(const QJsonObject& obj) {
    if (obj.value("type").toString() != "hello") return std::nullopt;

    HelloPacket p;
    p.protocol = obj.value("protocol").toString();
    p.version = obj.value("version").toInt(1);
    p.type = "hello";
    p.mac = normalizeMac(obj.value("mac").toString());
    p.id = obj.value("id").toString().trimmed();
    p.firmware = obj.value("firmware").toString();

    if (p.mac.isEmpty()) {
        return std::nullopt; // Invalid MAC
    }
    return p;
}

QByteArray SetKeyPacket::toJson() const {
    QJsonObject obj;
    obj["protocol"] = protocol;
    obj["version"] = version;
    obj["type"] = type;
    obj["message_id"] = messageId;
    obj["target"] = target;
    obj["chunk_key"] = chunkKey;
    if (!timestamp.isEmpty()) {
        obj["timestamp"] = timestamp;
    } else {
        obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<SetKeyPacket> SetKeyPacket::fromJson(const QJsonObject& obj) {
    if (obj.value("type").toString() != "set_key") return std::nullopt;

    SetKeyPacket p;
    p.protocol = obj.value("protocol").toString();
    p.version = obj.value("version").toInt(1);
    p.type = "set_key";
    p.messageId = obj.value("message_id").toString();
    p.target = obj.value("target").toString("all");
    p.chunkKey = obj.value("chunk_key").toString();
    p.timestamp = obj.value("timestamp").toString();

    if (p.messageId.isEmpty() || p.chunkKey.isEmpty()) {
        return std::nullopt;
    }
    return p;
}

QByteArray AckPacket::toJson() const {
    QJsonObject obj;
    obj["protocol"] = protocol;
    obj["version"] = version;
    obj["type"] = type;
    obj["message_id"] = messageId;
    obj["mac"] = mac;
    obj["id"] = id;
    obj["status"] = status;
    if (!message.isEmpty()) {
        obj["message"] = message;
    }
    if (!errorCode.isEmpty()) {
        obj["error_code"] = errorCode;
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<AckPacket> AckPacket::fromJson(const QJsonObject& obj) {
    if (obj.value("type").toString() != "key_ack") return std::nullopt;

    AckPacket p;
    p.protocol = obj.value("protocol").toString();
    p.version = obj.value("version").toInt(1);
    p.type = "key_ack";
    p.messageId = obj.value("message_id").toString();
    p.mac = normalizeMac(obj.value("mac").toString());
    p.id = obj.value("id").toString().trimmed();
    p.status = obj.value("status").toString().toLower();
    p.message = obj.value("message").toString();
    p.errorCode = obj.value("error_code").toString();

    if (p.messageId.isEmpty() || p.mac.isEmpty()) {
        return std::nullopt;
    }
    return p;
}

PacketType detectPacketType(const QByteArray& data, QJsonObject& outObj) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return PacketType::Unknown;
    }
    outObj = doc.object();
    QString type = outObj.value("type").toString();
    if (type == "hello") return PacketType::Hello;
    if (type == "set_key") return PacketType::SetKey;
    if (type == "key_ack") return PacketType::KeyAck;
    return PacketType::Unknown;
}

} // namespace Protocol
