/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_POST_POST_PREVIEW_ITEM

#define INCLUDE_VIEW_POST_POST_PREVIEW_ITEM

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Components/ClickableLabel.h"
#include "Data/AvatarLoader.h"
#include "Entity/Post.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class PostPreviewItem : public QWidget {
    Q_OBJECT

    public:
        explicit PostPreviewItem(const Post& post, QWidget* parent = nullptr);

        int scaledHeightFor(double itemW);

        // Getters
        QString getAuthorName() const {
            return (m_authorName);
        }

        QString getAuthorID() const {
            return (m_authorID);
        }

        QString getTitle() const {
            return (m_titleText);
        }

        int getLikeCount() const {
            return (m_likeCount);
        }

        bool isLiked() const {
            return (m_liked);
        }

        QString getAuthorAvatarUrl() const {
            return (m_authorAvatarUrl);
        }

        QString getPostImageUrl() const {
            return (m_postImageUrl);
        }

        QPixmap getOriginalImage() {
            return (m_originalImage);
        }

        // Setters
        void setAuthorName(const QString& name) {
            m_authorName = name;
        }

        void setAuthorID(const QString& id) {
            m_authorID = id;
        }

        void setTitle(const QString& title) {
            m_titleText = title;
        }

        void setLikeCount(int count) {
            m_likeCount = count;
        }

        void setLiked(bool liked) {
            m_liked = liked;
        }

        void setAuthorAvatarUrl(const QString& url) {
            m_authorAvatarUrl = url;
        }

        void setPostImageUrl(const QString& url) {
            m_postImageUrl = url;
        }

    signals:
        void viewPost(QString postID);

        void viewAuthor();

        void loadFinished();

        void viewPostWithGeometry(QString postID, QRect globalGeometry, QPixmap originalImage);

    private slots:
        void onViewPost();

        void onViewAuthor();

        void onClickLike();

    protected:
        void resizeEvent(QResizeEvent* ev) Q_DECL_OVERRIDE;

        void showEvent(QShowEvent* ev) Q_DECL_OVERRIDE;

    private:
        void setupUI();

        void downloadAndSetupImages();

        QPixmap m_croppedPostImage; // setupUI 裁剪后的基准图
        QPixmap m_originalImage;
        QString m_authorName;
        QString m_titleText;
        int m_likeCount;
        bool m_liked;
        QString m_authorID;
        QString m_authorAvatarUrl;
        QString m_postImageUrl;
        QString m_postID;

        // UI 元素
        ClickableLabel* m_imageLabel;
        ClickableLabel* m_titleLabel;
        AvatarLabel* m_avatarLabel;
        ClickableLabel* m_authorLabel;
        ClickableLabel* m_likeIconLabel;
        ClickableLabel* m_likeCountLabel;

        // 网络相关
        QNetworkAccessManager m_networkManager;

        // 参数
        static constexpr int MinWidth = 200;
        static constexpr int MaxImgH = 300;
        static constexpr int Margin = 6;
        static constexpr int AvatarR = 30; // 头像直径
};

#endif /* INCLUDE_VIEW_POST_POST_PREVIEW_ITEM */
