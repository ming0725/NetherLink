#pragma once

#include "NetworkTypes.h"

#include <QObject>

class RealtimeEventDispatcher : public QObject
{
    Q_OBJECT

public:
    static RealtimeEventDispatcher& instance();

public slots:
    void dispatch(const RealtimeEvent& event);

signals:
    void eventDispatched(const RealtimeEvent& event);

private:
    explicit RealtimeEventDispatcher(QObject* parent = nullptr);
    Q_DISABLE_COPY(RealtimeEventDispatcher)
};
