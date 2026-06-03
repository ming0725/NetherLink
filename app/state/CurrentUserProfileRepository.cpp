#include "CurrentUserProfileRepository.h"

#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/services/ImageService.h"

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
    if (normalized == QStringLiteral("active") || normalized == QStringLiteral("online")) {
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

CurrentUserProfile profileFromJson(QJsonObject object, const QString& responseEtag = {})
{
    if (object.value(QStringLiteral("user")).isObject()) {
        object = object.value(QStringLiteral("user")).toObject();
    }

    CurrentUserProfile profile;
    profile.userUuid = object.value(QStringLiteral("userUuid")).toString();
    profile.userId = object.value(QStringLiteral("userId")).toString(
            object.value(QStringLiteral("id")).toString(profile.userUuid));
    profile.nickName = object.value(QStringLiteral("nickName")).toString(
            object.value(QStringLiteral("nick")).toString(object.value(QStringLiteral("displayName")).toString()));
    profile.avatarPath = object.value(QStringLiteral("avatarPath")).toString(
            object.value(QStringLiteral("avatarUrl")).toString(QString::fromLatin1(kDefaultAvatarPath)));
    profile.status = statusFromString(object.value(QStringLiteral("status")).toString());
    profile.signature = object.value(QStringLiteral("signature")).toString();
    profile.region = object.value(QStringLiteral("region")).toString();
    profile.version = object.value(QStringLiteral("version")).toInt();
    profile.etag = object.value(QStringLiteral("etag")).toString(responseEtag);
    if (profile.etag.isEmpty()) {
        profile.etag = responseEtag;
    }
    return profile;
}

QJsonObject profileToJson(const CurrentUserProfile& profile)
{
    QJsonObject object{
            {QStringLiteral("userUuid"), profile.userUuid},
            {QStringLiteral("userId"), profile.userId},
            {QStringLiteral("nickName"), profile.nickName},
            {QStringLiteral("avatarPath"), profile.avatarPath},
            {QStringLiteral("status"), statusToString(profile.status)},
            {QStringLiteral("signature"), profile.signature},
            {QStringLiteral("region"), profile.region},
            {QStringLiteral("version"), profile.version}
    };
    if (!profile.etag.isEmpty()) {
        object.insert(QStringLiteral("etag"), profile.etag);
    }
    return object;
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
        changed = previous.userUuid != profile.userUuid
                || previous.userId != profile.userId
                || previous.nickName != profile.nickName
                || previous.avatarPath != profile.avatarPath
                || previous.status != profile.status
                || previous.signature != profile.signature
                || previous.region != profile.region
                || previous.version != profile.version
                || previous.etag != profile.etag;
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

void CurrentUserProfileRepository::saveCurrentUserProfileObject(const QJsonObject& object,
                                                               const QString& responseEtag)
{
    saveCurrentUserProfile(profileFromJson(object, responseEtag));
}
