#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QWidget>

class SearchTypeTab : public QWidget {
    Q_OBJECT Q_PROPERTY(int indicatorX READ indicatorX WRITE setIndicatorX)

    public:
        explicit SearchTypeTab(QWidget* parent = nullptr);

        void setCurrentIndex(int index, bool animate = true);

    protected:
        void paintEvent(QPaintEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

    signals:
        void currentIndexChanged(int index);

    private:
        QList <QLabel*> tabs;
        QList <QString> tabTexts = {"全部", "用户", "群聊"};
        int currentIdx = 0;
        int m_indicatorX = 0;
        QPropertyAnimation* animation;
        const int TAB_HEIGHT = 40;
        const int INDICATOR_HEIGHT = 2;
        const int INDICATOR_WIDTH = 28;
        const QColor SELECTED_COLOR = QColor(0, 153, 255); // #0099FF
        const QColor NORMAL_COLOR = QColor(0, 0, 0);

        int indicatorX() const {
            return (m_indicatorX);
        }

        void setIndicatorX(int x) {
            m_indicatorX = x;
            update();
        }

        void initTabs();

        void updateTabStyles();

};