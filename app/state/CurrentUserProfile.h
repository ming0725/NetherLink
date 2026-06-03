#pragma once

#include <QString>

#include "shared/types/User.h"

struct CurrentUserProfile {
    QString userUuid;
    QString userId;
    QString nickName;
    QString avatarPath;
    UserStatus status = Offline;
    QString signature;
    QString region;
    int version = 0;
    QString etag;

    bool isValid() const { return !userId.isEmpty(); }
};
