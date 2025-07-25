/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_TRANSPARENT_MENU
#define INCLUDE_COMPONENTS_TRANSPARENT_MENU

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

#endif /* INCLUDE_COMPONENTS_TRANSPARENT_MENU */
