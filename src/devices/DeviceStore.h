#pragma once

#include <QString>
#include <QMap>
#include "DeviceRecord.h"

class DeviceStore {
public:
    explicit DeviceStore(const QString& filePath = QString());

    bool load(QMap<QString, DeviceRecord>& outDevices);
    bool save(const QMap<QString, DeviceRecord>& devices);

    QString defaultFilePath() const;
    void setFilePath(const QString& path);

private:
    QString m_filePath;
};
