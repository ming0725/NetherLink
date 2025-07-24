// PostApplication.h

/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QStackedWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class PostFeedPage;
class PostApplicationBar;
class Post;
class PostDetailView;
class PostCreatePage;

class PostApplication : public QWidget {
    Q_OBJECT

    public:
        explicit PostApplication(QWidget* parent = nullptr);

    private slots:
        void onPostClickedWithGeometry(const QString& postID, const QRect& sourceGeometry, const QPixmap& originalImage);

    protected:
        void resizeEvent(QResizeEvent* ev) Q_DECL_OVERRIDE;

        void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

        bool eventFilter(QObject* obj, QEvent* ev) Q_DECL_OVERRIDE;

    private:
        void onPageChanged(int index);

        void onPostCreated();

        PostApplicationBar*   m_bar;
        QWidget* m_overlay = nullptr;
        QStackedWidget*       m_stack;
        QImage noiseTexture;
        PostDetailView* m_detailView;
        PostCreatePage* m_createPage;
};
