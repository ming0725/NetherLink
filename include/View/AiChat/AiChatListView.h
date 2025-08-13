/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_AI_CHAT_AI_CHAT_LIST_VIEW

#define INCLUDE_VIEW_AI_CHAT_AI_CHAT_LIST_VIEW

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QListView>

#include "Components/SmoothScrollBar.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class AiChatListView : public QListView {
    Q_OBJECT Q_PROPERTY(int smoothScrollValue READ smoothScrollValue WRITE setSmoothScrollValue)

    public:
        explicit AiChatListView(QWidget* parent = nullptr);

        void setModel(QAbstractItemModel* model) override;

        void scrollToBottom();

    protected:
        void mousePressEvent(QMouseEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;

        void wheelEvent(QWheelEvent* event) override;

        void enterEvent(QEnterEvent* event) override;

        void leaveEvent(QEvent* event) override;

    private slots:
        void onCustomScrollValueChanged(int value);

        void onAnimationFinished();

        void onModelRowsChanged();

        void checkScrollBarVisibility();

    private:
        SmoothScrollBar* customScrollBar;
        QPropertyAnimation* scrollAnimation;
        int m_smoothScrollValue;
        bool hovered = false;

        int smoothScrollValue() const {
            return (m_smoothScrollValue);
        }

        void setSmoothScrollValue(int value);

        void updateCustomScrollBar();

        void startScrollAnimation(int targetValue);
};

#endif /* INCLUDE_VIEW_AI_CHAT_AI_CHAT_LIST_VIEW */
