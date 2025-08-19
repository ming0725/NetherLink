/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPropertyAnimation>
#include <QWheelEvent>
#include <QEvent>
#include <QScrollBar>
#include <QTimer>
#include <QCursor>

#include "Components/SmoothScrollBar.h"
#include "View/Friend/FriendListView.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

FriendListView::FriendListView(QWidget* parent) : QListView(parent), customScrollBar(new SmoothScrollBar(this)), scrollAnimation(new QPropertyAnimation(this, "smoothScrollValue", this)), m_smoothScrollValue(0), hovered(false) {
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setSpacing(0);
    setUniformItemSizes(false);
    setResizeMode(QListView::Adjust);
    setViewMode(QListView::ListMode);
    setFlow(QListView::TopToBottom);
    setWrapping(false);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setGridSize(QSize(0, 72));
    setWordWrap(false);
    setTextElideMode(Qt::ElideRight);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    customScrollBar->hide();

    scrollAnimation->setDuration(300);
    scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(customScrollBar, &SmoothScrollBar::valueChanged, this, &FriendListView::onCustomScrollValueChanged);
    connect(scrollAnimation, &QPropertyAnimation::finished, this, &FriendListView::onAnimationFinished);
}

void FriendListView::setModel(QAbstractItemModel* model) {
    if (this->model()) {
        disconnect(this->model(), &QAbstractItemModel::rowsInserted, this, &FriendListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved, this, &FriendListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::modelReset, this, &FriendListView::onModelRowsChanged);
    }
    QListView::setModel(model);

    if (model) {
        connect(model, &QAbstractItemModel::rowsInserted, this, &FriendListView::onModelRowsChanged);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &FriendListView::onModelRowsChanged);
        connect(model, &QAbstractItemModel::modelReset, this, &FriendListView::onModelRowsChanged);

        QTimer::singleShot(100, this, [this]() {
            checkScrollBarVisibility();
            updateCustomScrollBar();
        });
    }
}

void FriendListView::resizeEvent(QResizeEvent* event) {
    QListView::resizeEvent(event);

    customScrollBar->setGeometry(width() - customScrollBar->width() - 4, 0, customScrollBar->width(), height());
    updateCustomScrollBar();

    if (model()) {
        doItemsLayout();
    }
}

void FriendListView::wheelEvent(QWheelEvent* event) {
    if (model() && (model()->rowCount() > 0)) {
        QScrollBar* vScrollBar = verticalScrollBar();
        int totalHeight = verticalScrollBar()->maximum() + verticalScrollBar()->pageStep();
        if (totalHeight <= viewport()->height()) {
            event->accept();
            return;
        }
        int delta = event->angleDelta().y();
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

void FriendListView::enterEvent(QEnterEvent* event) {
    QListView::enterEvent(event);
    hovered = true;
    if (customScrollBar->isVisible()) {
        customScrollBar->showScrollBar();
    }
}

void FriendListView::leaveEvent(QEvent* event) {
    QListView::leaveEvent(event);
    QPoint globalPos = QCursor::pos();
    QPoint localPos = mapFromGlobal(globalPos);
    if (!rect().contains(localPos) && !customScrollBar->geometry().contains(localPos)) {
        customScrollBar->startFadeOut();
        hovered = false;
    }
}

bool FriendListView::viewportEvent(QEvent* event) {
    switch (event->type()) {
        case QEvent::Enter:
            if (customScrollBar->isVisible()) {
                customScrollBar->showScrollBar();
            }
            break;
        case QEvent::Leave:
        {
            QPoint globalPos = QCursor::pos();
            QPoint localPos = mapFromGlobal(globalPos);
            if (!rect().contains(localPos)) {
                customScrollBar->startFadeOut();
            }
            break;
        }
        case QEvent::MouseMove:
            if (customScrollBar->isVisible()) {
                customScrollBar->showScrollBar();
            }
            break;
        default:
            break;
    }
    return (QListView::viewportEvent(event));
}

void FriendListView::onCustomScrollValueChanged(int value) {
    if (scrollAnimation->state() != QPropertyAnimation::Running) {
        m_smoothScrollValue = value;
        verticalScrollBar()->setValue(value);
    }
}

void FriendListView::onAnimationFinished() {
    updateCustomScrollBar();
}

void FriendListView::onModelRowsChanged() {
    QTimer::singleShot(50, this, &FriendListView::checkScrollBarVisibility);
}

void FriendListView::checkScrollBarVisibility() {
    doItemsLayout();
    updateGeometries();
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

void FriendListView::setSmoothScrollValue(int value) {
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    if (totalHeight > viewport()->height()) {
        if (m_smoothScrollValue != value) {
            bool isAtBottom = value >= vScrollBar->maximum() - 5;
            m_smoothScrollValue = value;
            vScrollBar->setValue(value);
            if (!isAtBottom) {
                if (customScrollBar->isVisible()) {
                    customScrollBar->setValue(value);
                }
            }
        }
    }
}

void FriendListView::updateCustomScrollBar() {
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    bool needScrollBar = totalHeight > viewport()->height();
    if (needScrollBar) {
        customScrollBar->setRange(vScrollBar->minimum(), vScrollBar->maximum());
        customScrollBar->setPageStep(vScrollBar->pageStep());
        int currentValue = vScrollBar->value();
        customScrollBar->setValue(currentValue);
        m_smoothScrollValue = currentValue;
        customScrollBar->show();
    } else {
        customScrollBar->hide();
        m_smoothScrollValue = 0;
        vScrollBar->setValue(0);
    }
}

void FriendListView::startScrollAnimation(int targetValue) {
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    if (totalHeight > viewport()->height()) {
        scrollAnimation->stop();
        scrollAnimation->setDuration(300);
        scrollAnimation->setStartValue(m_smoothScrollValue);
        scrollAnimation->setEndValue(targetValue);
        scrollAnimation->start();
    }
}

