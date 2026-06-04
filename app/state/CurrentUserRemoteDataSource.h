#pragma once

#include "CurrentUserPreferences.h"
#include "CurrentUserProfile.h"
#include "shared/network/NetworkTypes.h"

#include <QHash>
#include <QObject>
#include <QString>

class CurrentUserRemoteDataSource final : public QObject
{
    Q_OBJECT

public:
    static CurrentUserRemoteDataSource& instance();

    QString fetchProfile();
    QString updateProfile(const CurrentUserProfile& profile);
    QString uploadAvatar(const CurrentUserProfile& profile);
    QString fetchPreferences();
    QString updatePreferences(const CurrentUserPreferences& preferences);

signals:
    void profileFetched(const QString& requestId, const CurrentUserProfile& profile);
    void profileFetchFailed(const QString& requestId, const NetworkError& error);
    void profileUpdated(const QString& requestId, const CurrentUserProfile& profile);
    void profileUpdateFailed(const QString& requestId, const NetworkError& error);
    void avatarUpdated(const QString& requestId, const CurrentUserProfile& profile);
    void avatarUpdateFailed(const QString& requestId, const NetworkError& error);
    void preferencesFetched(const QString& requestId, const CurrentUserPreferences& preferences);
    void preferencesFetchFailed(const QString& requestId, const NetworkError& error);
    void preferencesUpdated(const QString& requestId, const CurrentUserPreferences& preferences);
    void preferencesUpdateFailed(const QString& requestId, const NetworkError& error);

private:
    enum class RequestKind {
        FetchProfile,
        UpdateProfile,
        UploadAvatar,
        FetchPreferences,
        UpdatePreferences
    };

    explicit CurrentUserRemoteDataSource(QObject* parent = nullptr);
    Q_DISABLE_COPY(CurrentUserRemoteDataSource)

    void handleRequestSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleRequestFailed(const QString& requestId, const NetworkError& error);

    QHash<QString, RequestKind> m_pendingRequests;
    QHash<QString, CurrentUserProfile> m_pendingAvatarProfiles;
};

Q_DECLARE_METATYPE(CurrentUserProfile)
Q_DECLARE_METATYPE(CurrentUserPreferences)
