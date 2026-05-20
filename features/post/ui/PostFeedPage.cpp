#include "PostFeedPage.h"

#include <QDateTime>
#include <QTimer>

#include "PostCardDelegate.h"
#include "features/post/model/PostFeedModel.h"
#include "features/post/ui/PostSessionController.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kPageSize = 12;
constexpr int kLoadingAnimationFrameMs = 16;
constexpr int kFeedInitialLoadingMinDurationMs = 750;

} // namespace

PostFeedPage::PostFeedPage(QWidget* parent)
    : PostMasonryView(parent)
    , m_model(new PostFeedModel(this))
    , m_delegate(new PostCardDelegate(this))
    , m_loadingAnimationTimer(new QTimer(this))
{
#ifdef Q_OS_WIN
    setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::WindowBackground));
    setPalette(palette);
#endif
    setModel(m_model);
    setCardDelegate(m_delegate);
    m_loadingAnimationTimer->setInterval(kLoadingAnimationFrameMs);
    connect(m_loadingAnimationTimer, &QTimer::timeout, this, [this]() {
        if (!m_model->hasLoadingPlaceholders()) {
            stopLoadingAnimation();
            return;
        }
        viewport()->update();
    });

    connect(this, &PostMasonryView::reachedBottom, this, &PostFeedPage::scheduleLoadMore);
    connect(this, &PostMasonryView::postActivated,
            this, &PostFeedPage::onPostActivated);
    connect(this, &PostMasonryView::postLikeRequested,
            this, &PostFeedPage::onPostLikeRequested);
}

void PostFeedPage::setController(PostSessionController* controller)
{
    if (m_controller == controller) {
        return;
    }

    if (m_postUpdatedConnection) {
        disconnect(m_postUpdatedConnection);
        m_postUpdatedConnection = {};
    }

    m_controller = controller;
    if (!m_controller) {
        return;
    }

    m_postUpdatedConnection = connect(m_controller, &PostSessionController::postUpdated,
                                      this, &PostFeedPage::onRepositoryPostUpdated);
}

void PostFeedPage::ensureInitialized()
{
    if (m_initialized) {
        return;
    }

    m_initialized = true;
    showInitialLoadingPlaceholders();
    const qint64 loadingStartedAt = QDateTime::currentMSecsSinceEpoch();
    m_loading = true;
    QTimer::singleShot(0, this, [this, loadingStartedAt]() {
        if (!m_model->hasLoadingPlaceholders()) {
            m_loading = false;
            return;
        }
        m_loading = false;
        loadMore(loadingStartedAt);
    });
}

void PostFeedPage::setPosts(const QVector<PostSummary>& posts)
{
    m_initialized = true;
    m_loading = false;
    m_loadMoreScheduled = false;
    stopLoadingAnimation();
    m_model->setPosts(posts);
    m_nextOffset = posts.size();
    m_hasMore = posts.size() >= kPageSize;
}

void PostFeedPage::loadMore()
{
    loadMore(0);
}

void PostFeedPage::loadMore(qint64 loadingStartedAt)
{
    m_loadMoreScheduled = false;
    if (!m_controller || !m_hasMore || m_loading) {
        if (!m_controller && m_model->hasLoadingPlaceholders()) {
            stopLoadingAnimation();
            m_model->setPosts({});
        }
        return;
    }

    m_loading = true;
    const QVector<PostSummary> posts = m_controller->loadFeedPage(m_nextOffset, kPageSize);
    m_loading = false;

    const auto applyPosts = [this, posts]() {
        stopLoadingAnimation();
        if (posts.isEmpty()) {
            m_hasMore = false;
            if (m_model->hasLoadingPlaceholders()) {
                m_model->setPosts({});
            }
            return;
        }

        if (m_nextOffset == 0) {
            m_model->setPosts(posts);
        } else {
            m_model->appendPosts(posts);
        }
        m_nextOffset += posts.size();
        m_hasMore = posts.size() >= kPageSize;
    };

    if (loadingStartedAt <= 0 || !m_model->hasLoadingPlaceholders()) {
        applyPosts();
        return;
    }

    const int elapsed = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - loadingStartedAt);
    const int delay = qMax(0, kFeedInitialLoadingMinDurationMs - elapsed);
    if (delay <= 0) {
        applyPosts();
        return;
    }

    QTimer::singleShot(delay, this, applyPosts);
}

void PostFeedPage::scheduleLoadMore()
{
    if (m_loadMoreScheduled || m_loading || !m_hasMore) {
        return;
    }

    m_loadMoreScheduled = true;
    QTimer::singleShot(100, this, [this]() {
        loadMore();
    });
}

void PostFeedPage::showInitialLoadingPlaceholders()
{
    m_model->showLoadingPlaceholders(loadingPlaceholderCountForViewport());
    if (!m_loadingAnimationTimer->isActive()) {
        m_loadingAnimationTimer->start();
    }
}

int PostFeedPage::loadingPlaceholderCountForViewport() const
{
    constexpr int kViewHorizontalMargin = 16;
    constexpr int kHorizontalGap = 12;
    constexpr int kMinItemWidth = 200;
    constexpr int kEstimatedCardHeight = 250;

    const int availableWidth = viewport()
            ? qMax(0, viewport()->width() - 2 * kViewHorizontalMargin)
            : 0;
    const int columns = qMax(1, (availableWidth + kHorizontalGap) / (kMinItemWidth + kHorizontalGap));
    const int rows = viewport()
            ? qMax(3, (viewport()->height() + kEstimatedCardHeight - 1) / kEstimatedCardHeight + 1)
            : 3;
    return qMax(kPageSize, columns * rows);
}

void PostFeedPage::stopLoadingAnimation()
{
    if (m_loadingAnimationTimer->isActive()) {
        m_loadingAnimationTimer->stop();
    }
}

void PostFeedPage::onPostActivated(const PostSummary& summary, const QRect& globalGeometry)
{
    emit postClicked(summary.postId);
    emit postClickedWithGeometry(summary, globalGeometry);
}

void PostFeedPage::onPostLikeRequested(const QString& postId, bool liked)
{
    if (m_controller) {
        m_controller->setPostLiked(postId, liked);
    }
}

void PostFeedPage::onRepositoryPostUpdated(const PostSummary& summary)
{
    m_model->updatePost(summary);
}
