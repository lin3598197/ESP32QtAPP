#include "DeviceStore.h"
#include "../common/LogService.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>

DeviceStore::DeviceStore(const QString& filePath) {
    if (filePath.isEmpty()) {
        m_filePath = defaultFilePath();
    } else {
        m_filePath = filePath;
    }
}

QString DeviceStore::defaultFilePath() const {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData + "/devices.json";
}

void DeviceStore::setFilePath(const QString& path) {
    m_filePath = path;
}

bool DeviceStore::load(QMap<QString, DeviceRecord>& outDevices) {
    QFile file(m_filePath);
    if (!file.exists()) {
        return true; // No file yet, fresh start
    }
    if (!file.open(QIODevice::ReadOnly)) {
        LogService::instance().warn("STORE", QString("Could not open device store file for reading: %1").arg(m_filePath));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        LogService::instance().warn("STORE", QString("Invalid JSON format in %1: %2").arg(m_filePath, err.errorString()));
        return false;
    }

    QJsonArray array = doc.array();
    for (const QJsonValue& val : array) {
        if (val.isObject()) {
            DeviceRecord rec = DeviceRecord::fromJson(val.toObject());
            if (!rec.mac.isEmpty()) {
                outDevices.insert(rec.mac, rec);
            }
        }
    }

    LogService::instance().info("STORE", QString("Loaded %1 registered devices from %2").arg(outDevices.size()).arg(m_filePath));
    return true;
}

bool DeviceStore::save(const QMap<QString, DeviceRecord>& devices) {
    QJsonArray array;
    for (const auto& dev : devices) {
        array.append(dev.toJson());
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LogService::instance().error("STORE", QString("Failed to open device store file for writing: %1").arg(m_filePath));
        return false;
    }

    QJsonDocument doc(array);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
