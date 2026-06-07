#include "CurrentUserProfileRepository.h"

#include "shared/data/LocalDataStore.h"
#include "shared/data/RepositoryTemplate.h"
#include "shared/network/AppEventBus.h"
#include "shared/services/AvatarSource.h"
#include "shared/services/ImageService.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <utility>

namespace {

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QString avatarFileIdFrom(const QJsonObject& object)
{
    const QJsonObject avatar = object.value(QStringLiteral("avatar")).toObject();
    const QString nestedFileId = firstString(avatar, {QStringLiteral("fileId"), QStringLiteral("id")});
    if (!nestedFileId.isEmpty()) {
        return nestedFileId;
    }

    return firstString(object, {QStringLiteral("avatarFileId"),
                                QStringLiteral("avatar_file_id"),
                                QStringLiteral("fileId")});
}

bool hasAvatarPayload(const QJsonObject& object)
{
    return object.contains(QStringLiteral("avatar")) ||
           object.contains(QStringLiteral("avatarPath")) ||
           object.contains(QStringLiteral("avatarUrl")) ||
           object.contains(QStringLiteral("avatarVersion")) ||
           object.contains(QStringLiteral("avatarEtag")) ||
           object.contains(QStringLiteral("avatarContentHash")) ||
           object.contains(QStringLiteral("avatarFileId")) ||
           object.contains(QStringLiteral("avatar_file_id")) ||
           object.contains(QStringLiteral("fileId"));
}

void keepAvatarFromPrevious(CurrentUserProfile& profile, const CurrentUserProfile& previous)
{
    profile.avatarPath = previous.avatarPath;
    profile.avatarVersion = previous.avatarVersion;
    profile.avatarEtag = previous.avatarEtag;
    profile.avatarContentHash = previous.avatarContentHash;
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
        return QStringLiteral("airplane");
    case Invisible:
        return QStringLiteral("invisible");
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
    if (normalized == QStringLiteral("mining") ||
        normalized == QStringLiteral("busy") ||
        normalized == QStringLiteral("dnd")) {
        return Mining;
    }
    if (normalized == QStringLiteral("airplane") ||
        normalized == QStringLiteral("flying") ||
        normalized == QStringLiteral("away")) {
        return Flying;
    }
    if (normalized == QStringLiteral("invisible")) {
        return Invisible;
    }
    return Offline;
}

QJsonObject presenceObjectFrom(const QJsonObject& object)
{
    const QJsonObject presence = object.value(QStringLiteral("presence")).toObject();
    if (!presence.isEmpty()) {
        return presence;
    }

    QJsonObject legacyPresence;
    if (object.contains(QStringLiteral("status"))) {
        legacyPresence.insert(QStringLiteral("status"), object.value(QStringLiteral("status")));
    }
    if (object.contains(QStringLiteral("lastSeenAt"))) {
        legacyPresence.insert(QStringLiteral("lastSeenAt"), object.value(QStringLiteral("lastSeenAt")));
    }
    return legacyPresence;
}

QDateTime dateTimeFromString(const QString& value)
{
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    return dateTime;
}

bool hasPresencePayload(const QJsonObject& object)
{
    return object.contains(QStringLiteral("presence")) ||
           object.contains(QStringLiteral("status")) ||
           object.contains(QStringLiteral("lastSeenAt"));
}

CurrentUserProfile profileFromJson(QJsonObject object, const QString& responseEtag = {})
{
    if (object.value(QStringLiteral("user")).isObject()) {
        object = object.value(QStringLiteral("user")).toObject();
    }

    CurrentUserProfile profile;
    profile.userUuid = firstString(object, {QStringLiteral("userUuid"),
                                            QStringLiteral("uuid")});
    profile.userId = firstString(object, {QStringLiteral("userId"),
                                          QStringLiteral("publicUserId"),
                                          QStringLiteral("accountId")});
    if (profile.userId.isEmpty()) {
        profile.userId = object.value(QStringLiteral("id")).toString(profile.userUuid);
    }
    profile.nickName = object.value(QStringLiteral("nickName")).toString(
            object.value(QStringLiteral("nick")).toString(object.value(QStringLiteral("displayName")).toString()));
    profile.avatarVersion = object.value(QStringLiteral("avatarVersion")).toInt();
    profile.avatarEtag = object.value(QStringLiteral("avatarEtag")).toString();
    profile.avatarContentHash = object.value(QStringLiteral("avatarContentHash")).toString();
    QString avatarPath = AvatarSource::fromAvatarFileId(avatarFileIdFrom(object));
    if (avatarPath.isEmpty()) {
        avatarPath = object.value(QStringLiteral("avatarPath")).toString(
                object.value(QStringLiteral("avatarUrl")).toString());
    }
    profile.avatarPath = AvatarSource::versioned(
            avatarPath,
            profile.avatarVersion,
            profile.avatarEtag,
            profile.avatarContentHash);
    const QJsonObject presence = presenceObjectFrom(object);
    profile.status = statusFromString(presence.value(QStringLiteral("status")).toString());
    profile.lastSeenAt = dateTimeFromString(presence.value(QStringLiteral("lastSeenAt")).toString());
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
            {QStringLiteral("avatarVersion"), profile.avatarVersion},
            {QStringLiteral("avatarEtag"), profile.avatarEtag},
            {QStringLiteral("avatarContentHash"), profile.avatarContentHash},
            {QStringLiteral("status"), statusToString(profile.status)},
            {QStringLiteral("presence"), QJsonObject{
                    {QStringLiteral("status"), statusToString(profile.status)},
                    {QStringLiteral("lastSeenAt"), profile.lastSeenAt.isValid()
                                                   ? profile.lastSeenAt.toUTC().toString(Qt::ISODateWithMs)
                                                   : QString()}
            }},
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
        return identityOnly(m_profiles.value(query.userId));
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
        return m_profiles.value(query.userId);
    }

    QMap<QString, CurrentUserProfile> m_profiles;
};

} // namespace

CurrentUserProfileRepository::CurrentUserProfileRepository(QObject* parent)
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
                if (domain == QStringLiteral("current_profiles")) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);

    connect(&AppEventBus::instance(),
            &AppEventBus::typedEventReceived,
            this,
            [this](const QString& type, const QJsonObject& payload, const RealtimeEvent&) {
        if (type == QStringLiteral("presence.updated")) {
            const QString userUuid = firstString(payload, {QStringLiteral("userUuid"),
                                                           QStringLiteral("uuid"),
                                                           QStringLiteral("userId")});
            updatePresence(userUuid,
                           payload.value(QStringLiteral("status")).toString(),
                           payload.value(QStringLiteral("lastSeenAt")).toString());
            return;
        }

        if (type != QStringLiteral("profile.updated")) {
            return;
        }

        QJsonObject object = payload.value(QStringLiteral("profile")).toObject();
        if (object.isEmpty()) {
            object = payload;
        }

        const QString userId = payload.value(QStringLiteral("userId")).toString();
        const QString userUuid = payload.value(QStringLiteral("userUuid")).toString();
        if (!userId.isEmpty() && !object.contains(QStringLiteral("userId"))) {
            object.insert(QStringLiteral("userId"), userId);
        }
        if (!userUuid.isEmpty() && !object.contains(QStringLiteral("userUuid"))) {
            object.insert(QStringLiteral("userUuid"), userUuid);
        }
        for (const QString& key : {QStringLiteral("avatarUrl"),
                                   QStringLiteral("avatarFileId"),
                                   QStringLiteral("fileId"),
                                   QStringLiteral("avatarVersion"),
                                   QStringLiteral("avatarEtag"),
                                   QStringLiteral("avatarContentHash")}) {
            if (payload.contains(key) && !object.contains(key)) {
                object.insert(key, payload.value(key));
            }
        }
        if (payload.contains(QStringLiteral("avatar")) && !object.contains(QStringLiteral("avatar"))) {
            object.insert(QStringLiteral("avatar"), payload.value(QStringLiteral("avatar")));
        }

        saveCurrentUserProfileObject(object);
    });
}

CurrentUserProfileRepository& CurrentUserProfileRepository::instance()
{
    static CurrentUserProfileRepository repo;
    return repo;
}

void CurrentUserProfileRepository::reloadFromStore()
{
    QMap<QString, CurrentUserProfile> nextProfiles;
    for (const QJsonObject& object : LocalDataStore::instance().values(QStringLiteral("current_profiles"))) {
        const CurrentUserProfile profile = profileFromJson(object);
        if (!profile.userId.isEmpty()) {
            nextProfiles.insert(profile.userId, profile);
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_profiles = nextProfiles;
    }
    for (const QString& userId : nextProfiles.keys()) {
        emit currentUserProfileChanged(userId);
    }
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
    CurrentUserProfile next = profile;
    {
        QMutexLocker locker(&m_mutex);
        const CurrentUserProfile previous = m_profiles.value(profile.userId);
        if (!previous.userId.isEmpty() &&
            previous.avatarVersion > 0 &&
            next.avatarVersion > 0 &&
            next.avatarVersion < previous.avatarVersion) {
            keepAvatarFromPrevious(next, previous);
        }
        if (!previous.userUuid.isEmpty() && next.userUuid.isEmpty()) {
            next.userUuid = previous.userUuid;
        }
        oldAvatarPath = previous.avatarPath;
        changed = previous.userUuid != next.userUuid
                || previous.userId != next.userId
                || previous.nickName != next.nickName
                || previous.avatarPath != next.avatarPath
                || previous.avatarVersion != next.avatarVersion
                || previous.avatarEtag != next.avatarEtag
                || previous.avatarContentHash != next.avatarContentHash
                || previous.status != next.status
                || previous.lastSeenAt != next.lastSeenAt
                || previous.signature != next.signature
                || previous.region != next.region
                || previous.version != next.version
                || previous.etag != next.etag;
        m_profiles.insert(next.userId, next);
    }

    LocalDataStore::instance().upsertValue(QStringLiteral("current_profiles"),
                                           next.userId,
                                           profileToJson(next));

    if (!oldAvatarPath.isEmpty() && oldAvatarPath != next.avatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
    if (changed) {
        emit currentUserProfileChanged(next.userId);
    }
}

void CurrentUserProfileRepository::saveCurrentUserProfileObject(const QJsonObject& object,
                                                               const QString& responseEtag)
{
    CurrentUserProfile profile = profileFromJson(object, responseEtag);
    const CurrentUserProfile previous = requestCurrentUserProfile({profile.userId});
    if (!previous.userId.isEmpty()) {
        if (!hasAvatarPayload(object) ||
            (previous.avatarVersion > 0 &&
             profile.avatarVersion > 0 &&
             profile.avatarVersion < previous.avatarVersion)) {
            keepAvatarFromPrevious(profile, previous);
        }
        if (profile.nickName.isEmpty()) {
            profile.nickName = previous.nickName;
        }
        if (profile.userUuid.isEmpty()) {
            profile.userUuid = previous.userUuid;
        }
        profile.status = hasPresencePayload(object) ? profile.status : previous.status;
        profile.lastSeenAt = hasPresencePayload(object) ? profile.lastSeenAt : previous.lastSeenAt;
        profile.signature = object.contains(QStringLiteral("signature")) ? profile.signature : previous.signature;
        profile.region = object.contains(QStringLiteral("region")) ? profile.region : previous.region;
        profile.version = object.contains(QStringLiteral("version")) ? profile.version : previous.version;
        profile.etag = profile.etag.isEmpty() ? previous.etag : profile.etag;
    }
    saveCurrentUserProfile(profile);
}

bool CurrentUserProfileRepository::updatePresence(const QString& userUuid,
                                                  const QString& status,
                                                  const QString& lastSeenAt)
{
    if (userUuid.isEmpty()) {
        return false;
    }

    const UserStatus nextStatus = statusFromString(status);
    const QDateTime nextLastSeenAt = dateTimeFromString(lastSeenAt);

    bool updated = false;
    QString profileKey;
    CurrentUserProfile profile;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_profiles.find(userUuid);
        if (it == m_profiles.end()) {
            for (auto candidate = m_profiles.begin(); candidate != m_profiles.end(); ++candidate) {
                if (candidate->userId == userUuid ||
                    candidate->userUuid == userUuid) {
                    it = candidate;
                    break;
                }
            }
        }
        if (it == m_profiles.end()) {
            return false;
        }

        updated = it->status != nextStatus ||
                  (nextLastSeenAt.isValid() && it->lastSeenAt != nextLastSeenAt);
        it->status = nextStatus;
        if (nextLastSeenAt.isValid()) {
            it->lastSeenAt = nextLastSeenAt;
        }
        profileKey = it.key();
        profile = *it;
    }

    if (!profileKey.isEmpty()) {
        LocalDataStore::instance().upsertValue(QStringLiteral("current_profiles"),
                                               profileKey,
                                               profileToJson(profile));
    }
    if (updated) {
        emit currentUserProfileChanged(profile.userId);
    }
    return true;
}
