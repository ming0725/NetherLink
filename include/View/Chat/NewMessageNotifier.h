/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_CHAT_NEW_MESSAGE_NOTIFIER

#define INCLUDE_VIEW_CHAT_NEW_MESSAGE_NOTIFIER

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QLabel>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class NewMessageNotifier : public QWidget {
    Q_OBJECT

    public:
        explicit NewMessageNotifier(QWidget* parent = nullptr);

        void setMessageCount(int count);

    signals:
        void clicked();

    protected:
        void paintEvent(QPaintEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;

    private:
        QLabel* label;

        void updateText(int count);

        void updateLayout();
};

#endif /* INCLUDE_VIEW_CHAT_NEW_MESSAGE_NOTIFIER */
