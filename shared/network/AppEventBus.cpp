#include "AppEventBus.h"

AppEventBus& AppEventBus::instance()
{
    static AppEventBus bus;
    return bus;
}

AppEventBus::AppEventBus(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<RealtimeEvent>("RealtimeEvent");
}

void AppEventBus::publish(const RealtimeEvent& event)
{
    emit eventReceived(event);
    emit typedEventReceived(event.type, event.payload, event);
    if (event.requiresFullSync()) {
        emit fullSyncRequired(event);
    }
}
