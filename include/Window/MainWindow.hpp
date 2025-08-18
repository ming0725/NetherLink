/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_WINDOW_MAIN_WINDOW

#define INCLUDE_WINDOW_MAIN_WINDOW

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>

#include "View/Mainwindow/ApplicationBar.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class MainWindow;
} // namespace Ui

namespace Window {

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class MainWindow : public QMainWindow {
        Q_OBJECT

        public:
            explicit MainWindow(QWidget* parent = nullptr);

            ~MainWindow();

            static QWidget* getInstance();

        protected:
            void resizeEvent(QResizeEvent* event) override;

            void mousePressEvent(QMouseEvent* event) override;

        // bool eventFilter(QObject* watched, QEvent* ev);

        private slots:
            void onBarItemClicked(ApplicationBarItem* item);

        private:
            Ui::MainWindow* ui;
            static QWidget* instance;
            ApplicationBar* appBar;
            QWidget* rightContent;
            int contentFixedWidth;
            QSplitter* splitter;
            QPushButton* btnMinimize;
            QPushButton* btnMaximize;
            QPushButton* btnClose;
            QStackedWidget* stack;

            // QIcon iconClose, iconCloseHover;
    };
}

#endif /* INCLUDE_WINDOW_MAIN_WINDOW */
