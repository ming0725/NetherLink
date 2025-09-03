/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPainter>
#include <QPainterPath>
#include <QStyle>

#include "ui_NavBar.h"
#include "Util/RoundedPixmap.hpp"
#include "Window/NavBar.hpp"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Window {
/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
    NavBar::NavBar(QWidget* parent) : QWidget(parent), ui(new Ui::NavBar) {
        ui->setupUi(this);

        // setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(true);
        界面_头像按钮 = ui->btn_profile;
        界面_聊天界面按钮 = ui->btn_Chat;
        界面_好友列表按钮 = ui->btn_Friend;
        界面_朋友圈按钮 = ui->btn_Post;
        界面_人工智能按钮 = ui->btn_AI;
        成员变量_按钮列表 << 界面_聊天界面按钮 << 界面_好友列表按钮 << 界面_朋友圈按钮 << 界面_人工智能按钮;

        QPixmap 默认头像(":/icon/icon.png");
        QPixmap 圆角头像 = Util::RoundedPixmap::函数_圆角头像(默认头像, 60);

        界面_头像按钮->setIcon(圆角头像);
        connect(界面_聊天界面按钮, &QPushButton::clicked, this, [=, this]() {
            setActiveButton(界面_聊天界面按钮);
            emit sig_Chat();
        });
        connect(界面_好友列表按钮, &QPushButton::clicked, this, [=, this]() {
            setActiveButton(界面_好友列表按钮);
            emit sig_Friend();
        });
        connect(界面_朋友圈按钮, &QPushButton::clicked, this, [=, this]() {
            setActiveButton(界面_朋友圈按钮);
            emit sig_Post();
        });
        connect(界面_人工智能按钮, &QPushButton::clicked, this, [=, this]() {
            setActiveButton(界面_人工智能按钮);
            emit sig_AI();
        });
    }

    NavBar::~NavBar() {
        delete ui;
    }

    void NavBar::paintEvent(QPaintEvent*) {
        QPainter p(this);

        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        /* 1. 构造高亮条路径（左侧圆角，右侧直线） */
        const int barWidth = 55;
        const int radius = 20;
        const int h = height();
        QPainterPath path;

        path.moveTo(barWidth, 0);
        path.lineTo(radius, 0);
        path.arcTo(0, 0, radius * 2, radius * 2, 90, 90);
        path.lineTo(0, h - radius);
        path.arcTo(0, h - radius * 2, radius * 2, radius * 2, 180, 90);
        path.lineTo(barWidth, h);
        path.closeSubpath();

        /* 2. 先刷底色 */
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x96, 0xaa, 0xb4));
        p.drawPath(path);
    }

    void NavBar::setActiveButton(QPushButton* activeBtn) {
        for (QPushButton* btn : 成员变量_按钮列表) {
            btn->setProperty("active", false);   // 方便 qss 判断
            btn->style()->unpolish(btn); // 立即刷新样式
            btn->style()->polish(btn);
        }
        activeBtn->setProperty("active", true);
        activeBtn->style()->unpolish(activeBtn);
        activeBtn->style()->polish(activeBtn);
    }
}
