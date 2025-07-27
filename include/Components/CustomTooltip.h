/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_CUSTOM_TOOLTIP
#define INCLUDE_COMPONENTS_CUSTOM_TOOLTIP

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QLabel>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class CustomTooltip : public QWidget {
    Q_OBJECT

    public:
        explicit CustomTooltip(QWidget*parent = nullptr);

        void setText(const QString &text);

        void showTooltip(const QPoint &pos);

    protected:
        void paintEvent(QPaintEvent*event) override;

    private:
        QLabel*m_label;
        static const int CORNER_RADIUS = 8;
        static const int PADDING = 8;
};

#endif /* INCLUDE_COMPONENTS_CUSTOM_TOOLTIP */
