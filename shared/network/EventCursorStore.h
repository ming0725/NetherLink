#pragma once

#include "NetworkTypes.h"

#include <QMutex>
#include <QObject>

class EventCursorStore : public QObject
{
    Q_OBJECT

public:
    static EventCursorStore& instance();

    qint64 lastEventSeq() const;
    QString lastEventId() const;
    bool markProcessed(const RealtimeEvent& event);
    void reset();

signals:
    void cursorChanged(qint64 lastEventSeq, const QString& lastEventId);

private:
    explicit EventCursorStore(QObject* parent = nullptr);
    Q_DISABLE_COPY(EventCursorStore)

    void load();
    void save();

    mutable QMutex m_mutex;
    qint64 m_lastEventSeq = 0;
    QString m_lastEventId;
    bool m_loaded = false;
};
