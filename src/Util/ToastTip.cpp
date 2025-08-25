/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "Util/ToastTip.hpp"

#include <QPainter>
#include <QPainterPath>

#include <QScreen>

#include <QTimer>

#include "ui_ToastTip.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Util {
/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

    ToastTip::ToastTip(QWidget* parent) : QWidget(parent), ui(new Ui::ToastTip) {
        ui->setupUi(this);
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_StyledBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        animation = new QPropertyAnimation(this, "pos", this);
        animation->setDuration(300);
        animation->setEasingCurve(QEasingCurve::OutCubic);
    }

    ToastTip::~ToastTip() {
        delete ui;
    }

    void ToastTip::paintEvent(QPaintEvent* event) {
        QPainter painter(this);

        painter.setRenderHint(QPainter::Antialiasing);

        QPainterPath path;

        path.addRoundedRect(rect(), 30, 30);
        painter.setPen(Qt::NoPen);
        painter.fillPath(path, QColor(0, 0, 0, 180));
        QWidget::paintEvent(event);
    }

    ToastTip& ToastTip::函数_实例() {
        static ToastTip 实例;

        return (实例);
    }

    void ToastTip::函数_显示消息(枚举_消息类型 形参_消息类型, const QString& 形参_消息内容) {
        成员变量_图片 = (形参_消息类型 == 枚举_消息类型::ENUM_SUCCESS) ? ":/icon/correct.png" : ":/icon/fail.png";
        成员变量_标题 = (形参_消息类型 == 枚举_消息类型::ENUM_SUCCESS) ? "成功" : "错误";
        ui->lbl_title->setText(成员变量_标题);
        ui->lbl_icon->setPixmap(QPixmap(成员变量_图片));
        ui->lbl_message->setText(形参_消息内容);

        if (成员变量_正在显示) {
            // 如果当前正处于显示中，则先执行隐藏动画，再在动画结束时重回本函数
            startAnimation(false);
            QTimer::singleShot(300, this, [this, 形参_消息类型, 形参_消息内容]() {
                this->函数_显示消息(形参_消息类型, 形参_消息内容);
            });

            return;
        }

        QScreen* screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int startY = 0 - height();

        move(x, startY);
        show();
        startAnimation(true);

        // 3s 后自动隐藏
        QTimer::singleShot(3000, this, [=, this]() {
            startAnimation(false);
        });
    }

    /* 顶部居中版本（不变动） */
    void ToastTip::函数_显示消息(QWidget* 形参_目标窗口, 枚举_消息类型 形参_消息类型, const QString& 形参_消息内容) {
        // 如果没有传入 目标窗口，就退回到全屏居中版本
        if (!形参_目标窗口) {
            函数_显示消息(形参_消息类型, 形参_消息内容);

            return;
        }
        成员变量_图片 = (形参_消息类型 == 枚举_消息类型::ENUM_SUCCESS) ? ":/icon/correct.png" : ":/icon/fail.png";
        成员变量_标题 = (形参_消息类型 == 枚举_消息类型::ENUM_SUCCESS) ? "成功" : "错误";
        ui->lbl_title->setText(成员变量_标题);
        ui->lbl_icon->setPixmap(QPixmap(成员变量_图片));
        ui->lbl_message->setText(形参_消息内容);

        // === 动态计算宽度 ===
        // 保持高度固定 40px，但宽度根据文字长度 + 图标 + 左右 margin/spacing 自动调整
        QFontMetrics fm(ui->lbl_message->font());
        int textWidth = fm.horizontalAdvance(形参_消息内容);
        int iconWidth = ui->lbl_icon->width();           // 通常为 20px
        int leftMargin = 10; // 布局左侧 margin
        int rightMargin = 10; // 布局右侧 margin
        int spacing = 10; // icon 与文字之间的间距
        int totalWidth = leftMargin + iconWidth + spacing + textWidth + rightMargin;
        int totalHeight = 60;                     // 保持原来固定高度

        setFixedSize(totalWidth, totalHeight);

        // ======================
        // 如果当前正在显示，则先隐藏后再调用本函数
        if (成员变量_正在显示) {
            // 先让当前动画滑上去隐藏
            animation->stop();

            // 当前位置
            QPoint cur = this->pos();

            // 隐藏到：在目标控件正上方再往上一个高度的位置（使用动态计算后的 height()）
            QPoint hideEnd(cur.x(), cur.y() - height());

            animation->setStartValue(cur);
            animation->setEndValue(hideEnd);
            animation->start();

            // 300ms 后再执行本函数，以播放新一次的显示
            QTimer::singleShot(300, this, [this, 形参_目标窗口, 形参_消息类型, 形参_消息内容]() {
                this->函数_显示消息(形参_目标窗口, 形参_消息类型, 形参_消息内容);
            });

            return;
        }

        // 计算目标控件在全局坐标系中的左上角
        QPoint topLeftGlobal = 形参_目标窗口->mapToGlobal(QPoint(0, 0));
        int widgetTopY = topLeftGlobal.y();

        // “可见位置”（结束动画后的位置）：让通知的顶部偏移 +16px 显示在目标控件上方
        int yVisible = widgetTopY + 16;

        // “完全隐藏位置”：就是通知的 y 坐标等于 widgetTopY（此时刚好在目标控件顶部，完全看不到）
        int yHidden = widgetTopY;

        // x 坐标：在 目标窗口 水平方向居中
        int widgetWidth = 形参_目标窗口->width();
        int x = topLeftGlobal.x() + (widgetWidth - width()) / 2;

        // 先把通知移动到“隐藏位置”，并且 show()（此时完全不可见）
        move(x, yHidden);
        show();

        // 播放“下滑”动画：从 yHidden → yVisible
        animation->stop();
        animation->setStartValue(QPoint(x, yHidden));
        animation->setEndValue(QPoint(x, yVisible));
        成员变量_正在显示 = true;
        animation->start();

        // 1.5s（1500ms）后自动隐藏
        QTimer::singleShot(1500, this, [this, x, yHidden, yVisible]() {
            // 播放“上滑”动画：从 yVisible → yHidden
            animation->stop();
            animation->setStartValue(QPoint(x, yVisible));
            animation->setEndValue(QPoint(x, yHidden));
            animation->start();

            // 隐藏动画结束后真正 hide()，并重置 成员变量_正在显示
            // 动画时长 150ms，因此延迟 150ms 后 hide()
            QTimer::singleShot(250, this, [=, this]() {
                this->hide();
                成员变量_正在显示 = false;
            });
        });
    }

    /***（保持原实现，不改动） * 全屏版的上下滑动逻辑 */
    void ToastTip::startAnimation(bool show) {
        QScreen* screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int startY = show ? (0 - height()) : 20;
        int endY = show ? 20 : (0 - height());

        animation->stop();
        animation->setStartValue(QPoint(x, startY));
        animation->setEndValue(QPoint(x, endY));
        成员变量_正在显示 = show;
        animation->start();
    }
}
