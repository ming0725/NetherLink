#include "EventCursorStore.h"

#include "shared/data/LocalDataStore.h"

#include <QDateTime>
#include <QJsonObject>
#include <QMutexLocker>

namespace {

const QString kDomain(QStringLiteral("network_event_cursor"));
const QString kGlobalCursorKey(QStringLiteral("global"));

} // namespace

EventCursorStore& EventCursorStore::instance()
{
    static EventCursorStore store;
    return store;
}

EventCursorStore::EventCursorStore(QObject* parent)
    : QObject(parent)
{
}

qint64 EventCursorStore::lastEventSeq() const
{
    QMutexLocker locker(&m_mutex);
    const_cast<EventCursorStore*>(this)->load();
    return m_lastEventSeq;
}

QString EventCursorStore::lastEventId() const
{
    QMutexLocker locker(&m_mutex);
    const_cast<EventCursorStore*>(this)->load();
    return m_lastEventId;
}

bool EventCursorStore::markProcessed(const RealtimeEvent& event)
{
    if (event.eventSeq <= 0) {
        return false;
    }

    qint64 nextSeq = 0;
    QString nextId;
    {
        QMutexLocker locker(&m_mutex);
        load();
        if (event.eventSeq <= m_lastEventSeq) {
            return false;
        }
        m_lastEventSeq = event.eventSeq;
        m_lastEventId = event.eventId;
        save();
        nextSeq = m_lastEventSeq;
        nextId = m_lastEventId;
    }
    emit cursorChanged(nextSeq, nextId);
    return true;
}

void EventCursorStore::reset()
{
    {
        QMutexLocker locker(&m_mutex);
        m_loaded = true;
        m_lastEventSeq = 0;
        m_lastEventId.clear();
        LocalDataStore::instance().removeValue(kDomain, kGlobalCursorKey);
    }
    emit cursorChanged(0, {});
}

void EventCursorStore::load()
{
    if (m_loaded) {
        return;
    }
    const QJsonObject object = LocalDataStore::instance().value(kDomain, kGlobalCursorKey);
    m_lastEventSeq = static_cast<qint64>(object.value(QStringLiteral("lastEventSeq")).toDouble());
    m_lastEventId = object.value(QStringLiteral("lastEventId")).toString();
    m_loaded = true;
}

void EventCursorStore::save()
{
    LocalDataStore::instance().upsertValue(kDomain, kGlobalCursorKey, {
            {QStringLiteral("lastEventSeq"), static_cast<double>(m_lastEventSeq)},
            {QStringLiteral("lastEventId"), m_lastEventId},
            {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    });
}
