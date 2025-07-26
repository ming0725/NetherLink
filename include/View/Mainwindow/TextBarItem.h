/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_MAINWINDOW_TEXT_BAR_ITEM
#define INCLUDE_VIEW_MAINWINDOW_TEXT_BAR_ITEM

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPainter>
#include <QWidget>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class TextBarItem : public QWidget {
    Q_OBJECT

    public:
        TextBarItem(const QString& text, int idx, QWidget* parent) : QWidget(parent), label(text), index(idx) {
            setAttribute(Qt::WA_Hover);
        }

        QString label;
        int index;
        bool hovered = false;

        void paintEvent(QPaintEvent*) override {
            QPainter p(this);

            p.setRenderHint(QPainter::Antialiasing);

            QColor bg = QColor(0, 0, 0, 0);

            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect(), 10, 10);

            QColor textColor = Qt::black;

            p.setPen(textColor);
            p.drawText(rect(), Qt::AlignCenter, label);
        }

        void mousePressEvent(QMouseEvent*) override {
            emit clicked(index);
        }

        void enterEvent(QEnterEvent*) override {
            hovered = true;
            setCursor(Qt::PointingHandCursor);
        }

        void leaveEvent(QEvent*) override {
            hovered = false;
            unsetCursor();
        }

    signals:
        void clicked(int index);

};

#endif /* INCLUDE_VIEW_MAINWINDOW_TEXT_BAR_ITEM */
