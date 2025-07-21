
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QString>

/* struct ----------------------------------------------------------------- 80 // ! ----------------------------- 120 */
struct Post {
    QString postID;
    QString title;
    int likes;
    QString authorID;
    QString authorName;
    QString firstImage;
    QString authorAvaUrl;
    bool isLiked = false;
};
