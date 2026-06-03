#include "CurrentUserPreferencesRepository.h"

#include "shared/data/LocalDataStore.h"

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

} // namespace

CurrentUserPreferencesRepository& CurrentUserPreferencesRepository::instance()
{
    static CurrentUserPreferencesRepository repo;
    return repo;
}

CurrentUserPreferencesRepository::CurrentUserPreferencesRepository(QObject* parent)
    : QObject(parent)
{
    const QJsonObject object = LocalDataStore::instance().value(QString::fromLatin1(kPreferencesDomain),
                                                               QString::fromLatin1(kCurrentPreferencesKey));
    if (!object.isEmpty()) {
        m_preferences = preferencesFromJson(object);
    }
}

CurrentUserPreferences CurrentUserPreferencesRepository::currentPreferences() const
{
    QMutexLocker locker(&m_mutex);
    return m_preferences;
}

void CurrentUserPreferencesRepository::saveCurrentUserPreferences(const CurrentUserPreferences& preferences)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = !preferencesEqual(m_preferences, preferences);
        m_preferences = preferences;
    }

    LocalDataStore::instance().upsertValue(QString::fromLatin1(kPreferencesDomain),
                                           QString::fromLatin1(kCurrentPreferencesKey),
                                           preferencesToJson(preferences));
    if (changed) {
        emit currentUserPreferencesChanged();
    }
}

void CurrentUserPreferencesRepository::saveCurrentUserPreferencesObject(const QJsonObject& object,
                                                                       const QString& responseEtag)
{
    saveCurrentUserPreferences(preferencesFromJson(object, responseEtag));
}
