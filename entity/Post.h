#pragma once

#include <QString>
#include <QVector>

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