#include "FriendDetailPage.h"
#include "shared/services/AppFonts.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include "features/friend/ui/FriendSessionController.h"
#include "shared/services/ImageService.h"
#include "shared/ui/ImageViewer.h"
#include "shared/ui/popup/InWindowPopupDialogs.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kAvatarSize = 88;
constexpr int kContentMaxWidth = 660;
constexpr int kHeaderSpacing = 32;
constexpr int kIdentityTopInset = 6;
constexpr int kSeparatorTopSpacing = 54;

PaintedLabel* makeTitleLabel(const QString& text)
{
    auto* label = new PaintedLabel(text);
    QFont font = label->font();
    font.setPixelSize(14);
    label->setFont(font);
    label->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    return label;
}

QHBoxLayout* makeInfoRow(const QString& title, QWidget* valueWidget)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(36);

    auto* titleLabel = makeTitleLabel(title);
    titleLabel->setFixedWidth(108);
    row->addWidget(titleLabel, 0, Qt::AlignTop);
    row->addWidget(valueWidget, 1);
    return row;
}

void applyPrimaryButtonStyle(StatefulPushButton* button)
{
    button->setRadius(8);
    button->setPrimaryStyle();
}

void applyDangerOutlineButtonStyle(StatefulPushButton* button)
{
    button->setRadius(8);
    button->setNormalColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    button->setHoverColor(ThemeManager::instance().color(ThemeColor::DangerControlHover));
    button->setPressColor(ThemeManager::instance().color(ThemeColor::DangerControlPressed));
    button->setTextColor(ThemeManager::instance().color(ThemeColor::DangerText));
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    button->setBorderWidth(1);
}

class CopyIdButton final : public QToolButton
{
public:
    explicit CopyIdButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setFixedSize(26, 24);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setIconSize(QSize(15, 15));
        setToolTip(QStringLiteral("复制ID"));
        setAccessibleName(QStringLiteral("复制ID"));
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            m_hovered = true;
            update();
        } else if (event->type() == QEvent::Leave) {
            m_hovered = false;
            update();
        }
        return QToolButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (isDown() || m_hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(ThemeManager::instance().color(isDown()
                    ? ThemeColor::ControlPressed
                    : ThemeColor::ControlHover));
            painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);
        }

        QPixmap icon = ImageService::instance().scaled(QStringLiteral(":/resources/icon/copy.svg"),
                                                       iconSize(),
                                                       Qt::KeepAspectRatio,
                                                       devicePixelRatioF());
        if (!icon.isNull()) {
            if (ThemeManager::instance().isDark()) {
                QImage image = icon.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
                image.invertPixels(QImage::InvertRgb);
                icon = QPixmap::fromImage(image);
                icon.setDevicePixelRatio(devicePixelRatioF());
            }
            QRect target(QPoint(0, 0), iconSize());
            target.moveCenter(rect().center());
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawPixmap(target, icon);
            return;
        }

        const QColor iconColor = isEnabled()
                ? ThemeManager::instance().color(m_hovered ? ThemeColor::PrimaryText : ThemeColor::TertiaryText)
                : ThemeManager::instance().color(ThemeColor::PlaceholderText);
        painter.setPen(QPen(iconColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);

        const QRect iconRect(QPoint(0, 0), QSize(16, 16));
        QRect centeredIconRect = iconRect;
        centeredIconRect.moveCenter(rect().center());
        const QRectF backSheet(centeredIconRect.left() + 6,
                               centeredIconRect.top() + 2,
                               8,
                               10);
        const QRectF frontSheet(centeredIconRect.left() + 2,
                                centeredIconRect.top() + 6,
                                10,
                                8);
        painter.drawRoundedRect(backSheet, 2, 2);
        painter.fillRect(frontSheet.adjusted(0, 0, 1, 1),
                         ThemeManager::instance().color(ThemeColor::PageBackground));
        painter.drawRoundedRect(frontSheet, 2, 2);
    }

private:
    bool m_hovered = false;
};

} // namespace

class GroupSelectButton : public QToolButton
{
public:
    explicit GroupSelectButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setFixedSize(172, 34);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setPopupMode(QToolButton::DelayedPopup);
        setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    void setMenuHoverSuppressed(bool suppressed)
    {
        if (m_menuHoverSuppressed == suppressed) {
            return;
        }

        m_menuHoverSuppressed = suppressed;
        update();
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            m_hovered = true;
            setMenuHoverSuppressed(false);
            update();
        } else if (event->type() == QEvent::Leave) {
            m_hovered = false;
            setMenuHoverSuppressed(false);
            update();
        } else if (event->type() == QEvent::MouseButtonPress) {
            setMenuHoverSuppressed(false);
        }
        return QToolButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);
        painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1));
        painter.setBrush((m_hovered && !m_menuHoverSuppressed)
                                 ? ThemeManager::instance().color(ThemeColor::ListHover)
                                 : ThemeManager::instance().color(ThemeColor::InputBackground));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

        QFont textFont = font();
        textFont.setPixelSize(14);
        painter.setFont(textFont);
        painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        const QRect textRect = rect().adjusted(14, 0, -34, 0);
        painter.drawText(textRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(textFont).elidedText(text(), Qt::ElideRight, textRect.width()));

        const int centerX = width() - 19;
        const int centerY = height() / 2;
        QPen arrowPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                      1.6,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(centerX - 4.5, centerY - 2.0), QPointF(centerX, centerY + 2.5));
        painter.drawLine(QPointF(centerX, centerY + 2.5), QPointF(centerX + 4.5, centerY - 2.0));
    }

private:
    bool m_hovered = false;
    bool m_menuHoverSuppressed = false;
};

FriendDetailPage::FriendDetailPage(QWidget* parent)
    : QWidget(parent)
    , m_contentWidget(new QWidget(this))
    , m_nameLabel(new PaintedLabel(this))
    , m_idPrefixLabel(new PaintedLabel(QStringLiteral("ID"), this))
    , m_idLabel(new PaintedLabel(this))
    , m_copyIdButton(new CopyIdButton(this))
    , m_regionRow(new QWidget(this))
    , m_regionLabel(new PaintedLabel(this))
    , m_statusIcon(new QLabel(this))
    , m_statusLabel(new PaintedLabel(this))
    , m_remarkEdit(new InlineEditableText(this))
    , m_groupButton(new GroupSelectButton(this))
    , m_groupMenu(new StyledActionMenu(this))
    , m_signatureLabel(new PaintedLabel(this))
    , m_messageButton(new StatefulPushButton(QStringLiteral("发消息"), this))
    , m_deleteButton(new StatefulPushButton(QStringLiteral("删除好友"), this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 80, 48, 44);
    root->setSpacing(0);

    m_contentWidget->setMaximumWidth(kContentMaxWidth);
    auto* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    root->addWidget(m_contentWidget, 0, Qt::AlignHCenter);
    root->addStretch(1);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(kHeaderSpacing);
    header->addSpacing(kAvatarSize);

    auto* identity = new QVBoxLayout;
    identity->setContentsMargins(0, 6, 0, 0);
    identity->setSpacing(6);

    QFont nameFont = m_nameLabel->font();
    nameFont.setPixelSize(20);
    nameFont.setWeight(QFont::DemiBold);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_nameLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont secondaryFont = m_idLabel->font();
    secondaryFont.setPixelSize(13);
    m_idPrefixLabel->setFont(secondaryFont);
    m_idLabel->setFont(secondaryFont);
    m_regionLabel->setFont(secondaryFont);
    m_statusLabel->setFont(secondaryFont);
    m_idPrefixLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_idLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_regionLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_statusLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_idPrefixLabel->setTextColor(ThemeManager::instance().color(ThemeColor::TertiaryText));
    m_idLabel->setTextColor(ThemeManager::instance().color(ThemeColor::TertiaryText));
    m_idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_regionLabel->setTextColor(ThemeManager::instance().color(ThemeColor::TertiaryText));
    m_regionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));

    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(6);
    m_statusIcon->setFixedSize(14, 14);
    statusRow->addWidget(m_statusIcon);
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    auto* idRow = new QHBoxLayout;
    idRow->setContentsMargins(0, 0, 0, 0);
    idRow->setSpacing(5);
    idRow->addWidget(m_idPrefixLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    idRow->addWidget(m_idLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    idRow->addWidget(m_copyIdButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
    idRow->addStretch();
    m_idPrefixLabel->setVisible(false);
    m_copyIdButton->setEnabled(false);
    m_copyIdButton->setVisible(false);

    identity->addWidget(m_nameLabel);
    identity->addLayout(idRow);
    identity->addLayout(statusRow);
    identity->addStretch();
    header->addLayout(identity, 1);
    contentLayout->addLayout(header);

    contentLayout->addSpacing(54);
    contentLayout->addSpacing(1);
    contentLayout->addSpacing(34);

    auto* regionHost = new QWidget(this);
    auto* regionLayout = new QHBoxLayout(regionHost);
    regionLayout->setContentsMargins(0, 0, 0, 0);
    regionLayout->addWidget(m_regionLabel, 1);
    contentLayout->addWidget(m_regionRow);
    m_regionRow->setLayout(makeInfoRow(QStringLiteral("地区"), regionHost));
    m_regionRow->layout()->setContentsMargins(0, 0, 0, 28);
    m_regionRow->hide();

    m_remarkEdit->setPlaceholderText(QStringLiteral("设置好友备注"));
    m_remarkEdit->setFocusPolicy(Qt::ClickFocus);
    m_remarkEdit->setFixedHeight(34);
    m_remarkEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_remarkEdit->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    m_remarkEdit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    m_remarkEdit->setNormalBackgroundColor(Qt::transparent);
    m_remarkEdit->setHoverBackgroundColor(ThemeManager::instance().color(ThemeColor::ListHover));
    m_remarkEdit->setFocusBackgroundColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    m_remarkEdit->setNormalBorderColor(Qt::transparent);
    m_remarkEdit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    m_remarkEdit->setBorderWidth(1);
    m_remarkEdit->setRadius(5);
    m_remarkEdit->setHorizontalPadding(8);
    connect(m_remarkEdit, &InlineEditableText::editingFinished, this, &FriendDetailPage::saveRemark);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("备注"), m_remarkEdit));
    contentLayout->addSpacing(28);

    auto* groupRowHost = new QWidget(this);
    auto* groupRow = new QHBoxLayout(groupRowHost);
    groupRow->setContentsMargins(0, 0, 0, 0);
    groupRow->addWidget(m_groupButton, 0, Qt::AlignLeft);
    groupRow->addStretch();
    contentLayout->addLayout(makeInfoRow(QStringLiteral("好友分组"), groupRowHost));
    contentLayout->addSpacing(28);

    m_signatureLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_signatureLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    QFont signatureFont = m_signatureLabel->font();
    signatureFont.setPixelSize(14);
    m_signatureLabel->setFont(signatureFont);
    m_signatureLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("签名"), m_signatureLabel));

    contentLayout->addSpacing(58);
    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addStretch();
    m_messageButton->setFixedSize(118, 38);
    m_deleteButton->setFixedSize(118, 38);
    applyPrimaryButtonStyle(m_messageButton);
    applyDangerOutlineButtonStyle(m_deleteButton);
    buttons->addWidget(m_messageButton);
    buttons->addWidget(m_deleteButton);
    buttons->addStretch();
    contentLayout->addLayout(buttons);

    connect(m_messageButton, &StatefulPushButton::clicked, this, [this]() {
        if (m_hasUser) {
            emit requestMessage(m_user.id);
        }
    });
    connect(m_deleteButton, &StatefulPushButton::clicked, this, [this]() {
        confirmDeleteFriend();
    });
    connect(m_copyIdButton, &QToolButton::clicked, this, &FriendDetailPage::copyCurrentId);

    connect(m_groupButton, &QToolButton::clicked, this, &FriendDetailPage::showGroupMenu);
    connect(m_groupMenu, &QMenu::aboutToHide, this, [this]() {
        m_groupButton->setDown(false);
        static_cast<GroupSelectButton*>(m_groupButton)->setMenuHoverSuppressed(true);
        m_groupButton->update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });

    qApp->installEventFilter(this);
    applyTheme();
}

FriendDetailPage::~FriendDetailPage()
{
    qApp->removeEventFilter(this);
}

QRect FriendDetailPage::avatarRect() const
{
    if (!m_contentWidget || !m_nameLabel) {
        return {};
    }

    const QPoint nameTopLeft = m_nameLabel->mapTo(const_cast<FriendDetailPage*>(this), QPoint(0, 0));
    const int avatarX = nameTopLeft.x() - kHeaderSpacing - kAvatarSize;
    const int avatarY = nameTopLeft.y() - kIdentityTopInset;
    return QRect(avatarX, avatarY, kAvatarSize, kAvatarSize);
}

void FriendDetailPage::setController(FriendSessionController* controller)
{
    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;
    if (!m_controller) {
        return;
    }

    connect(m_controller, &FriendSessionController::userAvatarImageReady,
            this, [this](const QString& requestId, const QString& userId, const QImage& image) {
        if (requestId != m_avatarImageRequestId || !m_avatarViewer || userId != m_user.id) {
            return;
        }
        m_avatarImageRequestId.clear();
        m_avatarViewer->replaceImage(image, m_avatarSource);
    });
    connect(m_controller, &FriendSessionController::userAvatarImageFailed,
            this, [this](const QString& requestId, const QString&) {
        if (requestId == m_avatarImageRequestId) {
            m_avatarImageRequestId.clear();
        }
    });
}

void FriendDetailPage::setUserId(const QString& userId)
{
    if (userId.isEmpty()) {
        clear();
        return;
    }
    setUser(m_controller ? m_controller->loadFriend(userId) : User());
}

void FriendDetailPage::clear()
{
    m_user = {};
    m_hasUser = false;
    m_avatarSource.clear();
    m_avatarImageRequestId.clear();
    m_nameLabel->clear();
    m_idPrefixLabel->setVisible(false);
    m_idLabel->clear();
    m_copyIdButton->setEnabled(false);
    m_copyIdButton->setVisible(false);
    m_regionLabel->clear();
    m_regionRow->hide();
    m_statusLabel->clear();
    m_statusIcon->clear();
    update();
}

void FriendDetailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateSignatureText();
}

void FriendDetailPage::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::PageBackground));

    if (!m_contentWidget) {
        return;
    }

    const QRect avatarRect = this->avatarRect();

    if (avatarRect.intersects(event->rect()) && !m_avatarSource.isEmpty()) {
        const QPixmap avatar = ImageService::instance().circularAvatar(m_avatarSource,
                                                                       kAvatarSize,
                                                                       devicePixelRatioF());
        painter.drawPixmap(avatarRect, avatar);
    }

    const int separatorY = avatarRect.top() + kAvatarSize + kSeparatorTopSpacing;
    const QRect separatorRect(m_contentWidget->x(), separatorY, m_contentWidget->width(), 1);
    if (separatorRect.intersects(event->rect())) {
        painter.fillRect(separatorRect, ThemeManager::instance().color(ThemeColor::Divider));
    }
}

void FriendDetailPage::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && avatarRect().contains(event->pos())) {
        openAvatarViewer();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

bool FriendDetailPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && m_remarkEdit->isEditing()) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget != m_remarkEdit && !m_remarkEdit->isAncestorOf(widget)) {
            saveRemark();
            m_remarkEdit->finishEditing();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FriendDetailPage::setUser(const User& user)
{
    if (user.id.isEmpty()) {
        clear();
        return;
    }

    m_user = user;
    m_hasUser = true;
    m_remarkEdit->finishEditing();

    m_nameLabel->setText(m_user.nick);
    m_idPrefixLabel->setVisible(true);
    m_idLabel->setText(m_user.id);
    m_copyIdButton->setEnabled(true);
    m_copyIdButton->setVisible(true);
    m_regionLabel->setText(m_user.region);
    m_regionRow->setVisible(!m_user.region.isEmpty());
    m_statusLabel->setText(statusText(m_user.status));
    updateAvatar();
    updateRemarkText();
    updateGroupButtonText();
    updateSignatureText();
    QTimer::singleShot(0, this, &FriendDetailPage::updateSignatureText);
}

void FriendDetailPage::openAvatarViewer()
{
    if (!m_hasUser || m_avatarSource.isEmpty()) {
        return;
    }

    QPixmap preview = ImageService::instance().circularAvatar(m_avatarSource, kAvatarSize, 1.0);
    if (preview.isNull()) {
        preview = ImageService::instance().pixmap(m_avatarSource);
    }
    if (preview.isNull()) {
        return;
    }

    auto* viewer = new ImageViewer(preview, m_avatarSource, window());
    m_avatarViewer = viewer;
    if (m_controller) {
        m_avatarImageRequestId = m_controller->requestUserAvatarImage(m_user.id);
    }
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

void FriendDetailPage::updateAvatar()
{
    m_avatarSource = m_user.avatarPath;
    const QPixmap statusPixmap = ImageService::instance().scaled(statusIconPath(m_user.status),
                                                                 QSize(14, 14),
                                                                 Qt::KeepAspectRatio,
                                                                 devicePixelRatioF());
    m_statusIcon->setPixmap(statusPixmap);
    update();
}

void FriendDetailPage::applyTheme()
{
    const QList<PaintedLabel*> labels = findChildren<PaintedLabel*>();
    for (PaintedLabel* label : labels) {
        const QVariant roleValue = label->property("themeTextRole");
        if (!roleValue.isValid()) {
            continue;
        }
        const auto role = static_cast<ThemeColor>(roleValue.toInt());
        label->setTextColor(ThemeManager::instance().color(role));
    }

    m_remarkEdit->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    m_remarkEdit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    m_remarkEdit->setHoverBackgroundColor(ThemeManager::instance().color(ThemeColor::ListHover));
    m_remarkEdit->setFocusBackgroundColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    m_remarkEdit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));

    applyPrimaryButtonStyle(m_messageButton);
    applyDangerOutlineButtonStyle(m_deleteButton);
    m_groupButton->update();
    m_copyIdButton->update();
    update();
}

void FriendDetailPage::updateRemarkText()
{
    if (m_remarkEdit->text() != m_user.remark) {
        m_remarkEdit->setText(m_user.remark);
    }
}

void FriendDetailPage::updateGroupButtonText()
{
    m_groupButton->setText(m_user.friendGroupName);
}

void FriendDetailPage::updateSignatureText()
{
    const QString signature = m_user.signature;
    if (signature.isEmpty()) {
        m_signatureLabel->clear();
        return;
    }

    const int availableWidth = qMax(0, m_signatureLabel->width());
    const QFontMetrics metrics(m_signatureLabel->font());
    m_signatureLabel->setText(metrics.elidedText(signature, Qt::ElideRight, availableWidth));
}

void FriendDetailPage::copyCurrentId()
{
    if (!m_hasUser || m_user.id.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(m_user.id);
    GlobalNotification::showSuccess(this, QStringLiteral("复制成功"));
}

void FriendDetailPage::saveRemark()
{
    if (!m_hasUser) {
        return;
    }

    const QString remark = m_remarkEdit->text().trimmed();
    if (remark == m_user.remark) {
        return;
    }

    m_user.remark = remark;
    if (!m_controller || !m_controller->saveFriend(m_user)) {
        return;
    }
    updateRemarkText();
}

void FriendDetailPage::showGroupMenu()
{
    if (!m_hasUser) {
        return;
    }

    static_cast<GroupSelectButton*>(m_groupButton)->setMenuHoverSuppressed(false);
    rebuildGroupMenu();
    m_groupMenu->setFixedWidth(m_groupButton->width());
    m_groupMenu->popup(m_groupButton->mapToGlobal(QPoint(0, m_groupButton->height())));
    m_groupButton->setDown(false);
    static_cast<GroupSelectButton*>(m_groupButton)->setMenuHoverSuppressed(true);
    m_groupButton->update();
}

void FriendDetailPage::rebuildGroupMenu()
{
    QMenu* menu = m_groupMenu;
    if (!menu) {
        return;
    }
    menu->clear();
    const QList<QActionGroup*> oldGroups = menu->findChildren<QActionGroup*>(QString(), Qt::FindDirectChildrenOnly);
    for (QActionGroup* oldGroup : oldGroups) {
        delete oldGroup;
    }

    auto* group = new QActionGroup(menu);
    group->setExclusive(true);

    auto addGroupAction = [this, menu, group](const QString& groupId, const QString& groupName) {
        QAction* action = menu->addAction(groupName);
        action->setCheckable(true);
        action->setChecked(groupId == m_user.friendGroupId);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, groupId, groupName]() {
            changeGroup(groupId, groupName);
        });
    };

    addGroupAction(m_user.friendGroupId, m_user.friendGroupName);
    const QMap<QString, QString> groups = m_controller
            ? m_controller->loadFriendGroups()
            : QMap<QString, QString>();
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if (it.key() == m_user.friendGroupId) {
            continue;
        }
        addGroupAction(it.key(), it.value());
    }

}

void FriendDetailPage::changeGroup(const QString& groupId, const QString& groupName)
{
    if (!m_hasUser || groupId == m_user.friendGroupId) {
        return;
    }

    m_user.friendGroupId = groupId;
    m_user.friendGroupName = groupName;
    if (!m_controller || !m_controller->saveFriend(m_user)) {
        return;
    }
    updateGroupButtonText();
}

void FriendDetailPage::confirmDeleteFriend()
{
    if (!m_hasUser) {
        return;
    }

    const InWindowPopup::Button result = InWindowPopup::question(this,
                                                                 QStringLiteral("删除好友"),
                                                                 QStringLiteral("确认删除该好友吗？"));
    if (result != InWindowPopup::Button::Yes) {
        return;
    }

    if (m_controller && m_controller->deleteFriend(m_user.id)) {
        clear();
        emit friendDeleted();
    }
}
