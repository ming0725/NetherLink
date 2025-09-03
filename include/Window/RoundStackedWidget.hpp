/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_WINDOW_ROUND_STACKED_WIDGET

#define INCLUDE_WINDOW_ROUND_STACKED_WIDGET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QStackedWidget>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// namespace Ui {
// class RoundStackWidget;
// } // namespace Ui
namespace Window {

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class RoundStackedWidget : public QStackedWidget {
        Q_OBJECT

        public:
            explicit RoundStackedWidget(QWidget* parent = nullptr);

            ~RoundStackedWidget();

        protected:
            void paintEvent(QPaintEvent*)override;

        private:
            // Ui::RoundStackWidget *ui;
    };
}

#endif /* INCLUDE_WINDOW_ROUND_STACKED_WIDGET */
