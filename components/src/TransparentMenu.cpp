#include "TransparentMenu.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QScreen>
#include <QApplication>
#include <QStyleOption>
#include <QStyle>
#include <QGraphicsDropShadowEffect>
#include <QTimer>

TransparentMenu::TransparentMenu(QWidget *parent) : QMenu(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    
    // 设置内容边距，为阴影效果预留足够空间
    setContentsMargins(15, 12, 15, 12);

    // 初始化阴影效果
    auto* shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setOffset(0, 2);
    shadowEffect->setColor(QColor(0, 0, 0, 40));
    shadowEffect->setBlurRadius(12);
    setGraphicsEffect(shadowEffect);
    
    // 设置样式
    QString style = QString(
        "QMenu {"
        "    background: transparent;"
        "    border: none;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background: transparent;"
        "    padding: 8px 24px;"
        "    border-radius: 4px;"
        "    margin: 2px 4px;"
        "    color: #333333;"
        "    font-size: 14px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: rgba(0, 0, 0, 0.1);"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: rgba(0, 0, 0, 0.1);"
        "    margin: 4px 8px;"
        "}"
    );
    setStyleSheet(style);
}


void TransparentMenu::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 获取内容区域（考虑边距）
    QRect contentRect = rect().adjusted(15, 12, -15, -12);

    // 绘制半透明白色背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawRoundedRect(contentRect, 8, 8);
    
    // 绘制菜单项
    QStyleOptionMenuItem menuOpt;
    menuOpt.initFrom(this);
    menuOpt.state = QStyle::State_None;
    menuOpt.checkType = QStyleOptionMenuItem::NotCheckable;
    menuOpt.maxIconWidth = 0;
    
    for (QAction* action : actions()) {
        if (action->isSeparator()) {
            // 绘制分隔线
            QRect sepRect = actionGeometry(action);
            painter.setPen(QPen(QColor(0, 0, 0, 20), 1));
            painter.drawLine(sepRect.left() + 8, sepRect.center().y(),
                           sepRect.right() - 8, sepRect.center().y());
        } else {
            // 绘制菜单项
            QRect itemRect = actionGeometry(action);
            bool isSelected = itemRect.contains(mapFromGlobal(QCursor::pos()));
            
            if (isSelected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 15));
                painter.drawRoundedRect(itemRect.adjusted(4, 2, -4, -2), 4, 4);
            }
            
            // 绘制图标
            if (!action->icon().isNull()) {
                QIcon icon = action->icon();
                QSize iconSize(16, 16);
                QRect iconRect = itemRect;
                iconRect.setSize(iconSize);
                iconRect.moveLeft(itemRect.left() + 12);
                iconRect.moveTop(itemRect.top() + (itemRect.height() - iconSize.height()) / 2);
                icon.paint(&painter, iconRect);
            }
            
            painter.setPen(QColor(51, 51, 51));
            QFont font = painter.font();
            font.setPixelSize(14);
            painter.setFont(font);
            
            // 调整文本位置，为图标留出空间
            QRect textRect = itemRect;
            textRect.setLeft(itemRect.left() + 34);
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                           action->text());
        }
    }
}

