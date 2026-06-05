#pragma once

#include <QDateTime>
#include <QSize>
#include <QString>
#include <QVector>

struct Post {
    QString postID;
    QString title;
    QString content;
    int likes;
    int commentCount = 0;
    QString authorID;
    QString authorName;
    QString authorAvatarPath;
    QDateTime createdAt;
    QDateTime contentCreatedAt;
    QString thumbnailPath;
    QSize thumbnailSize;
    QVector<QString> picturesPath;
    bool isLiked = false;
    bool isFollowedAuthor = false;
};
