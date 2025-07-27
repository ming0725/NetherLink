/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>

#include "View/AiChat/AiChatListModel.h"
#include "View/AiChat/AiChatListView.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

AiChatListView::AiChatListView(QWidget*parent) : QListView(parent), m_smoothScrollValue(0) {
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setViewportMargins(0, 0, 12, 0);
    setSpacing(2);

    // 设置背景色
    setStyleSheet("AiChatListView { background-color: #F2F2F2; }");

    // 创建自定义滚动条
    customScrollBar = new SmoothScrollBar(this);
    customScrollBar->hide();

    // 创建滚动动画
    scrollAnimation = new QPropertyAnimation(this, "smoothScrollValue", this);
    scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    scrollAnimation->setDuration(200);

    // 连接信号槽
    connect(customScrollBar, &SmoothScrollBar::valueChanged, this, &AiChatListView::onCustomScrollValueChanged);
    connect(scrollAnimation, &QPropertyAnimation::finished, this, &AiChatListView::onAnimationFinished);
}

void AiChatListView::setModel(QAbstractItemModel*model) {
    if (this->model()) {
        disconnect(this->model(), &QAbstractItemModel::rowsInserted, this, &AiChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved, this, &AiChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::modelReset, this, &AiChatListView::onModelRowsChanged);
    }
    QListView::setModel(model);

    if (model) {
        connect(model, &QAbstractItemModel::rowsInserted, this, &AiChatListView::onModelRowsChanged);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &AiChatListView::onModelRowsChanged);
        connect(model, &QAbstractItemModel::modelReset, this, &AiChatListView::onModelRowsChanged);

        // 初始化后延迟检查滚动条状态
        QTimer::singleShot(50, this, &AiChatListView::checkScrollBarVisibility);
    }
}

void AiChatListView::checkScrollBarVisibility() {
    // 强制重新计算布局
    doItemsLayout();
    updateGeometries();

    // 检查是否需要滚动条
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();

    if (totalHeight <= viewport()->height()) {
        customScrollBar->hide();
        m_smoothScrollValue = 0;
        vScrollBar->setValue(0);
    } else {
        updateCustomScrollBar();
    }
}

void AiChatListView::onModelRowsChanged() {
    // 延迟检查滚动条状态
    QTimer::singleShot(50, this, &AiChatListView::checkScrollBarVisibility);

    // 如果在底部，自动滚动到新位置
    QScrollBar* vScrollBar = verticalScrollBar();

    if (180 >= vScrollBar->maximum() - vScrollBar->value()) {
        QTimer::singleShot(50, this, &AiChatListView::scrollToBottom);
    }
}

void AiChatListView::mousePressEvent(QMouseEvent* event) {
    QModelIndex index = indexAt(event->pos());

    if (!index.isValid() || static_cast <AiChatListModel*>(model())->isBottomSpace(index.row())) {
        // 点击空白区域或底部留白，清除选中状态
        if (model()) {
            static_cast <AiChatListModel*>(model())->clearSelection();
        }
        event->accept();

        return;
    }
    QListView::mousePressEvent(event);
}

void AiChatListView::resizeEvent(QResizeEvent*event) {
    QListView::resizeEvent(event);

    // 更新自定义滚动条位置和大小
    customScrollBar->setGeometry(width() - customScrollBar->width() - 4, 0, customScrollBar->width(), height());
    updateCustomScrollBar();

    // 强制重新计算所有项的大小和位置
    if (model()) {
        for (int i = 0; i < model()->rowCount(); ++i) {
            QModelIndex index = model()->index(i, 0);

            update(index);
        }
        doItemsLayout();
    }
}

void AiChatListView::wheelEvent(QWheelEvent*event) {
    if (model() && (model()->rowCount() > 0)) {
        QScrollBar* vScrollBar = verticalScrollBar();
        int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();

        if (totalHeight <= viewport()->height()) {
            event->accept();

            return;
        }

        // 原先直接用 angleDelta().y()，假设想让每格滚轮只滚动 30 像素：
        int rawDelta = event->angleDelta().y(); // 比如 ±120
        int step = 185; // 自定义"每格滚动"要滚动的像素数
        // 可以用 rawDelta / 120 * step，这样一格滚轮滚动就是 ±30 像素：
        int delta = ((rawDelta > 0) ? step : -step);
        int targetValue = m_smoothScrollValue - delta;

        targetValue = qBound(0, targetValue, vScrollBar->maximum());
        startScrollAnimation(targetValue);

        if (totalHeight > viewport()->height()) {
            customScrollBar->show();
            customScrollBar->showScrollBar();
        }
        event->accept();
    }
}

void AiChatListView::enterEvent(QEnterEvent*event) {
    hovered = true;
    updateCustomScrollBar();
    QListView::enterEvent(event);
}

void AiChatListView::leaveEvent(QEvent*event) {
    hovered = false;

    if (customScrollBar->isVisible()) {
        customScrollBar->startFadeOut();
    }
    QListView::leaveEvent(event);
}

void AiChatListView::onCustomScrollValueChanged(int value) {
    if (scrollAnimation->state() != QPropertyAnimation::Running) {
        m_smoothScrollValue = value;
        verticalScrollBar()->setValue(value);
    }
}

void AiChatListView::onAnimationFinished() {
    // 如果鼠标不在视图上，隐藏滚动条
    if (!hovered) {
        customScrollBar->startFadeOut();
    }
}

void AiChatListView::setSmoothScrollValue(int value) {
    // 检查是否需要滚动
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();

    // 只在内容高度大于视口高度时更新滚动值
    if (totalHeight > viewport()->height()) {
        if (m_smoothScrollValue != value) {
            bool isAtBottom = value >= vScrollBar->maximum() - 5;

            m_smoothScrollValue = value;
            vScrollBar->setValue(value);

            if (!isAtBottom) {
                // 不在底部时才更新自定义滚动条的位置
                if (customScrollBar->isVisible()) {
                    customScrollBar->setValue(value);
                }
            }
        }
    }
}

void AiChatListView::updateCustomScrollBar() {
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();

    // 计算是否需要显示滚动条
    if (totalHeight > viewport()->height()) {
        customScrollBar->setRange(0, vScrollBar->maximum());
        customScrollBar->setPageStep(vScrollBar->pageStep());
        customScrollBar->setValue(vScrollBar->value());

        if (hovered) {
            customScrollBar->show();
            customScrollBar->showScrollBar();
        }
    } else {
        customScrollBar->hide();
    }
}

void AiChatListView::startScrollAnimation(int targetValue) {
    if (scrollAnimation->state() == QPropertyAnimation::Running) {
        scrollAnimation->stop();
    }
    scrollAnimation->setStartValue(m_smoothScrollValue);
    scrollAnimation->setEndValue(targetValue);
    scrollAnimation->start();
}

void AiChatListView::scrollToBottom() {
    QScrollBar* vScrollBar = verticalScrollBar();

    startScrollAnimation(vScrollBar->maximum());
}
