/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QScreen>

#include "View/Friend/SearchFriendWindow.h"

/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
SearchFriendWindow* SearchFriendWindow::instance = nullptr;

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

SearchFriendWindow::SearchFriendWindow(QWidget* parent) : FramelessWindow(parent) {
    // 窗口基础设置
    resize(400, 500);
    setMinimumWidth(400);
    setMinimumHeight(500);
    setAttribute(Qt::WA_DeleteOnClose);

    // 创建标题标签
    titleLabel = new QLabel("添加好友/群", this);

    QFont titleFont;

    titleFont.setPixelSize(14);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    // 创建搜索框
    searchBox = new LineEditComponent(this);
    searchBox->setFixedHeight(SEARCH_BOX_HEIGHT);
    searchBox->getLineEdit()->setPlaceholderText("请输入关键词");

    // 创建类型选项卡
    typeTab = new SearchTypeTab(this);

    // 创建搜索结果列表
    resultList = new SearchResultList(this);
    resultList->setWindow(this);

    // 创建窗口控制按钮
    btnMinimize = new QPushButton(this);
    btnClose = new QPushButton(this);

    // 设置图标
    iconClose = QIcon(":/icon/close.png");
    iconCloseHover = QIcon(":/icon/hovered_close.png");
    btnClose->installEventFilter(this);
    btnMinimize->setIcon(QIcon(":/icon/minimize.png"));
    btnClose->setIcon(iconClose);
    btnMinimize->setIconSize(QSize(16, 16));
    btnClose->setIconSize(QSize(16, 16));

    // 设置按钮样式
    QString btnStyle = QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #E9E9E9;
        }
    )");

    btnMinimize->setStyleSheet(btnStyle);
    btnClose->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #C42B1C;
        }
    )"));
    resultList->setStyleSheet("border-width:0px;border-style:solid;");
    btnMinimize->setFixedSize(BTN_SIZE, BTN_SIZE);
    btnClose->setFixedSize(BTN_SIZE, BTN_SIZE);

    // 连接信号
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(typeTab, &SearchTypeTab::currentIndexChanged, this, &SearchFriendWindow::onSearchTypeChanged);
    connect(btnClose, &QPushButton::clicked, this, [this] () {
        close();
        instance = nullptr;
    });
    connect(searchBox->getLineEdit(), &QLineEdit::textChanged, this, &SearchFriendWindow::onSearchTextChanged);

    // 设置窗口位置到屏幕中央
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect sg = screen->geometry();
    int cx = (sg.width() - width()) / 2;
    int cy = (sg.height() - height()) / 2;

    move(cx, cy);
}

SearchFriendWindow* SearchFriendWindow::getInstance() {
    if (!instance) {
        instance = new SearchFriendWindow();
    }
    return (instance);
}

void SearchFriendWindow::resizeEvent(QResizeEvent* event) {
    FramelessWindow::resizeEvent(event);

    int w = width();

    // 设置标题位置
    titleLabel->setGeometry(0, 0, w, TITLE_HEIGHT);

    // 设置按钮位置
    btnClose->move(w - BTN_SIZE, 0);
    btnMinimize->move(w - BTN_SIZE * 2, 0);

    // 设置搜索框位置
    int searchBoxWidth = w - 2 * MARGIN;

    searchBox->setGeometry((w - searchBoxWidth) / 2, TITLE_HEIGHT + MARGIN, searchBoxWidth, SEARCH_BOX_HEIGHT);

    // 设置类型选项卡位置
    typeTab->setGeometry(0, TITLE_HEIGHT + MARGIN + SEARCH_BOX_HEIGHT + MARGIN / 2, w, typeTab->height());

    // 设置结果列表位置
    int listY = typeTab->y() + typeTab->height();

    resultList->setGeometry(0, listY, w, height() - listY);
}

void SearchFriendWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    // 设置白色背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(rect());
}

void SearchFriendWindow::mousePressEvent(QMouseEvent* event) {
    // 获取当前焦点控件
    QWidget* fw = QApplication::focusWidget();

    // 如果当前有焦点控件，并且是LineEditComponent或QLineEdit
    if (fw && (qobject_cast <LineEditComponent*>(fw) || qobject_cast <QLineEdit*>(fw))) {
        // 清除焦点
        fw->clearFocus();
    }

    // 调用父类的mousePressEvent以保持窗口拖动功能
    FramelessWindow::mousePressEvent(event);
}

void SearchFriendWindow::onSearchTypeChanged(int index) {
    // 所有类型使用相同的提示文字
    searchBox->getLineEdit()->setPlaceholderText("请输入关键词");

    // 更新搜索类型
    SearchResultList::SearchType type;

    switch (index) {
        case 0:
            type = SearchResultList::SearchType::All;
            break;
        case 1:
            type = SearchResultList::SearchType::Users;
            break;
        case 2:
            type = SearchResultList::SearchType::Groups;
            break;
        default:
            type = SearchResultList::SearchType::All;
    }
    resultList->setSearchType(type);
}

void SearchFriendWindow::onSearchTextChanged(const QString& text) {
    resultList->setSearchText(text);
}

bool SearchFriendWindow::eventFilter(QObject* watched, QEvent* ev) {
    if (watched == btnClose) {
        if (ev->type() == QEvent::Enter) {
            btnClose->setIcon(iconCloseHover);
        } else if (ev->type() == QEvent::Leave) {
            btnClose->setIcon(iconClose);
        }
    }
    return (FramelessWindow::eventFilter(watched, ev));
}
