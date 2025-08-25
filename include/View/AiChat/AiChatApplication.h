/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_AI_CHAT_AI_CHAT_APPLICATION

#define INCLUDE_VIEW_AI_CHAT_AI_CHAT_APPLICATION

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */
#include <QObject>
#include <QStackedWidget>

#include "View/AiChat/AiChatListWidget.h"
#include "View/Mainwindow/DefaultPage.h"

#include "Util/ToastTip.hpp"
#include "Window/MainWindow.hpp"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class AiChatWindow;

class AiChatApplication : public QWidget {
    Q_OBJECT

    public:
        explicit AiChatApplication(QWidget* parent = nullptr);

    protected:
        void resizeEvent(QResizeEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

    private slots:
        void onChatItemClicked(AiChatListItem* item);

        void onChatItemRenamed(AiChatListItem* item);

        void onChatItemDeleted(AiChatListItem* item);

    private:

        class LeftPane : public QWidget {
            private:
                AiChatListWidget* m_aiChatList;
                QLabel* m_iconLabel;
                QPushButton* m_button;
                const int topMargin = 20;
                const int leftMargin = 10;
                const int iconSize = 50;
                const int iconBtnDist = 10;
                const int btnListDist = 2;
                const int buttonSize = 20;

            public:
                explicit LeftPane(QWidget* parent = nullptr) : QWidget(parent), m_aiChatList(new AiChatListWidget(this)), m_iconLabel(new QLabel(this)), m_button(new QPushButton(this)) {
                    setMinimumWidth(200);
                    setMaximumWidth(400);
                    m_aiChatList->setStyleSheet("border-width:0px;border-style:solid;");

                    // 设置图标
                    QPixmap pixmap(":/icon/aichat.png");

                    m_iconLabel->setPixmap(pixmap);
                    m_iconLabel->setScaledContents(true); // 自动缩放
                    m_iconLabel->setStyleSheet("background: transparent;");

                    // 设置按钮
                    m_button->setText("＋");
                    m_button->setFixedSize(buttonSize * 1.6, buttonSize);
                    m_button->setCursor(Qt::PointingHandCursor);
                    m_button->setStyleSheet("border-radius:10px; background-color:#0078D7; color:white;");

                    // 设置列表样式
                    m_aiChatList->setStyleSheet("border-width:0px; border-style:solid;");
                    connect(m_button, &QPushButton::clicked, [=, this]() {
                auto* item = new AiChatListItem(m_aiChatList);
                item->setTitle("新对话");
                item->setTime(QDateTime::currentDateTime());
                m_aiChatList->addChatItem(item);
                Util::ToastTip::函数_实例().函数_显示消息(Window::MainWindow::getInstance(), Util::ToastTip::枚举_消息类型::ENUM_SUCCESS, "新建成功！");
            });
                }

                AiChatListWidget* chatList() const {
                    return (m_aiChatList);
                }

            protected:
                void resizeEvent(QResizeEvent* ev) override {
                    QWidget::resizeEvent(ev);

                    int y = topMargin;
                    QPixmap originalPixmap(":/icon/aichat.png");
                    QSize iconTargetSize(iconSize, iconSize);

                    if (!originalPixmap.isNull()) {
                        QSize scaledSize = originalPixmap.size();

                        scaledSize.scale(iconSize, iconSize, Qt::KeepAspectRatio);
                        iconTargetSize = scaledSize;
                    }
                    m_iconLabel->setGeometry(leftMargin, y, iconTargetSize.width(), iconTargetSize.height());
                    y += iconTargetSize.height() + iconBtnDist;
                    m_button->move(leftMargin + 10, y);
                    y += buttonSize + btnListDist;
                    m_aiChatList->setGeometry(0, y, width(), height() - y);
                }
        };

        LeftPane* m_leftPane;
        DefaultPage* m_defaultPage;
        QSplitter* m_splitter;
        QStackedWidget* m_rightPane;
        QMap <QString, AiChatWindow*> m_chatWindows;
};

#endif /* INCLUDE_VIEW_AI_CHAT_AI_CHAT_APPLICATION */
