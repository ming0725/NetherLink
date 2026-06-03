#include "CurrentUserProfileRepository.h"

#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/services/ImageService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <utility>

namespace {

constexpr auto kDefaultAvatarPath = ":/resources/avatar/9.jpg";

CurrentUserProfile fallbackProfile(const QString& userId)
{
    CurrentUserProfile profile;
    profile.userId = userId;
    profile.nickName = userId.isEmpty() ? QStringLiteral("未登录用户") : userId;
    profile.avatarPath = QString::fromLatin1(kDefaultAvatarPath);
    profile.status = Offline;
    return profile;
}

CurrentUserProfile identityOnly(CurrentUserProfile profile)
{
    profile.signature.clear();
    profile.region.clear();
    return profile;
}

QString statusToString(UserStatus status)
{
    switch (status) {
    case Online:
        return QStringLiteral("online");
    case Mining:
        return QStringLiteral("mining");
    case Flying:
        return QStringLiteral("flying");
    case Offline:
    default:
        return QStringLiteral("offline");
    }
}

UserStatus statusFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("online")) {
        return Online;
    }
    if (normalized == QStringLiteral("mining")) {
        return Mining;
    }
    if (normalized == QStringLiteral("flying")) {
        return Flying;
    }
    return Offline;
}

CurrentUserProfile profileFromJson(const QJsonObject& object)
{
    CurrentUserProfile profile;
    profile.userId = object.value(QStringLiteral("userId")).toString();
    profile.nickName = object.value(QStringLiteral("nickName")).toString();
    profile.avatarPath = object.value(QStringLiteral("avatarPath")).toString(QString::fromLatin1(kDefaultAvatarPath));
    profile.status = statusFromString(object.value(QStringLiteral("status")).toString());
    profile.signature = object.value(QStringLiteral("signature")).toString();
    profile.region = object.value(QStringLiteral("region")).toString();
    return profile;
}

QJsonObject profileToJson(const CurrentUserProfile& profile)
{
    return {
            {QStringLiteral("userId"), profile.userId},
            {QStringLiteral("nickName"), profile.nickName},
            {QStringLiteral("avatarPath"), profile.avatarPath},
            {QStringLiteral("status"), statusToString(profile.status)},
            {QStringLiteral("signature"), profile.signature},
            {QStringLiteral("region"), profile.region}
    };
}

class CurrentUserIdentityRequestOperation final
    : public RepositoryTemplate<CurrentUserIdentityRequest, CurrentUserProfile> {
public:
    explicit CurrentUserIdentityRequestOperation(QMap<QString, CurrentUserProfile> profiles)
        : m_profiles(std::move(profiles))
    {
    }

private:
    CurrentUserProfile doRequest(const CurrentUserIdentityRequest& query) const override
    {
        return identityOnly(m_profiles.value(query.userId, fallbackProfile(query.userId)));
    }

    QMap<QString, CurrentUserProfile> m_profiles;
};

class CurrentUserProfileRequestOperation final
    : public RepositoryTemplate<CurrentUserProfileRequest, CurrentUserProfile> {
public:
    explicit CurrentUserProfileRequestOperation(QMap<QString, CurrentUserProfile> profiles)
        : m_profiles(std::move(profiles))
    {
    }

private:
    CurrentUserProfile doRequest(const CurrentUserProfileRequest& query) const override
    {
        return m_profiles.value(query.userId, fallbackProfile(query.userId));
    }

    QMap<QString, CurrentUserProfile> m_profiles;
};

} // namespace

CurrentUserProfileRepository::CurrentUserProfileRepository(QObject* parent)
    : QObject(parent)
{
    LocalDataStore& store = LocalDataStore::instance();
    if (!store.hasDomain(QStringLiteral("current_profiles"))) {
        const QJsonArray profiles = store.seedArray(QStringLiteral(":/resources/data/current_profiles.json"));
        for (const QJsonValue& value : profiles) {
            const CurrentUserProfile profile = profileFromJson(value.toObject());
            if (!profile.userId.isEmpty()) {
                store.upsertValue(QStringLiteral("current_profiles"), profile.userId, profileToJson(profile));
            }
        }
    }

    for (const QJsonObject& object : store.values(QStringLiteral("current_profiles"))) {
        const CurrentUserProfile profile = profileFromJson(object);
        if (!profile.userId.isEmpty()) {
            m_profiles.insert(profile.userId, profile);
        }
    }
}

CurrentUserProfileRepository& CurrentUserProfileRepository::instance()
{
    static CurrentUserProfileRepository repo;
    return repo;
}

CurrentUserProfile CurrentUserProfileRepository::requestCurrentUserIdentity(
        const CurrentUserIdentityRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return CurrentUserIdentityRequestOperation(m_profiles).request(query);
}

CurrentUserProfile CurrentUserProfileRepository::requestCurrentUserProfile(
        const CurrentUserProfileRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return CurrentUserProfileRequestOperation(m_profiles).request(query);
}

void CurrentUserProfileRepository::saveCurrentUserProfile(const CurrentUserProfile& profile)
{
    if (profile.userId.isEmpty()) {
        return;
    }

    bool changed = false;
    QString oldAvatarPath;
    {
        QMutexLocker locker(&m_mutex);
        const CurrentUserProfile previous = m_profiles.value(profile.userId);
        oldAvatarPath = previous.avatarPath;
        changed = previous.userId != profile.userId
                || previous.nickName != profile.nickName
                || previous.avatarPath != profile.avatarPath
                || previous.status != profile.status
                || previous.signature != profile.signature
                || previous.region != profile.region;
        m_profiles.insert(profile.userId, profile);
    }

    LocalDataStore::instance().upsertValue(QStringLiteral("current_profiles"),
                                           profile.userId,
                                           profileToJson(profile));

    if (!oldAvatarPath.isEmpty() && oldAvatarPath != profile.avatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
    if (changed) {
        emit currentUserProfileChanged(profile.userId);
    }
}
