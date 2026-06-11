#pragma once

#include <QString>
#include <QStringList>

#include "RpcServiceBase.h"

class RecordService : public RpcServiceBase {
public:
    explicit RecordService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void getRetention(QObject* context, JsonCallback cb) const;
    void setRetention(QObject* context, quint64 seconds, JsonCallback cb) const;
    void listRecent(QObject* context, const QString& type, quint64 count,
                    quint64 lastSeconds, JsonCallback cb) const;
    void fetch(QObject* context, const QString& type, const QStringList& recordIds,
               JsonCallback cb) const;
    void removeRecords(QObject* context, const QString& type, const QStringList& recordIds,
                       JsonCallback cb) const;
};
