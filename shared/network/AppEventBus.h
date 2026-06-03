#pragma once

#include "NetworkTypes.h"

#include <QObject>

class AppEventBus : public QObject
{
    Q_OBJECT

public:
    static AppEventBus& instance();

    void publish(const RealtimeEvent& event);

signals:
    void eventReceived(const RealtimeEvent& event);
    void typedEventReceived(const QString& type, const QJsonObject& payload, const RealtimeEvent& event);
    void fullSyncRequired(const RealtimeEvent& event);

private:
    explicit AppEventBus(QObject* parent = nullptr);
    Q_DISABLE_COPY(AppEventBus)
};
