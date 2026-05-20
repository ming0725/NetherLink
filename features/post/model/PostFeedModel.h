#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class PostFeedModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PostIdRole = Qt::UserRole + 1,
        TitleRole,
        ThumbnailImageRole,
        ThumbnailSizeRole,
        AuthorIdRole,
        AuthorNameRole,
        AuthorAvatarRole,
        LikeCountRole,
        CommentCountRole,
        IsLikedRole,
        IsLoadingPlaceholderRole,
        LoadingStartedAtRole
    };

    explicit PostFeedModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setPosts(QVector<PostSummary> posts);
    void appendPosts(const QVector<PostSummary>& posts);
    void updatePost(const PostSummary& post);
    void showLoadingPlaceholders(int count);
    bool hasLoadingPlaceholders() const;

    QString postIdAt(const QModelIndex& index) const;
    PostSummary postAt(const QModelIndex& index) const;

private:
    void rebuildPostIndex();
    int indexOfPost(const QString& postId) const;

    QVector<PostSummary> m_posts;
    QHash<QString, int> m_postRows;
    int m_loadingPlaceholderCount = 0;
    qint64 m_loadingStartedAtMs = 0;
};
