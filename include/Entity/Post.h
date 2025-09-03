/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_ENTITY_POST

#define INCLUDE_ENTITY_POST

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

#endif /* INCLUDE_ENTITY_POST */
