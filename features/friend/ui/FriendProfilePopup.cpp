#include "FriendProfilePopup.h"

#include <QApplication>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVariant>

#include "app/state/CurrentUser.h"
#include "app/state/CurrentUserProfile.h"
#include "features/friend/ui/FriendSessionController.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/ImageViewer.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"

namespace {

constexpr int kPopupWidth = 280;
constexpr int kHorizontalMargin = 28;
constexpr int kTopMargin = 30;
constexpr int kBottomMargin = 24;
constexpr int kAvatarSize = 64;
constexpr int kHeaderSpacing = 18;
constexpr int kIdentityTopInset = 3;
constexpr int kSeparatorTopSpacing = 24;
constexpr int kSeparatorBottomSpacing = 25;
constexpr int kTitleWidth = 72;
constexpr int kTitleValueSpacing = 14;
constexpr int kInfoRowSpacing = 17;
constexpr int kInfoRowMinHeight = 24;
constexpr int kInfoButtonSpacing = 14;
constexpr int kButtonWidth = 132;
constexpr int kButtonHeight = 36;

PaintedLabel* makeThemedLabel(ThemeColor role, int pixelSize, QWidget* parent)
{
    auto* label = new PaintedLabel(parent);
    QFont font = label->font();
    font.setPixelSize(pixelSize);
    label->setFont(font);
    label->setProperty("themeTextRole", static_cast<int>(role));
    label->setTextColor(ThemeManager::instance().color(role));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

PaintedLabel* makeTitleLabel(const QString& text, QWidget* parent)
{
    auto* label = makeThemedLabel(ThemeColor::SecondaryText, 13, parent);
    label->setText(text);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

QWidget* makeInfoRow(PaintedLabel* titleLabel, PaintedLabel* valueLabel, QWidget* parent)
{
    auto* rowWidget = new QWidget(parent);
    titleLabel->setParent(rowWidget);
    valueLabel->setParent(rowWidget);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
    valueLabel->setWordWrap(true);
    return rowWidget;
}

void applyPrimaryButtonStyle(StatefulPushButton* button)
{
    button->setRadius(8);
    button->setPrimaryStyle();
}

} // namespace

FriendProfilePopup::FriendProfilePopup(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_contentWidget(new QWidget(this))
    , m_nameLabel(makeThemedLabel(ThemeColor::PrimaryText, 18, this))
    , m_idLabel(makeThemedLabel(ThemeColor::TertiaryText, 12, this))
    , m_statusLabel(makeThemedLabel(ThemeColor::PrimaryText, 12, this))
    , m_regionTitleLabel(makeTitleLabel(QStringLiteral("地区"), this))
    , m_remarkTitleLabel(makeTitleLabel(QStringLiteral("备注"), this))
    , m_signatureTitleLabel(makeTitleLabel(QStringLiteral("签名"), this))
    , m_regionLabel(makeThemedLabel(ThemeColor::PrimaryText, 13, this))
    , m_remarkLabel(makeThemedLabel(ThemeColor::PrimaryText, 13, this))
    , m_signatureLabel(makeThemedLabel(ThemeColor::PrimaryText, 13, this))
    , m_statusIcon(new QLabel(this))
    , m_actionButton(new StatefulPushButton(this))
{
    setFixedWidth(kPopupWidth);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    m_contentWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    QFont nameFont = m_nameLabel->font();
    nameFont.setWeight(QFont::DemiBold);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_idLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_statusIcon->setFixedSize(12, 12);

    m_regionRow = makeInfoRow(m_regionTitleLabel, m_regionLabel, this);
    m_remarkRow = makeInfoRow(m_remarkTitleLabel, m_remarkLabel, this);
    m_signatureRow = makeInfoRow(m_signatureTitleLabel, m_signatureLabel, this);

    m_actionButton->resize(kButtonWidth, kButtonHeight);
    applyPrimaryButtonStyle(m_actionButton);

    connect(m_actionButton, &StatefulPushButton::clicked, this, [this]() {
        if (!m_hasUser) {
            return;
        }

        if (m_isCurrentUser) {
            emit requestEditProfile();
        } else if (m_user.isFriend) {
            emit requestMessage(m_user.id);
        } else {
            emit requestAddFriend(m_user.id);
        }
        hide();
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });
    connect(&CurrentUser::instance(), &CurrentUser::identityChanged, this, [this]() {
        if (m_isCurrentUser) {
            setUser(currentUserAsUser(), true);
        }
    });
    connect(&CurrentUser::instance(), &CurrentUser::profileChanged, this, [this]() {
        if (m_isCurrentUser) {
            setUser(currentUserAsUser(), true);
        }
    });

    applyTheme();
    clear();
}

FriendProfilePopup::~FriendProfilePopup() = default;

void FriendProfilePopup::setController(FriendSessionController* controller)
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

void FriendProfilePopup::popupAt(const QPoint& globalPos, const QString& userId)
{
    setUserId(userId);
    if (!m_hasUser) {
        return;
    }

    QPoint popupPos = globalPos;
    if (m_isCurrentUser) {
        popupPos.setX(globalPos.x() - width());
    }

    move(constrainedPopupPos(popupPos));
    show();
    raise();
    activateWindow();
}

void FriendProfilePopup::setUserId(const QString& userId)
{
    if (userId.isEmpty()) {
        clear();
        return;
    }

    if (CurrentUser::instance().isCurrentUserId(userId)) {
        setUser(currentUserAsUser(), true);
        return;
    }

    setUser(m_controller ? m_controller->loadFriend(userId) : User(), false);
}

void FriendProfilePopup::clear()
{
    m_user = {};
    m_hasUser = false;
    m_isCurrentUser = false;
    m_avatarSource.clear();
    m_avatarImageRequestId.clear();
    m_nameLabel->clear();
    m_idLabel->clear();
    m_statusLabel->clear();
    m_statusIcon->clear();
    m_regionRow->hide();
    m_remarkRow->hide();
    m_signatureRow->hide();
    m_actionButton->hide();
    updatePopupHeight();
    update();
}

void FriendProfilePopup::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutPopup();
    updateElidedTexts();
}

void FriendProfilePopup::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    layoutPopup();
    updateElidedTexts();
}

void FriendProfilePopup::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::instance().color(ThemeColor::PanelBackground));
    painter.drawRoundedRect(rect(), 8, 8);

    const QRect avatar = avatarRect();
    if (!m_avatarSource.isEmpty()) {
        const QPixmap avatarPixmap = ImageService::instance().circularAvatar(m_avatarSource,
                                                                             kAvatarSize,
                                                                             devicePixelRatioF());
        painter.drawPixmap(avatar, avatarPixmap);
    }

    const int separatorY = avatar.top() + kAvatarSize + kSeparatorTopSpacing;
    painter.fillRect(QRect(m_contentWidget->x(), separatorY, m_contentWidget->width(), 1),
                     ThemeManager::instance().color(ThemeColor::Divider));
}

void FriendProfilePopup::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && avatarRect().contains(event->pos())) {
        openAvatarViewer();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

QRect FriendProfilePopup::avatarRect() const
{
    if (!m_contentWidget || !m_nameLabel) {
        return {};
    }

    const QPoint nameTopLeft = m_nameLabel->mapTo(const_cast<FriendProfilePopup*>(this), QPoint(0, 0));
    const int avatarX = nameTopLeft.x() - kHeaderSpacing - kAvatarSize;
    const int avatarY = nameTopLeft.y() - kIdentityTopInset;
    return QRect(avatarX, avatarY, kAvatarSize, kAvatarSize);
}

void FriendProfilePopup::setUser(const User& user, bool isCurrentUser)
{
    if (user.id.isEmpty()) {
        clear();
        return;
    }

    m_user = user;
    m_hasUser = true;
    m_isCurrentUser = isCurrentUser;

    m_idLabel->setText(QStringLiteral("ID %1").arg(m_user.id));
    m_statusLabel->setText(statusText(m_user.status));
    updateAvatar();
    updateInfoRows();
    updateActionButton();
    updateElidedTexts();
    updatePopupHeight();
    updateElidedTexts();
    QTimer::singleShot(0, this, &FriendProfilePopup::updateElidedTexts);
}

void FriendProfilePopup::openAvatarViewer()
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
    if (m_controller && !m_isCurrentUser) {
        m_avatarImageRequestId = m_controller->requestUserAvatarImage(m_user.id);
    }
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

void FriendProfilePopup::updateAvatar()
{
    m_avatarSource = m_user.avatarPath;
    const QPixmap statusPixmap = ImageService::instance().scaled(statusIconPath(m_user.status),
                                                                 QSize(12, 12),
                                                                 Qt::KeepAspectRatio,
                                                                 devicePixelRatioF());
    m_statusIcon->setPixmap(statusPixmap);
    update();
}

void FriendProfilePopup::updateActionButton()
{
    if (!m_hasUser) {
        m_actionButton->hide();
        return;
    }

    m_actionButton->setText(m_isCurrentUser
                                    ? QStringLiteral("编辑资料")
                                    : (m_user.isFriend ? QStringLiteral("发消息")
                                                       : QStringLiteral("添加好友")));
    m_actionButton->show();
}

void FriendProfilePopup::updateInfoRows()
{
    const bool hasRegion = !m_user.region.trimmed().isEmpty();
    const bool showRemark = !m_isCurrentUser && m_user.isFriend;
    const bool hasSignature = !m_user.signature.trimmed().isEmpty();

    m_regionRow->setVisible(hasRegion);
    m_remarkRow->setVisible(showRemark);
    m_signatureRow->setVisible(hasSignature);
}

void FriendProfilePopup::updatePopupHeight()
{
    layoutPopup();
}

void FriendProfilePopup::layoutPopup()
{
    const int contentWidth = width() - kHorizontalMargin * 2;
    const int identityX = kHorizontalMargin + kAvatarSize + kHeaderSpacing;
    const int identityWidth = width() - identityX - kHorizontalMargin;
    const int nameY = kTopMargin + kIdentityTopInset;
    const int nameHeight = QFontMetrics(m_nameLabel->font()).height() + 4;
    const int idHeight = QFontMetrics(m_idLabel->font()).height() + 2;
    const int statusHeight = qMax(12, QFontMetrics(m_statusLabel->font()).height() + 2);

    m_contentWidget->setGeometry(kHorizontalMargin, 0, contentWidth, height());
    m_nameLabel->setGeometry(identityX, nameY, identityWidth, nameHeight);
    m_idLabel->setGeometry(identityX, m_nameLabel->geometry().bottom() + 5,
                           identityWidth, idHeight);
    m_statusIcon->setGeometry(identityX, m_idLabel->geometry().bottom() + 7, 12, 12);
    m_statusLabel->setGeometry(m_statusIcon->geometry().right() + 6,
                               m_statusIcon->y() - 1,
                               identityWidth - 18,
                               statusHeight);

    const int rowWidth = contentWidth;
    const int valueX = kTitleWidth + kTitleValueSpacing;
    const int valueWidth = rowWidth - valueX;
    int nextRowY = kTopMargin + kAvatarSize + kSeparatorTopSpacing + kSeparatorBottomSpacing;

    auto valueHeight = [valueWidth](PaintedLabel* label) {
        if (!label || label->isHidden()) {
            return 0;
        }

        const QFontMetrics metrics(label->font());
        const QRect bounds = metrics.boundingRect(QRect(0, 0, valueWidth, 10000),
                                                  Qt::TextWordWrap | Qt::TextWrapAnywhere,
                                                  label->text());
        return qMax(kInfoRowMinHeight, bounds.height() + 2);
    };

    auto placeRow = [&](QWidget* row, PaintedLabel* titleLabel, PaintedLabel* valueLabel) {
        if (!row || row->isHidden()) {
            return;
        }

        const int rowHeight = qMax(kInfoRowMinHeight, valueHeight(valueLabel));
        row->setGeometry(kHorizontalMargin, nextRowY, rowWidth, rowHeight);
        titleLabel->setGeometry(0, 1, kTitleWidth, rowHeight);
        valueLabel->setGeometry(valueX, 0, valueWidth, rowHeight);
        nextRowY += rowHeight + kInfoRowSpacing;
    };

    const int rowsStartY = nextRowY;
    placeRow(m_regionRow, m_regionTitleLabel, m_regionLabel);
    placeRow(m_remarkRow, m_remarkTitleLabel, m_remarkLabel);
    placeRow(m_signatureRow, m_signatureTitleLabel, m_signatureLabel);
    const bool hasRows = nextRowY != rowsStartY;
    if (hasRows) {
        nextRowY -= kInfoRowSpacing;
    }

    const int buttonY = nextRowY + (hasRows ? kInfoButtonSpacing : 0);
    m_actionButton->setGeometry((width() - kButtonWidth) / 2, buttonY,
                                kButtonWidth, kButtonHeight);

    const int popupHeight = buttonY + kButtonHeight + kBottomMargin;
    if (height() != popupHeight) {
        resize(kPopupWidth, popupHeight);
        return;
    }
    update();
}

void FriendProfilePopup::updateElidedTexts()
{
    if (!m_hasUser) {
        return;
    }

    const auto elided = [](PaintedLabel* label, const QString& text) {
        const int availableWidth = qMax(0, label->width());
        const QFontMetrics metrics(label->font());
        label->setText(metrics.elidedText(text, Qt::ElideRight, availableWidth));
    };

    elided(m_nameLabel, m_user.nick.isEmpty() ? m_user.id : m_user.nick);
    elided(m_idLabel, QStringLiteral("ID %1").arg(m_user.id));
    elided(m_statusLabel, statusText(m_user.status));
    m_regionLabel->setText(m_user.region.trimmed());
    m_remarkLabel->setText(m_user.remark.trimmed().isEmpty()
                                   ? QStringLiteral("未设置")
                                   : m_user.remark.trimmed());
    m_signatureLabel->setText(m_user.signature.trimmed());
}

void FriendProfilePopup::applyTheme()
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

    applyPrimaryButtonStyle(m_actionButton);
    update();
}

User FriendProfilePopup::currentUserAsUser() const
{
    const CurrentUserProfile profile = CurrentUser::instance().profile();

    User user;
    user.id = profile.userId;
    user.nick = profile.nickName;
    user.avatarPath = profile.avatarPath;
    user.status = profile.status;
    user.signature = profile.signature;
    user.region = profile.region;
    user.isFriend = false;
    return user;
}

QPoint FriendProfilePopup::constrainedPopupPos(const QPoint& globalPos) const
{
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return globalPos;
    }

    const QRect available = screen->availableGeometry().adjusted(8, 8, -8, -8);
    QPoint pos = globalPos;
    pos.setX(qBound(available.left(), pos.x(), available.right() - width() + 1));
    pos.setY(qBound(available.top(), pos.y(), available.bottom() - height() + 1));
    return pos;
}
