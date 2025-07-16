#include "SearchTypeTab.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

SearchTypeTab::SearchTypeTab(QWidget* parent) : QWidget(parent), animation(new QPropertyAnimation(this, "indicatorX", this)) {
    setFixedHeight(TAB_HEIGHT);

    // 添加样式表移除边框
    setStyleSheet("QWidget { border: none; background-color: transparent; }");
    initTabs();

    // 设置动画属性
    animation->setDuration(200); // 200ms
    animation->setEasingCurve(QEasingCurve::OutCubic);

    // 初始化指示器位置
    QTimer::singleShot(0, this, [this] () {
        QLabel* firstTab = tabs[0];
        m_indicatorX = firstTab->x() + (firstTab->width() - INDICATOR_WIDTH) / 2;
        update();
    });
}

void SearchTypeTab::initTabs() {
    QHBoxLayout* layout = new QHBoxLayout(this);

    layout->setContentsMargins(20, 0, 20, 0);  // 左右边距20px
    layout->setSpacing(30); // 标签间距30px

    for (const QString& text : tabTexts) {
        QLabel* label = new QLabel(text, this);

        label->setFixedHeight(TAB_HEIGHT - INDICATOR_HEIGHT);
        label->setAlignment(Qt::AlignCenter);
        label->setCursor(Qt::PointingHandCursor);  // 设置手型光标
        tabs.append(label);
        layout->addWidget(label);
    }
    layout->addStretch();  // 右侧添加弹簧
    updateTabStyles();
}

void SearchTypeTab::updateTabStyles() {
    for (int i = 0; i < tabs.size(); ++i) {
        QFont font = tabs[i]->font();

        font.setPixelSize(14);
        tabs[i]->setFont(font);

        // 设置颜色
        QString color = (i == currentIdx) ? QString("color: #0099FF;") : QString("color: #000000;");

        tabs[i]->setStyleSheet(QString("QLabel { %1 }").arg(color));
    }
}

void SearchTypeTab::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制蓝色指示器
    painter.setPen(Qt::NoPen);
    painter.setBrush(SELECTED_COLOR);

    // 计算指示器位置
    QLabel* currentTab = tabs[currentIdx];
    int x = currentTab->x() + (currentTab->width() - INDICATOR_WIDTH) / 2;
    int y = height() - INDICATOR_HEIGHT;

    // 使用动画位置绘制指示器
    painter.drawRect(m_indicatorX, y, INDICATOR_WIDTH, INDICATOR_HEIGHT);
}

void SearchTypeTab::mousePressEvent(QMouseEvent* event) {
    QPoint pos = event->pos();

    for (int i = 0; i < tabs.size(); ++i) {
        if (tabs[i]->geometry().contains(pos)) {
            setCurrentIndex(i);
            break;
        }
    }
}

void SearchTypeTab::setCurrentIndex(int index, bool animate) {
    if ((index == currentIdx) || (index < 0) || (index >= tabs.size()))
        return;
    currentIdx = index;
    updateTabStyles();

    // 计算新的指示器位置
    QLabel* targetTab = tabs[index];
    int targetX = targetTab->x() + (targetTab->width() - INDICATOR_WIDTH) / 2;

    if (animate) {
        animation->setStartValue(m_indicatorX);
        animation->setEndValue(targetX);
        animation->start();
    } else {
        m_indicatorX = targetX;
        update();
    }

    emit currentIndexChanged(index);
}