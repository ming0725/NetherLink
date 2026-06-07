#include "RealtimeEventDispatcher.h"

#include "AppEventBus.h"
#include "EventCursorStore.h"

#include <QJsonArray>
#include <QJsonValue>

RealtimeEventDispatcher& RealtimeEventDispatcher::instance()
{
    static RealtimeEventDispatcher dispatcher;
    return dispatcher;
}

RealtimeEventDispatcher::RealtimeEventDispatcher(QObject* parent)
    : QObject(parent)
{
}

void RealtimeEventDispatcher::dispatch(const RealtimeEvent& event)
{
    if (event.type == QStringLiteral("event.replay")) {
        AppEventBus::instance().publish(event);

        const QJsonArray events = event.payload.value(QStringLiteral("events")).toArray();
        for (const QJsonValue& value : events) {
            const QJsonObject object = value.toObject();
            if (object.isEmpty()) {
                continue;
            }
            const RealtimeEvent replayedEvent = RealtimeEvent::fromJson(object);
            if (!replayedEvent.type.isEmpty()) {
                dispatch(replayedEvent);
            }
        }

        EventCursorStore::instance().markProcessed(event);
        emit eventDispatched(event);
        return;
    }

    AppEventBus::instance().publish(event);
    EventCursorStore::instance().markProcessed(event);
    emit eventDispatched(event);
}
