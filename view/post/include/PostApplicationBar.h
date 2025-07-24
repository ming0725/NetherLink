
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QVariantAnimation>

#include "TextBarItem.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class PostApplicationBar : public QWidget {
    Q_OBJECT

    public:
        explicit PostApplicationBar(QWidget* parent = nullptr);

        QSize minimumSizeHint() const Q_DECL_OVERRIDE;

        QSize sizeHint() const Q_DECL_OVERRIDE;

        void enableBlur(bool enabled = true) {
            isEnableBlur = enabled;
        }

        void setCurrentIndex(int index);

    signals:
        void pageClicked(int index);

    protected:
        void paintEvent(QPaintEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;

        void updateBlurBackground();

    private slots:
        void onItemClicked(int index);

        void onHighlightValueChanged(const QVariant &value);

    private:
        QVector <TextBarItem*> items;
        TextBarItem* selectedItem = nullptr;
        QVariantAnimation* highlightAnim;
        int highlightX = 0;
        const int itemHeight = 32;
        const int spacing = 0;
        const int margin = 6;

        void initItems();

        void layoutItems();

    private:
        QWidget* m_parent;
        QTimer* m_updateTimer;
        QImage m_blurredBackground;
        QRect selectedRect;
        bool isEnableBlur = false;
        QSize preSize;
};
