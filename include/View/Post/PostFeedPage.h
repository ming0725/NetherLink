/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_POST_POST_FEED_PAGE
#define INCLUDE_VIEW_POST_POST_FEED_PAGE


/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QVector>
#include <QWidget>

#include "Components/CustomScrollArea.h"
#include "View/Post/PostPreviewItem.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class Post;

class PostFeedPage : public CustomScrollArea {
    Q_OBJECT

    public:
        explicit PostFeedPage(QWidget* parent = nullptr);

        // 设置数据源
        void setPosts(const QVector <Post>& posts);

        // 重新加载数据
        void reloadData();

    signals:
        void loadMore();

        void postClicked(QString postID);

        void postClickedWithGeometry(QString postID, QRect globalGeometry, QPixmap originalImage);

    protected:
        void layoutContent() Q_DECL_OVERRIDE;

        void showEvent(QShowEvent*event) Q_DECL_OVERRIDE;

    private:
        void loadPosts();

        QVector <Post>           m_data;
        QVector <PostPreviewItem*>   m_items;

        // 布局参数
        const int margin = 16;
        const int topMargin = 2;
        const int hgap = 12;
        const int vgap = 12;
        const int minItemW = 200; // 最小宽度
        bool m_isFirstShow = true; // 是否是第一次显示
        bool m_needReload = false; // 是否需要重新加载
};


#endif /* INCLUDE_VIEW_POST_POST_FEED_PAGE */
