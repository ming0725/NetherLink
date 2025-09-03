/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_WINDOW_NAV_BAR

#define INCLUDE_WINDOW_NAV_BAR

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPushButton>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class NavBar;
} // namespace Ui

namespace Window {

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class NavBar : public QWidget {
        Q_OBJECT

        public:
            explicit NavBar(QWidget* parent = nullptr);

            ~NavBar();

        protected:
            void paintEvent(QPaintEvent*)override;

        private:
            void setActiveButton(QPushButton* activeBtn);

        private:
            Ui::NavBar* ui;
            QPushButton* 界面_头像按钮;
            QPushButton* 界面_聊天界面按钮;
            QPushButton* 界面_好友列表按钮;
            QPushButton* 界面_朋友圈按钮;
            QPushButton* 界面_人工智能按钮;

        private:
            QList <QPushButton*> 成员变量_按钮列表;

        signals:
            void sig_Chat();

            void sig_Friend();

            void sig_Post();

            void sig_AI();
    };
}

#endif /* INCLUDE_WINDOW_NAV_BAR */
