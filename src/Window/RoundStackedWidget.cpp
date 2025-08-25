/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPainter>
#include <QPainterPath>

#include "Window/RoundStackedWidget.hpp"

// #include "ui_RoundStackedWidget.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Window {
/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
    RoundStackedWidget::RoundStackedWidget(QWidget* parent) : QStackedWidget(parent) {
        // , ui(new Ui::RoundStackWidget)
        // ui->setupUi(this);
        // setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(true);
    }

    RoundStackedWidget::~RoundStackedWidget() {
        // delete ui;
    }

    void RoundStackedWidget::paintEvent(QPaintEvent*) {
        QPainter p(this);

        p.setRenderHint(QPainter::Antialiasing);

        // 当前调色板底色
        p.setBrush(palette().base());
        p.setPen(Qt::NoPen);

        QRectF r = rect();
        QPainterPath path;

        // 左/上/右 都是直角，仅右下角 20px 圆角
        path.moveTo(r.left(), r.top());
        path.lineTo(r.right(), r.top()); // 上边
        path.lineTo(r.right(), r.bottom() - 20); // 右边直到圆角起点
        path.arcTo(r.right() - 20, r.bottom() - 20, 20, 20, 0, 90); // 右下角 90° 圆弧
        path.lineTo(r.left(), r.bottom()); // 底边
        path.lineTo(r.left(), r.top()); // 左边
        p.fillPath(path, palette().base());
    }
}
