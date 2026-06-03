#include "RealtimeEventDispatcher.h"

#include "AppEventBus.h"
#include "EventCursorStore.h"

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
    AppEventBus::instance().publish(event);
    EventCursorStore::instance().markProcessed(event);
    emit eventDispatched(event);
}
