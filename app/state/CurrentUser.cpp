#include "CurrentUser.h"

#include "CurrentUserProfileRepository.h"
#include "CurrentUserRemoteDataSource.h"
#include "shared/network/RealtimeClient.h"

#include <QFileInfo>

namespace {

bool editableProfileFieldsChanged(const CurrentUserProfile& next, const CurrentUserProfile& previous)
{
    return next.nickName != previous.nickName ||
           next.signature != previous.signature ||
           next.region != previous.region;
}

CurrentUserProfile profilePatchAfterAvatarUpload(const CurrentUserProfile& editedProfile,
                                                 CurrentUserProfile uploadedProfile)
{
    if (uploadedProfile.userId.isEmpty()) {
        uploadedProfile.userId = editedProfile.userId;
    }
    if (uploadedProfile.userUuid.isEmpty()) {
        uploadedProfile.userUuid = editedProfile.userUuid;
    }
    uploadedProfile.nickName = editedProfile.nickName;
    uploadedProfile.signature = editedProfile.signature;
    uploadedProfile.region = editedProfile.region;

    if (uploadedProfile.version <= editedProfile.version && !uploadedProfile.etag.isEmpty()) {
        uploadedProfile.version = 0;
    }
    return uploadedProfile;
}

} // namespace

CurrentUser::CurrentUser(QObject* parent)
    : QObject(parent)
{
    connect(&CurrentUserProfileRepository::instance(),
            &CurrentUserProfileRepository::currentUserProfileChanged,
            this,
            [this](const QString& userId) {
                if (isCurrentUserId(userId)) {
                    if (m_profileLoadLevel == ProfileLoadLevel::Full) {
                        refreshProfile();
                    } else {
                        refreshIdentity();
                    }
                }
            });
    connect(&CurrentUserRemoteDataSource::instance(),
            &CurrentUserRemoteDataSource::profileUpdated,
            this,
            [this](const QString& requestId, const CurrentUserProfile& profile) {
                if (!m_pendingProfileSaveRequests.remove(requestId)) {
                    return;
                }
                if (isCurrentUserId(profile.userId)) {
                    applyProfile(profile, ProfileLoadLevel::Full);
                }
                emit profileSaveSucceeded(requestId);
            });
    connect(&CurrentUserRemoteDataSource::instance(),
            &CurrentUserRemoteDataSource::profileUpdateFailed,
            this,
            [this](const QString& requestId, const NetworkError& error) {
                if (m_pendingProfileSaveRequests.remove(requestId)) {
                    emit profileSaveFailed(requestId, error);
                }
            });
    connect(&CurrentUserRemoteDataSource::instance(),
            &CurrentUserRemoteDataSource::avatarUpdated,
            this,
            [this](const QString& requestId, const CurrentUserProfile& profile) {
                if (!m_pendingProfileSaveRequests.remove(requestId)) {
                    return;
                }
                if (isCurrentUserId(profile.userId)) {
                    applyProfile(profile, ProfileLoadLevel::Full);
                }
                const CurrentUserProfile editedProfile = m_pendingProfileUpdatesAfterAvatar.take(requestId);
                if (editedProfile.isValid()) {
                    const QString profileRequestId = CurrentUserRemoteDataSource::instance().updateProfile(
                            profilePatchAfterAvatarUpload(editedProfile, profile));
                    if (!profileRequestId.isEmpty()) {
                        m_pendingProfileSaveRequests.insert(profileRequestId);
                        return;
                    }
                    NetworkError error;
                    error.code = QStringLiteral("PROFILE_UPDATE_NOT_STARTED");
                    error.message = QStringLiteral("Profile update was not started after avatar upload.");
                    emit profileSaveFailed(requestId, error);
                    return;
                }
                emit profileSaveSucceeded(requestId);
            });
    connect(&CurrentUserRemoteDataSource::instance(),
            &CurrentUserRemoteDataSource::avatarUpdateFailed,
            this,
            [this](const QString& requestId, const NetworkError& error) {
                m_pendingProfileUpdatesAfterAvatar.remove(requestId);
                if (m_pendingProfileSaveRequests.remove(requestId)) {
                    emit profileSaveFailed(requestId, error);
                }
            });
    connect(&RealtimeClient::instance(),
            &RealtimeClient::eventReceived,
            this,
            [this](const RealtimeEvent& event) {
                if (event.type == QStringLiteral("realtime.ready") && !m_userId.isEmpty()) {
                    CurrentUserRemoteDataSource::instance().fetchProfile();
                }
            });
}

CurrentUser& CurrentUser::instance()
{
    static CurrentUser instance;
    return instance;
}

void CurrentUser::setUserInfo(const QString& userId, const QString& userName, const QString& avatarPath)
{
    m_userId = userId;
    m_profile = {};
    m_profileLoadLevel = ProfileLoadLevel::None;

    if (m_userId.isEmpty()) {
        emit identityChanged();
        emit profileChanged();
        return;
    }

    refreshIdentity();
    CurrentUserRemoteDataSource::instance().fetchProfile();
    CurrentUserRemoteDataSource::instance().fetchPreferences();
    if (!userName.isEmpty()) {
        m_profile.nickName = userName;
    }
    if (!avatarPath.isEmpty()) {
        m_profile.avatarPath = avatarPath;
    }
    if (!userName.isEmpty() || !avatarPath.isEmpty()) {
        refreshProfile();
        if (!userName.isEmpty()) {
            m_profile.nickName = userName;
        }
        if (!avatarPath.isEmpty()) {
            m_profile.avatarPath = avatarPath;
        }
        CurrentUserProfileRepository::instance().saveCurrentUserProfile(m_profile);
        emit identityChanged();
        emit profileChanged();
    }
}

QString CurrentUser::getUserName() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile.nickName;
}

QString CurrentUser::getAvatarPath() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile.avatarPath;
}

UserStatus CurrentUser::getStatus() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile.status;
}

QString CurrentUser::getSignature() const
{
    ensureProfileLoaded(ProfileLoadLevel::Full);
    return m_profile.signature;
}

QString CurrentUser::getRegion() const
{
    ensureProfileLoaded(ProfileLoadLevel::Full);
    return m_profile.region;
}

CurrentUserProfile CurrentUser::identity() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile;
}

CurrentUserProfile CurrentUser::profile() const
{
    ensureProfileLoaded(ProfileLoadLevel::Full);
    return m_profile;
}

bool CurrentUser::isCurrentUserId(const QString& userId) const
{
    if (userId.isEmpty() || m_userId.isEmpty()) {
        return false;
    }
    if (userId == m_userId) {
        return true;
    }
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return (!m_profile.userUuid.isEmpty() && userId == m_profile.userUuid) ||
           (!m_profile.userId.isEmpty() && userId == m_profile.userId);
}

void CurrentUser::setPresence(UserStatus status, const QDateTime& lastSeenAt)
{
    if (m_userId.isEmpty()) {
        return;
    }

    ensureProfileLoaded(ProfileLoadLevel::Identity);
    CurrentUserProfile profile = m_profile;
    if (profile.userId.isEmpty()) {
        profile.userId = m_userId;
    }

    const bool lastSeenChanged = lastSeenAt.isValid() && profile.lastSeenAt != lastSeenAt;
    if (profile.status == status && !lastSeenChanged) {
        return;
    }

    profile.status = status;
    if (lastSeenAt.isValid()) {
        profile.lastSeenAt = lastSeenAt;
    }

    const ProfileLoadLevel level = m_profileLoadLevel == ProfileLoadLevel::None
            ? ProfileLoadLevel::Identity
            : m_profileLoadLevel;
    applyProfile(profile, level);
    CurrentUserProfileRepository::instance().saveCurrentUserProfile(profile);
}

void CurrentUser::refreshIdentity()
{
    if (m_userId.isEmpty()) {
        return;
    }
    applyProfile(CurrentUserProfileRepository::instance().requestCurrentUserIdentity({m_userId}),
                 ProfileLoadLevel::Identity);
}

void CurrentUser::refreshProfile()
{
    if (m_userId.isEmpty()) {
        return;
    }
    applyProfile(CurrentUserProfileRepository::instance().requestCurrentUserProfile({m_userId}),
                 ProfileLoadLevel::Full);
}

QString CurrentUser::saveProfile(const CurrentUserProfile& profile)
{
    if (m_userId.isEmpty() || profile.userId != m_userId) {
        return {};
    }
    QString firstRequestId;
    const bool hasLocalAvatarChange = profile.avatarPath != m_profile.avatarPath &&
            QFileInfo(profile.avatarPath).isFile();
    const bool hasEditableProfileChange = editableProfileFieldsChanged(profile, m_profile);
    if (hasLocalAvatarChange) {
        const QString avatarRequestId = CurrentUserRemoteDataSource::instance().uploadAvatar(profile);
        if (!avatarRequestId.isEmpty()) {
            m_pendingProfileSaveRequests.insert(avatarRequestId);
            if (hasEditableProfileChange) {
                m_pendingProfileUpdatesAfterAvatar.insert(avatarRequestId, profile);
            }
            firstRequestId = avatarRequestId;
        }
    }

    if (hasLocalAvatarChange) {
        return firstRequestId;
    }

    const QString profileRequestId = CurrentUserRemoteDataSource::instance().updateProfile(profile);
    if (!profileRequestId.isEmpty()) {
        m_pendingProfileSaveRequests.insert(profileRequestId);
        if (firstRequestId.isEmpty()) {
            firstRequestId = profileRequestId;
        }
    }
    return firstRequestId;
}

void CurrentUser::clear()
{
    m_userId.clear();
    m_profile = {};
    m_profileLoadLevel = ProfileLoadLevel::None;
    emit identityChanged();
    emit profileChanged();
}

void CurrentUser::ensureProfileLoaded(ProfileLoadLevel level) const
{
    if (m_userId.isEmpty() ||
        m_profileLoadLevel == ProfileLoadLevel::Full ||
        m_profileLoadLevel == level) {
        return;
    }

    CurrentUser* self = const_cast<CurrentUser*>(this);
    if (level == ProfileLoadLevel::Full) {
        self->refreshProfile();
    } else {
        self->refreshIdentity();
    }
}

void CurrentUser::applyProfile(CurrentUserProfile profile, ProfileLoadLevel level)
{
    if (profile.userId.isEmpty()) {
        profile.userId = m_userId;
    }

    ProfileLoadLevel effectiveLevel = level;
    if (level == ProfileLoadLevel::Identity &&
        m_profileLoadLevel == ProfileLoadLevel::Full &&
        m_profile.userId == profile.userId) {
        profile.signature = m_profile.signature;
        profile.region = m_profile.region;
        effectiveLevel = ProfileLoadLevel::Full;
    }

    const bool identityChangedValue = m_profile.userId != profile.userId
            || m_profile.nickName != profile.nickName
            || m_profile.avatarPath != profile.avatarPath
            || m_profile.avatarVersion != profile.avatarVersion
            || m_profile.avatarEtag != profile.avatarEtag
            || m_profile.avatarContentHash != profile.avatarContentHash
            || m_profile.status != profile.status
            || m_profile.lastSeenAt != profile.lastSeenAt;
    const bool fullChangedValue = identityChangedValue
            || m_profile.signature != profile.signature
            || m_profile.region != profile.region
            || m_profileLoadLevel != effectiveLevel;

    m_profile = profile;
    m_profileLoadLevel = effectiveLevel;

    if (identityChangedValue) {
        emit identityChanged();
    }
    if (fullChangedValue) {
        emit profileChanged();
    }
}
