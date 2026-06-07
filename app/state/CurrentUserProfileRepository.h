#pragma once

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QJsonObject>
#include <QString>

#include "CurrentUserProfile.h"

struct CurrentUserIdentityRequest {
    QString userId;
};

struct CurrentUserProfileRequest {
    QString userId;
};

class CurrentUserProfileRepository : public QObject {
    Q_OBJECT
public:
    static CurrentUserProfileRepository& instance();

    CurrentUserProfile requestCurrentUserIdentity(const CurrentUserIdentityRequest& query) const;
    CurrentUserProfile requestCurrentUserProfile(const CurrentUserProfileRequest& query) const;
    void saveCurrentUserProfile(const CurrentUserProfile& profile);
    void saveCurrentUserProfileObject(const QJsonObject& object, const QString& responseEtag = {});
    bool updatePresence(const QString& userUuid, const QString& status, const QString& lastSeenAt = {});

signals:
    void currentUserProfileChanged(const QString& userId);

private:
    explicit CurrentUserProfileRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(CurrentUserProfileRepository)
    void reloadFromStore();

    QMap<QString, CurrentUserProfile> m_profiles;
    mutable QMutex m_mutex;
};
