#include "CurrentUserPreferencesRepository.h"

#include "shared/data/LocalDataStore.h"
#include "shared/network/AppEventBus.h"
#include "shared/theme/ThemeManager.h"

#include <QColor>
#include <QJsonValue>
#include <QMutexLocker>

namespace {

constexpr auto kPreferencesDomain = "current_preferences";
constexpr auto kCurrentPreferencesKey = "current";

CurrentUserPreferences preferencesFromJson(QJsonObject object, const QString& responseEtag = {})
{
    CurrentUserPreferences preferences;
    preferences.themeColor = object.value(QStringLiteral("themeColor")).toString();
    preferences.fontMode = object.value(QStringLiteral("fontMode")).toString();
    preferences.inputEffects = object.value(QStringLiteral("inputEffects")).toObject();
    preferences.settings = object.value(QStringLiteral("settings")).toObject();
    preferences.version = object.value(QStringLiteral("version")).toInt();
    preferences.etag = object.value(QStringLiteral("etag")).toString(responseEtag);
    if (preferences.etag.isEmpty()) {
        preferences.etag = responseEtag;
    }
    return preferences;
}

QJsonObject preferencesToJson(const CurrentUserPreferences& preferences)
{
    QJsonObject object{
            {QStringLiteral("themeColor"), preferences.themeColor.isEmpty()
                                                ? QJsonValue(QJsonValue::Null)
                                                : QJsonValue(preferences.themeColor)},
            {QStringLiteral("fontMode"), preferences.fontMode.isEmpty()
                                            ? QJsonValue(QJsonValue::Null)
                                            : QJsonValue(preferences.fontMode)},
            {QStringLiteral("inputEffects"), preferences.inputEffects},
            {QStringLiteral("settings"), preferences.settings},
            {QStringLiteral("version"), preferences.version}
    };
    if (!preferences.etag.isEmpty()) {
        object.insert(QStringLiteral("etag"), preferences.etag);
    }
    return object;
}

bool preferencesEqual(const CurrentUserPreferences& lhs, const CurrentUserPreferences& rhs)
{
    return lhs.themeColor == rhs.themeColor &&
           lhs.fontMode == rhs.fontMode &&
           lhs.inputEffects == rhs.inputEffects &&
           lhs.settings == rhs.settings &&
           lhs.version == rhs.version &&
           lhs.etag == rhs.etag;
}

bool isStalePreferences(const CurrentUserPreferences& current, const CurrentUserPreferences& incoming)
{
    return current.version > 0 &&
           incoming.version > 0 &&
           incoming.version < current.version;
}

QJsonObject preferencesPayloadObject(const QJsonObject& payload)
{
    QJsonObject object = payload.value(QStringLiteral("preferences")).toObject();
    if (object.isEmpty()) {
        object = payload.value(QStringLiteral("preference")).toObject();
    }
    if (object.isEmpty()) {
        object = payload;
    }

    if (payload.contains(QStringLiteral("version")) && !object.contains(QStringLiteral("version"))) {
        object.insert(QStringLiteral("version"), payload.value(QStringLiteral("version")));
    }
    if (payload.contains(QStringLiteral("etag")) && !object.contains(QStringLiteral("etag"))) {
        object.insert(QStringLiteral("etag"), payload.value(QStringLiteral("etag")));
    }
    return object;
}

void applyPreferencesToTheme(const CurrentUserPreferences& preferences)
{
    if (preferences.themeColor.isEmpty()) {
        return;
    }

    const QColor color(preferences.themeColor);
    if (color.isValid()) {
        ThemeManager::instance().setThemeColor(color);
    }
}

} // namespace

CurrentUserPreferencesRepository& CurrentUserPreferencesRepository::instance()
{
    static CurrentUserPreferencesRepository repo;
    return repo;
}

CurrentUserPreferencesRepository::CurrentUserPreferencesRepository(QObject* parent)
    : QObject(parent)
{
    reloadFromStore();

    connect(&LocalDataStore::instance(),
            &LocalDataStore::activeAccountChanged,
            this,
            [this](const QString&) {
                reloadFromStore();
            });
    connect(&LocalDataStore::instance(),
            &LocalDataStore::domainChanged,
            this,
            [this](const QString& domain) {
                if (domain == QString::fromLatin1(kPreferencesDomain)) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);
    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
                if (type != QStringLiteral("preference.updated") &&
                    type != QStringLiteral("preferences.updated")) {
                    return;
                }

                saveCurrentUserPreferencesObject(preferencesPayloadObject(payload));
            });
}

void CurrentUserPreferencesRepository::reloadFromStore()
{
    CurrentUserPreferences nextPreferences;
    const QJsonObject object = LocalDataStore::instance().value(QString::fromLatin1(kPreferencesDomain),
                                                               QString::fromLatin1(kCurrentPreferencesKey));
    if (!object.isEmpty()) {
        nextPreferences = preferencesFromJson(object);
    }

    bool changed = false;
    bool restoreCurrentStoreValue = false;
    CurrentUserPreferences currentPreferences;
    {
        QMutexLocker locker(&m_mutex);
        if (isStalePreferences(m_preferences, nextPreferences)) {
            currentPreferences = m_preferences;
            restoreCurrentStoreValue = true;
            nextPreferences = m_preferences;
        }
        changed = !preferencesEqual(m_preferences, nextPreferences);
        m_preferences = nextPreferences;
    }
    if (restoreCurrentStoreValue) {
        LocalDataStore::instance().upsertValue(QString::fromLatin1(kPreferencesDomain),
                                               QString::fromLatin1(kCurrentPreferencesKey),
                                               preferencesToJson(currentPreferences));
    }
    if (changed) {
        applyPreferencesToTheme(nextPreferences);
        emit currentUserPreferencesChanged();
    }
}

CurrentUserPreferences CurrentUserPreferencesRepository::currentPreferences() const
{
    QMutexLocker locker(&m_mutex);
    return m_preferences;
}

CurrentUserPreferences CurrentUserPreferencesRepository::preferencesForAccount(const QString& accountKey) const
{
    const QJsonObject object = LocalDataStore::instance().valueForAccount(QString(accountKey).trimmed(),
                                                                         QString::fromLatin1(kPreferencesDomain),
                                                                         QString::fromLatin1(kCurrentPreferencesKey));
    return object.isEmpty() ? CurrentUserPreferences{} : preferencesFromJson(object);
}

void CurrentUserPreferencesRepository::saveCurrentUserPreferences(const CurrentUserPreferences& preferences)
{
    bool changed = false;
    CurrentUserPreferences nextPreferences = preferences;
    {
        QMutexLocker locker(&m_mutex);
        if (isStalePreferences(m_preferences, nextPreferences)) {
            return;
        }

        if (nextPreferences.etag.isEmpty()) {
            nextPreferences.etag = m_preferences.etag;
        }
        changed = !preferencesEqual(m_preferences, nextPreferences);
        m_preferences = nextPreferences;
    }

    LocalDataStore::instance().upsertValue(QString::fromLatin1(kPreferencesDomain),
                                           QString::fromLatin1(kCurrentPreferencesKey),
                                           preferencesToJson(nextPreferences));
    applyPreferencesToTheme(nextPreferences);
    if (changed) {
        emit currentUserPreferencesChanged();
    }
}

void CurrentUserPreferencesRepository::saveCurrentUserPreferencesObject(const QJsonObject& object,
                                                                       const QString& responseEtag)
{
    CurrentUserPreferences preferences = preferencesFromJson(object, responseEtag);
    const CurrentUserPreferences previous = currentPreferences();
    if (previous.isValid()) {
        preferences.themeColor = object.contains(QStringLiteral("themeColor"))
                ? preferences.themeColor
                : previous.themeColor;
        preferences.fontMode = object.contains(QStringLiteral("fontMode"))
                ? preferences.fontMode
                : previous.fontMode;
        preferences.inputEffects = object.contains(QStringLiteral("inputEffects"))
                ? preferences.inputEffects
                : previous.inputEffects;
        preferences.settings = object.contains(QStringLiteral("settings"))
                ? preferences.settings
                : previous.settings;
        preferences.version = object.contains(QStringLiteral("version"))
                ? preferences.version
                : previous.version;
        preferences.etag = preferences.etag.isEmpty() ? previous.etag : preferences.etag;
    }
    saveCurrentUserPreferences(preferences);
}
