
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QString>

/* enum ------------------------------------------------------------------- 80 // ! ----------------------------- 120 */
enum UserStatus {
    Online = 0,
    Offline,
    Mining,
    Flying
};

/* struct ----------------------------------------------------------------- 80 // ! ----------------------------- 120 */
struct User {
    QString id;
    QString nick;
    QString remark;
    QString avatarPath;
    UserStatus status;
    QString signature;
    bool isDnd = false;
};

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

QString statusText(UserStatus userStatus);

QString statusIconPath(UserStatus userStatus);
