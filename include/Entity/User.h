/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_ENTITY_USER
#define INCLUDE_ENTITY_USER

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QString>
#include <QVariant>

/* enum ------------------------------------------------------------------- 80 // ! ----------------------------- 120 */
enum UserStatus {
    Online = 0,
    Offline,
    Mining,
    Flying
};

Q_DECLARE_METATYPE(UserStatus)

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

// QVariant支持
Q_DECLARE_METATYPE(User)

#endif /* INCLUDE_ENTITY_USER */
