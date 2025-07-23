
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMenu>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class TransparentMenu : public QMenu {
    Q_OBJECT

    public:
        explicit TransparentMenu(QWidget*parent = nullptr);

        void setItemMinHeight(int h);

        int  itemMinHeight() const;

    protected:
        void paintEvent(QPaintEvent*event) override;

        void showEvent(QShowEvent*event) override;

    private:
        int m_itemMinHeight = 40;
};
