/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_TOP_SEARCH_WIDGET
#define INCLUDE_COMPONENTS_TOP_SEARCH_WIDGET

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPushButton>

#include "Components/LineEditComponent.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class TopSearchWidget : public QWidget {
    Q_OBJECT

    public:
        explicit TopSearchWidget(QWidget*parent = nullptr);

        void resizeEvent(QResizeEvent*event) override;

    protected:
        void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

    private slots:
        void showAddMenu();

    private:
        LineEditComponent*searchBox;
        QPushButton*addButton;

        // 边距设置
        int topMargin = 24;
        int bottomMargin = 12;
        int leftMargin = 14;
        int rightMargin = 14;
        int spacing = 5; // 输入框和按钮之间的间距
};

#endif /* INCLUDE_COMPONENTS_TOP_SEARCH_WIDGET */
