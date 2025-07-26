/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_CHAT_MESSAGE_APPLICATION
#define INCLUDE_VIEW_CHAT_MESSAGE_APPLICATION

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QSplitter>
#include <QStackedWidget>

#include "Components/TopSearchWidget.h"
#include "View/Chat/ChatArea.h"
#include "View/Chat/MessageListWidget.h"
#include "View/Mainwindow/DefaultPage.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class MessageApplication : public QWidget {
    Q_OBJECT

    public:
        explicit MessageApplication(QWidget* parent = nullptr);

    protected:
        void resizeEvent(QResizeEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

    private slots:
        void onMessageClicked(MessageListItem* item);

    private:
        QSplitter*          m_splitter;
        TopSearchWidget*    m_topSearch;
        MessageListWidget*  m_msgList;
        QStackedWidget*     m_rightStack;
        DefaultPage*        m_defaultPage;
        ChatArea*           m_chatArea;
};

#endif /* INCLUDE_VIEW_CHAT_MESSAGE_APPLICATION */
