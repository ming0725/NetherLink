
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "FramelessWindow.h"
#include "LineEditComponent.h"

#include "SearchResultList.h"
#include "SearchTypeTab.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class SearchFriendWindow : public FramelessWindow {
    Q_OBJECT

    public:
        explicit SearchFriendWindow(QWidget* parent = nullptr);

        static SearchFriendWindow* getInstance();

    protected:
        void resizeEvent(QResizeEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

        bool eventFilter(QObject* watched, QEvent* ev) override;

    private slots:
        void onSearchTypeChanged(int index);

        void onSearchTextChanged(const QString& text);

    private:
        static SearchFriendWindow* instance;
        QLabel* titleLabel;
        SearchTypeTab* typeTab;
        LineEditComponent* searchBox;
        SearchResultList* resultList;
        QPushButton* btnMinimize;
        QPushButton* btnClose;
        QIcon iconClose, iconCloseHover;

        // 标题栏高度和按钮大小
        const int TITLE_HEIGHT = 40;
        const int BTN_SIZE = 32;
        const int SEARCH_BOX_HEIGHT = 32;
        const int MARGIN = 20;
};
