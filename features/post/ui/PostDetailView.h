#pragma once

#include <QDateTime>
#include <QHash>
#include <QMetaObject>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QWidget>

#include "shared/types/RepositoryTypes.h"

class IconLineEdit;
class QLabel;
class QMouseEvent;
class QEvent;
class PostCommentDelegate;
class PostDetailListModel;
class PostDetailListView;
class PostSessionController;
class QPushButton;
class QTimer;
class QVariantAnimation;

class PostDetailView : public QWidget {
    Q_OBJECT
public:
    explicit PostDetailView(QWidget* parent = nullptr);
    void setController(PostSessionController* controller);
    void setPreviewSummary(const PostSummary& summary);
    void setPostData(const PostDetailData& data);
    void updatePostSummary(const PostSummary& summary);
    void setImageVisible(bool visible);
    QWidget* panelWidget() const;
    QSize preferredSize(const QSize& availableBounds) const;
    QRect imageRect() const;
    QRect paintedImageRect() const;
    QRect transitionImageRect() const;
    QPixmap transitionPixmap() const;

signals:
    void closed();
    void followClicked(bool followed);
    void likeClicked(bool liked);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* ev) override;
    void paintEvent(QPaintEvent*) override;
private:
    struct State {
        QString postId;
        QString authorId;
        QString authorName;
        QString authorAvatarPath;
        QString title;
        QString content;
        QDateTime contentCreatedAt;
        QString previewImageSource;
        QSize previewImageSize;
        QVector<QString> imageSources;
        QVector<QSize> imageSizes;
        int currentImageIndex = 0;
        int previousImageIndex = -1;
        int imageSlideDirection = 0;
        qreal imageSlideProgress = 1.0;
        qreal fullImageOpacity = 0.0;
        bool imageVisible = true;
        bool imageHoverActive = false;
        qreal imageHoverProgress = 0.0;
        bool isFollowed = false;
        bool isLiked = false;
        int likeCount = 0;
        int commentCount = 0;
    };

    QRect fittedImageRect(const QRect& bounds, const QSize& imageSize) const;
    QString currentImageSource() const;
    QSize currentImageSize() const;
    QString firstImageSource() const;
    QSize firstImageSize() const;
    int imageCount() const;
    QRect imageCounterRect() const;
    QRect imageArrowRect(bool previous, qreal progress = 1.0) const;
    void updateImageHoverState(const QPoint& pos);
    void setImageHoverActive(bool active);
    void setCurrentImageIndex(int index, int direction);
    void showPreviousImage();
    void showNextImage();
    void preloadNextImage();
    void setupUI();
    void updateLayout();
    void applyTheme();
    void applySummaryState(const PostSummary& summary, bool resetDetailContent);
    void syncUiFromState();
    void syncFollowUi();
    void syncEngagementUi();
    void openPostImageViewer();
    void requestPostImageViewerReplacement();
    void loadInitialComments();
    void loadMoreComments();
    void maybeLoadMoreComments();
    void scheduleMaybeLoadMoreComments();
    void onCommentsReady(const QString& requestId, const PostCommentsPage& page);
    void animateCommentExpansion(const QString& commentId);
    void animateReplyExpansion(const QString& commentId, const QString& replyId);
    void animateMoreReplies(const QString& commentId);
    void stopCommentAnimations();
    void stopImageFadeAnimation();
    void stopImageSlideAnimation();
    void setReplyTarget(const QString& commentId, const QString& replyId = QString());
    void clearReplyTarget();
    void submitCommentText();
    void showLoadingPreview();
    void stopLoadingPreviewAnimation();
private:
    struct ReplyTarget {
        QString commentId;
        QString replyId;
    };

    State m_state;
    ReplyTarget m_replyTarget;
    QLabel* m_authorAvatar;
    QLabel* m_authorName;
    QPushButton* m_followBtn;
    QWidget* m_panelContainer;
    PostDetailListView* m_contentList;
    PostDetailListModel* m_detailModel;
    PostCommentDelegate* m_commentDelegate;
    QTimer* m_loadingAnimationTimer;
    PostSessionController* m_controller = nullptr;
    QMetaObject::Connection m_commentsLoadedConnection;
    QMetaObject::Connection m_commentLikeUpdatedConnection;
    QMetaObject::Connection m_replyLikeUpdatedConnection;
    QMetaObject::Connection m_commentCreatedConnection;
    QMetaObject::Connection m_replyCreatedConnection;
    QPushButton* m_likeBtn;
    QLabel* m_likeCount;
    QPushButton* m_commentBtn;
    QLabel* m_commentCount;
    IconLineEdit* m_commentLineEdit;
    bool m_loadingComments = false;
    bool m_pendingLoadMoreCheck = false;
    QString m_commentsRequestId;
    int m_pendingCommentsOffset = -1;
    int m_commentPageSize = 12;
    QHash<QString, QPointer<QVariantAnimation>> m_commentExpansionAnimations;
    QHash<QString, QPointer<QVariantAnimation>> m_replyExpansionAnimations;
    QHash<QString, QPointer<QVariantAnimation>> m_moreReplyAnimations;
    QPointer<QVariantAnimation> m_imageFadeAnimation;
    QPointer<QVariantAnimation> m_imageHoverAnimation;
    QPointer<QVariantAnimation> m_imageSlideAnimation;
    QPointer<class ImageViewer> m_postImageViewer;
    QString m_postImageViewerPostId;
};
