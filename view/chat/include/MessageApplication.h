
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QSplitter>

#include "ChatArea.h"

#include <QStackedWidget>

#include "DefaultPage.h"

#include "MessageListWidget.h"

#include "TopSearchWidget.h"

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
