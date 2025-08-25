/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_WINDOW_MAIN_WINDOW

#define INCLUDE_WINDOW_MAIN_WINDOW

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Window/NavBar.hpp"
#include "Window/RoundStackedWidget.hpp"

#include <QPushButton>
#include <QSplitter>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class MainWindow;
} // namespace Ui

namespace Window {

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class MainWindow : public QWidget {
        Q_OBJECT

        public:
            explicit MainWindow(QWidget* parent = nullptr);

            ~MainWindow();

            static QWidget* getInstance();

        protected:
            void paintEvent(QPaintEvent* event) override;

            void mousePressEvent(QMouseEvent* 形参_鼠标事件)override;

            void mouseMoveEvent(QMouseEvent* 形参_鼠标事件)override;

        // void resizeEvent(QResizeEvent* event) override;

        private:
            Ui::MainWindow* ui;
            static QWidget* instance;
            QPushButton* 界面_关闭按钮;
            NavBar* 界面_导航栏;
            RoundStackedWidget* 界面_容器;
            QWidget* rightContent;
            int contentFixedWidth;
            QSplitter* splitter;
            QPushButton* btnMinimize;
            QPushButton* btnMaximize;
            QPushButton* btnClose;
            QStackedWidget* stack;

        private:
            QPoint 成员变量_鼠标偏移量;
            enum PageIndex {
                PAGE_MESSAGE = 0,
                PAGE_FRIEND = 1,
                PAGE_POST = 2,
                PAGE_AI = 3,
                PAGE_DEFAULT = 4,
            };

            // QIcon iconClose, iconCloseHover;
    };
}

#endif /* INCLUDE_WINDOW_MAIN_WINDOW */
