#pragma once

#include <QDateTime>
#include <QString>

#include "shared/types/User.h"

struct CurrentUserProfile {
    QString userUuid;
    QString userId;
    QString nickName;
    QString avatarPath;
    int avatarVersion = 0;
    QString avatarEtag;
    QString avatarContentHash;
    UserStatus status = Offline;
    QDateTime lastSeenAt;
    QString signature;
    QString region;
    int version = 0;
    QString etag;

    bool isValid() const { return !userId.isEmpty(); }
};
