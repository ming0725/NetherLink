#pragma once

#include <QMetaObject>

#include "PostMasonryView.h"
#include "shared/types/RepositoryTypes.h"

class PostCardDelegate;
class PostFeedModel;
class PostSessionController;
class QTimer;

class PostFeedPage : public PostMasonryView
{
    Q_OBJECT

public:
    explicit PostFeedPage(QWidget* parent = nullptr);
    void setController(PostSessionController* controller);
    void ensureInitialized();
    void switchFeedMode(bool followOnly);
    void setPosts(const QVector<PostSummary>& posts);

signals:
    void postClicked(const QString& postId);
    void postClickedWithGeometry(const PostSummary& summary, QRect globalGeometry);

private slots:
    void loadMore();
    void onPostActivated(const PostSummary& summary, const QRect& globalGeometry);
    void onPostLikeRequested(const QString& postId, bool liked);
    void onRepositoryPostUpdated(const PostSummary& summary);

private:
    void scheduleLoadMore();
    void loadMore(qint64 loadingStartedAt, int generation);
    void onFeedPageLoaded(const QString& requestId,
                          int offset,
                          int limit,
                          bool followOnly,
                          const QVector<PostSummary>& posts,
                          bool hasMore);
    void clearFeedData();
    void reloadCurrentFeed();
    void showInitialLoadingPlaceholders();
    int loadingPlaceholderCountForViewport() const;
    void stopLoadingAnimation();

    PostFeedModel* m_model;
    PostCardDelegate* m_delegate;
    QTimer* m_loadingAnimationTimer;
    PostSessionController* m_controller = nullptr;
    QMetaObject::Connection m_postUpdatedConnection;
    QMetaObject::Connection m_feedLoadedConnection;
    QString m_feedRequestId;
    qint64 m_feedLoadingStartedAt = 0;
    int m_feedRequestGeneration = 0;
    int m_nextOffset = 0;
    int m_loadGeneration = 0;
    bool m_hasMore = true;
    bool m_initialized = false;
    bool m_loading = false;
    bool m_loadMoreScheduled = false;
    bool m_followOnly = false;
};
