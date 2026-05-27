#include "AddContactSearchWindow.h"

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/OverlayScrollListView.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/QtFallbackLiquidGlass.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/TransparentTextEdit.h"

#ifdef Q_OS_WIN
#include "platform/windows/WindowsWindowControlButton.h"
#endif

#include <QAbstractButton>
#include <QActionGroup>
#include <QApplication>
#include <QAbstractItemView>
#include <QCursor>
#include <QEventLoop>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

#ifdef Q_OS_MACOS
#include "platform/macos/MacPostBarBridge_p.h"
#endif

namespace {

constexpr auto kSystemFloatingBarsSuppressedProperty = "systemFloatingBarsSuppressed";
constexpr auto kUsesSystemFloatingBarBridgeProperty = "usesSystemFloatingBarBridge";
constexpr int kWindowWidth = 520;
constexpr int kWindowHeight = 640;
constexpr int kTitleTopMargin = 22;
constexpr int kTitleHeight = 24;
constexpr int kTitleSearchGap = 14;
constexpr int kSearchHeight = 34;
constexpr int kHorizontalMargin = 24;
constexpr int kDividerTopGap = 18;
constexpr int kModeBarWidth = 220;
constexpr int kModeBarHeight = 44;
constexpr int kModeBarBottomMargin = 18;
constexpr int kModeBarCornerRadius = 15;
constexpr int kSearchDebounceMs = 260;
constexpr int kSearchResultLimit = 240;
constexpr int kSearchPageSize = 10;
constexpr int kLoadMoreThresholdPx = 28;
constexpr int kActionButtonWidth = 58;
constexpr int kActionButtonHeight = 28;
constexpr int kActionButtonRightMargin = 24;
constexpr int kActionButtonTextGap = 16;
constexpr int kActionButtonRadius = 9;
constexpr int kUserAvatarSize = 36;
constexpr int kGroupAvatarSize = 38;
constexpr int kAvatarLeftMargin = 22;
constexpr int kRequestPopupAvatarSize = 72;
constexpr int kRequestPopupPadding = 24;
constexpr int kRequestPopupTitleWidth = 96;
constexpr int kRequestPopupRowSpacing = 24;
constexpr int kRequestPopupInputHeight = 34;
constexpr int kRequestPopupMessageHeight = 86;
constexpr int kRequestPopupButtonWidth = 92;
constexpr int kRequestPopupButtonHeight = 34;

QString userDisplayText(const User& user)
{
    if (user.id.isEmpty()) {
        return {};
    }
    return QStringLiteral("%1（%2）").arg(user.nick, user.id);
}

bool currentUserHasJoinedGroup(const Group& group)
{
    const QString currentUserId = CurrentUser::instance().getUserId();
    return !currentUserId.isEmpty()
            && (group.ownerId == currentUserId
                || group.adminsID.contains(currentUserId)
                || group.membersID.contains(currentUserId));
}

const QStringList& editableCategoryOrder()
{
    static const QStringList ids = {
            QStringLiteral("gg_joined"),
            QStringLiteral("gg_college"),
            QStringLiteral("gg_work"),
            QStringLiteral("gg_performance")
    };
    return ids;
}

PaintedLabel* makePopupLabel(const QString& text, ThemeColor role, int pixelSize, QWidget* parent = nullptr)
{
    auto* label = new PaintedLabel(text, parent);
    QFont font = label->font();
    font.setPixelSize(pixelSize);
    label->setFont(font);
    label->setTextColor(ThemeManager::instance().color(role));
    label->setProperty("themeTextRole", static_cast<int>(role));
    return label;
}

QHBoxLayout* makePopupInfoRow(const QString& title, QWidget* valueWidget)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kRequestPopupRowSpacing);

    auto* titleLabel = makePopupLabel(title, ThemeColor::PrimaryText, 14);
    titleLabel->setFixedWidth(kRequestPopupTitleWidth);
    row->addWidget(titleLabel, 0, Qt::AlignTop);
    row->addWidget(valueWidget, 1);
    return row;
}

void configurePopupLineEdit(InlineEditableText* edit, Qt::Alignment alignment)
{
    edit->setFixedHeight(kRequestPopupInputHeight);
    edit->setAlignment(alignment);
    edit->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    edit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    edit->setNormalBackgroundColor(Qt::transparent);
    edit->setHoverBackgroundColor(ThemeManager::instance().color(ThemeColor::ListHover));
    edit->setFocusBackgroundColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    edit->setNormalBorderColor(Qt::transparent);
    edit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    edit->setSelectionBackgroundColor(ThemeManager::instance().color(ThemeColor::AccentTextSelection));
    edit->setBorderWidth(1);
    edit->setRadius(5);
    edit->setHorizontalPadding(8);
}

class AvatarPreview final : public QWidget
{
public:
    explicit AvatarPreview(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(kRequestPopupAvatarSize, kRequestPopupAvatarSize);
    }

    void setSource(const QString& source)
    {
        m_source = source;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (m_source.isEmpty()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(ThemeManager::instance().color(ThemeColor::PanelRaisedBackground));
            painter.drawEllipse(rect());
            return;
        }

        const QPixmap pixmap = ImageService::instance().circularAvatar(m_source,
                                                                       kRequestPopupAvatarSize,
                                                                       devicePixelRatioF());
        if (!pixmap.isNull()) {
            painter.drawPixmap(rect(), pixmap);
        }
    }

private:
    QString m_source;
};

class PopupSelectButton final : public QToolButton
{
public:
    explicit PopupSelectButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setFixedSize(172, kRequestPopupInputHeight);
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

class RequestMessageBox final : public QWidget
{
public:
    explicit RequestMessageBox(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_edit(new TransparentTextEdit(this))
    {
        setFixedHeight(kRequestPopupMessageHeight);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(0);
        layout->addWidget(m_edit);
        m_edit->setAcceptRichText(false);
        m_edit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        m_edit->setViewportPadding(0, 0, 0, 0);
        updatePalette();
    }

    QTextEdit* editor() const { return m_edit; }
    QString text() const { return m_edit->toPlainText(); }
    void setText(const QString& text) { m_edit->setPlainText(text); }
    void setPlaceholderText(const QString& text) { m_edit->setPlaceholderText(text); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1));
        painter.setBrush(ThemeManager::instance().color(ThemeColor::InputBackground));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);
    }

private:
    void updatePalette()
    {
        QPalette palette = m_edit->palette();
        palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
        palette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::PlaceholderText));
        palette.setColor(QPalette::Base, Qt::transparent);
        m_edit->setPalette(palette);
        m_edit->setFont(AppFonts::applicationPixelSizedFont(14));
    }

    TransparentTextEdit* m_edit = nullptr;
};

struct ContactRequestData {
    bool accepted = false;
    QString requestMessage;
    QString remark;
    QString groupId;
    QString groupName;
};

InWindowPopupOverlay* overlayFor(QWidget* widget)
{
    QWidget* cursor = widget;
    while (cursor) {
        if (auto* overlay = qobject_cast<InWindowPopupOverlay*>(cursor)) {
            return overlay;
        }
        cursor = cursor->parentWidget();
    }
    return nullptr;
}

QWidget* createRequestPopupContent(const QString& title,
                                   const QString& avatarPath,
                                   const QString& displayName,
                                   const QString& idText,
                                   const QString& requestPlaceholder,
                                   const QString& defaultRemark,
                                   const QMap<QString, QString>& groups,
                                   const QString& currentGroupId,
                                   const QString& currentGroupName,
                                   const QString& remarkTitle,
                                   const QString& groupTitle,
                                   bool showRequestMessage,
                                   const QString& confirmText,
                                   ContactRequestData* result)
{
    auto* content = new QWidget;
    content->setMinimumWidth(430);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kRequestPopupPadding,
                               kRequestPopupPadding,
                               kRequestPopupPadding,
                               kRequestPopupPadding);
    layout->setSpacing(14);

    auto* titleLabel = makePopupLabel(title, ThemeColor::PrimaryText, 18, content);
    QFont titleFont = titleLabel->font();
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 2, 0, 0);
    header->setSpacing(16);
    auto* avatar = new AvatarPreview(content);
    avatar->setSource(avatarPath);
    header->addWidget(avatar, 0, Qt::AlignTop);

    auto* identity = new QVBoxLayout;
    identity->setContentsMargins(0, 5, 0, 0);
    identity->setSpacing(6);
    auto* nameLabel = makePopupLabel(displayName, ThemeColor::PrimaryText, 18, content);
    QFont nameFont = nameLabel->font();
    nameFont.setWeight(QFont::DemiBold);
    nameLabel->setFont(nameFont);
    auto* idLabel = makePopupLabel(idText, ThemeColor::TertiaryText, 13, content);
    identity->addWidget(nameLabel);
    identity->addWidget(idLabel);
    identity->addStretch();
    header->addLayout(identity, 1);
    layout->addLayout(header);

    RequestMessageBox* messageBox = nullptr;
    if (showRequestMessage) {
        messageBox = new RequestMessageBox(content);
        messageBox->setPlaceholderText(requestPlaceholder);
        messageBox->setText(QStringLiteral("我是%1").arg(CurrentUser::instance().getUserName()));
        layout->addLayout(makePopupInfoRow(QStringLiteral("申请信息"), messageBox));
    }

    auto* remarkEdit = new InlineEditableText(content);
    configurePopupLineEdit(remarkEdit, Qt::AlignRight | Qt::AlignVCenter);
    remarkEdit->setPlaceholderText(remarkTitle == QStringLiteral("备注")
                                   ? QStringLiteral("设置备注")
                                   : QStringLiteral("设置群备注"));
    remarkEdit->setText(defaultRemark);
    layout->addLayout(makePopupInfoRow(remarkTitle, remarkEdit));

    auto* selectHost = new QWidget(content);
    auto* selectLayout = new QHBoxLayout(selectHost);
    selectLayout->setContentsMargins(0, 0, 0, 0);
    selectLayout->addStretch();
    auto* groupButton = new PopupSelectButton(selectHost);
    auto* groupMenu = new StyledActionMenu(groupButton);
    groupMenu->setItemHoverColor(ThemeManager::instance().color(ThemeColor::ContextMenuHover));
    result->groupId = currentGroupId;
    result->groupName = currentGroupName;
    groupButton->setText(currentGroupName);
    selectLayout->addWidget(groupButton, 0, Qt::AlignRight);
    layout->addLayout(makePopupInfoRow(groupTitle, selectHost));

    auto rebuildMenu = [groupMenu, groupButton, groups, result]() {
        groupMenu->clear();
        const QList<QActionGroup*> oldGroups = groupMenu->findChildren<QActionGroup*>(
                QString(), Qt::FindDirectChildrenOnly);
        for (QActionGroup* oldGroup : oldGroups) {
            delete oldGroup;
        }
        auto* actionGroup = new QActionGroup(groupMenu);
        actionGroup->setExclusive(true);
        for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
            QAction* action = groupMenu->addAction(it.value());
            action->setCheckable(true);
            action->setChecked(it.key() == result->groupId);
            actionGroup->addAction(action);
            QObject::connect(action, &QAction::triggered, groupButton, [groupButton, result, id = it.key(), name = it.value()]() {
                result->groupId = id;
                result->groupName = name;
                groupButton->setText(name);
            });
        }
    };
    QObject::connect(groupButton, &QToolButton::clicked, content, [groupButton, groupMenu, rebuildMenu]() {
        static_cast<PopupSelectButton*>(groupButton)->setMenuHoverSuppressed(false);
        rebuildMenu();
        groupMenu->setFixedWidth(groupButton->width());
        groupMenu->popup(groupButton->mapToGlobal(QPoint(0, groupButton->height())));
        groupButton->setDown(false);
        static_cast<PopupSelectButton*>(groupButton)->setMenuHoverSuppressed(true);
        groupButton->update();
    });
    QObject::connect(groupMenu, &QMenu::aboutToHide, groupButton, [groupButton]() {
        static_cast<PopupSelectButton*>(groupButton)->setMenuHoverSuppressed(true);
        groupButton->update();
    });

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();
    auto* cancelButton = new StatefulPushButton(QStringLiteral("取消"), content);
    auto* okButton = new StatefulPushButton(confirmText, content);
    cancelButton->setFixedSize(kRequestPopupButtonWidth, kRequestPopupButtonHeight);
    okButton->setFixedSize(kRequestPopupButtonWidth, kRequestPopupButtonHeight);
    cancelButton->setDefaultStyle();
    cancelButton->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    cancelButton->setBorderWidth(1);
    okButton->setPrimaryStyle();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    QObject::connect(cancelButton, &QPushButton::clicked, content, [content]() {
        if (auto* overlay = overlayFor(content)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    });
    QObject::connect(okButton, &QPushButton::clicked, content, [content, result, messageBox, remarkEdit]() {
        result->accepted = true;
        result->requestMessage = messageBox ? messageBox->text().trimmed() : QString();
        result->remark = remarkEdit->text().trimmed();
        if (auto* overlay = overlayFor(content)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    });
    if (messageBox) {
        QTimer::singleShot(0, messageBox->editor(), [editor = messageBox->editor()]() {
            editor->setFocus();
        });
    } else {
        QTimer::singleShot(0, remarkEdit, [remarkEdit]() {
            remarkEdit->startEditing();
        });
    }

    return content;
}

ContactRequestData showRequestPopup(QWidget* parent,
                                    const QString& title,
                                    const QString& avatarPath,
                                    const QString& displayName,
                                    const QString& idText,
                                    const QString& requestPlaceholder,
                                    const QString& defaultRemark,
                                    const QMap<QString, QString>& groups,
                                    const QString& currentGroupId,
                                    const QString& currentGroupName,
                                    const QString& remarkTitle,
                                    const QString& groupTitle,
                                    bool showRequestMessage,
                                    const QString& confirmText)
{
    ContactRequestData result;
    QWidget* content = createRequestPopupContent(title,
                                                 avatarPath,
                                                 displayName,
                                                 idText,
                                                 requestPlaceholder,
                                                 defaultRemark,
                                                 groups,
                                                 currentGroupId,
                                                 currentGroupName,
                                                 remarkTitle,
                                                 groupTitle,
                                                 showRequestMessage,
                                                 confirmText,
                                                 &result);
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(520, 520);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content, options);
    if (!overlay) {
        content->deleteLater();
        return result;
    }

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();
    return result;
}

bool applyFriendRequest(const User& user, const ContactRequestData& request)
{
    if (user.id.isEmpty() || !request.accepted) {
        return false;
    }

    User nextUser = user;
    nextUser.isFriend = true;
    nextUser.remark = request.remark;
    nextUser.friendGroupId = request.groupId.isEmpty() ? QStringLiteral("default") : request.groupId;
    nextUser.friendGroupName = request.groupName.isEmpty() ? QStringLiteral("默认分组") : request.groupName;
    UserRepository::instance().saveUser(nextUser);
    return true;
}

bool applyGroupRequest(const Group& group, const ContactRequestData& request)
{
    if (group.groupId.isEmpty() || !request.accepted) {
        return false;
    }

    Group nextGroup = group;
    const QString currentUserId = CurrentUser::instance().getUserId();
    if (!currentUserId.isEmpty()
            && nextGroup.ownerId != currentUserId
            && !nextGroup.adminsID.contains(currentUserId)
            && !nextGroup.membersID.contains(currentUserId)) {
        nextGroup.membersID.push_back(currentUserId);
    }
    nextGroup.memberNum = nextGroup.membersID.size();
    nextGroup.remark = request.remark;
    nextGroup.currentUserNickname = CurrentUser::instance().getUserName();
    if (!currentUserId.isEmpty()) {
        nextGroup.memberNicknames.insert(currentUserId, nextGroup.currentUserNickname);
    }
    nextGroup.listGroupId = request.groupId.isEmpty() ? QStringLiteral("gg_joined") : request.groupId;
    nextGroup.listGroupName = request.groupName.isEmpty() ? QStringLiteral("我加入的群聊") : request.groupName;
    GroupRepository::instance().saveGroup(nextGroup);
    return true;
}

QString searchPlaceholderForMode(AddContactModeBar::Mode mode)
{
    return mode == AddContactModeBar::Mode::Users
            ? QStringLiteral("搜索昵称或 ID")
            : QStringLiteral("搜索群名或群号");
}

QString titleTextForMode(AddContactModeBar::Mode mode)
{
    return mode == AddContactModeBar::Mode::Users
            ? QStringLiteral("添加好友")
            : QStringLiteral("添加群聊");
}

QPixmap avatarPixmap(const QString& path, const QSize& size, qreal dpr)
{
    if (path.isEmpty()) {
        return {};
    }
    return ImageService::instance().scaled(path, size, Qt::KeepAspectRatioByExpanding, dpr);
}

void drawRoundPixmap(QPainter* painter, const QRect& rect, const QPixmap& pixmap, qreal radius)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter->setClipPath(path);
    painter->drawPixmap(rect, pixmap);
    painter->restore();
}

QRect actionButtonRect(const QStyleOptionViewItem& option)
{
    return QRect(option.rect.right() - kActionButtonRightMargin - kActionButtonWidth + 1,
                 option.rect.top() + (option.rect.height() - kActionButtonHeight) / 2,
                 kActionButtonWidth,
                 kActionButtonHeight);
}

void drawActionButton(QPainter* painter, const QRect& rect, const QString& text, bool hovered)
{
    QColor background = ThemeManager::instance().color(hovered
            ? ThemeColor::AccentHover
            : ThemeColor::Accent);
    painter->setPen(QPen(background, 1.5));
    painter->setBrush(background);
    painter->drawRoundedRect(rect, kActionButtonRadius, kActionButtonRadius);

    painter->setFont(AppFonts::applicationPixelWeightedFont(12, QFont::Medium));
    painter->setPen(ThemeManager::textColorOn(background));
    painter->drawText(rect, Qt::AlignCenter, text);
}

void drawActionButton(QPainter* painter, const QRect& rect, const QString& text, bool hovered, bool enabled)
{
    if (enabled) {
        drawActionButton(painter, rect, text, hovered);
        return;
    }

    const QColor background = ThemeManager::instance().color(ThemeColor::PanelRaisedBackground);
    painter->setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1));
    painter->setBrush(background);
    painter->drawRoundedRect(rect, kActionButtonRadius, kActionButtonRadius);
    painter->setFont(AppFonts::applicationPixelWeightedFont(12, QFont::Medium));
    painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
    painter->drawText(rect, Qt::AlignCenter, text);
}

} // namespace

AddContactSearchModel::AddContactSearchModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AddContactSearchModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant AddContactSearchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const AddContactSearchItem& item = m_items.at(index.row());
    switch (role) {
    case TypeRole:
        return item.type == AddContactSearchItem::Type::User ? 0 : 1;
    case UserIdRole:
        return item.user.id;
    case UserNameRole:
        return userDisplayText(item.user);
    case UserAvatarRole:
        return item.user.avatarPath;
    case GroupIdRole:
        return item.group.groupId;
    case GroupNameRole:
        return item.group.groupName;
    case GroupAvatarRole:
        return item.group.groupAvatarPath;
    case GroupMemberCountRole:
        return item.group.memberNum;
    case GroupIntroductionRole:
        return item.group.introduction;
    case ActionTextRole:
        if (item.type == AddContactSearchItem::Type::User) {
            return item.user.isFriend ? QStringLiteral("已添加") : QStringLiteral("添加");
        }
        return currentUserHasJoinedGroup(item.group) ? QStringLiteral("已加入") : QStringLiteral("加入");
    case ActionEnabledRole:
        if (item.type == AddContactSearchItem::Type::User) {
            return !item.user.isFriend;
        }
        return !currentUserHasJoinedGroup(item.group);
    case Qt::DisplayRole:
        return item.type == AddContactSearchItem::Type::User
                ? userDisplayText(item.user)
                : item.group.groupName;
    case Qt::SizeHintRole:
        if (item.type == AddContactSearchItem::Type::User) {
            return QSize(0, 62);
        }
        return item.group.introduction.trimmed().isEmpty() ? QSize(0, 72) : QSize(0, 92);
    default:
        return {};
    }
}

Qt::ItemFlags AddContactSearchModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void AddContactSearchModel::setItems(QVector<AddContactSearchItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

void AddContactSearchModel::appendItems(const QVector<AddContactSearchItem>& items)
{
    if (items.isEmpty()) {
        return;
    }

    const int firstRow = m_items.size();
    const int lastRow = firstRow + items.size() - 1;
    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_items += items;
    endInsertRows();
}

void AddContactSearchModel::clear()
{
    setItems({});
}

AddContactSearchItem AddContactSearchModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    return m_items.at(row);
}

AddContactSearchDelegate::AddContactSearchDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        if (auto* view = qobject_cast<QWidget*>(QObject::parent())) {
            view->update();
        }
        if (auto* itemView = qobject_cast<QAbstractItemView*>(QObject::parent())) {
            itemView->viewport()->update();
        }
    });
}

QSize AddContactSearchDelegate::sizeHint(const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const
{
    Q_UNUSED(option);
    return index.data(Qt::SizeHintRole).toSize();
}

void AddContactSearchDelegate::paint(QPainter* painter,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);

    const QColor panelBackground = ThemeManager::instance().color(ThemeColor::PanelBackground);
    const QColor hoverColor = ThemeManager::instance().color(ThemeColor::ListHover);
    const QColor primaryText = ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor secondaryText = ThemeManager::instance().color(ThemeColor::SecondaryText);
    const QColor tertiaryText = ThemeManager::instance().color(ThemeColor::TertiaryText);

    painter->fillRect(option.rect, panelBackground);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool actionEnabled = index.data(AddContactSearchModel::ActionEnabledRole).toBool();
    const QString actionText = index.data(AddContactSearchModel::ActionTextRole).toString();
    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? hoverColor.darker(ThemeManager::instance().isDark() ? 112 : 104)
                                   : hoverColor);
        painter->drawRoundedRect(option.rect.adjusted(10, 5, -10, -5), 8, 8);
    }

    const int type = index.data(AddContactSearchModel::TypeRole).toInt();
    const QRect buttonRect = actionButtonRect(option);
    const bool actionHovered = actionEnabled && hovered && buttonRect.contains(option.widget
            ? option.widget->mapFromGlobal(QCursor::pos())
            : QPoint(-1, -1));
    if (type == 0) {
        const QRect avatarRect(option.rect.left() + kAvatarLeftMargin,
                               option.rect.top() + (option.rect.height() - kUserAvatarSize) / 2,
                               kUserAvatarSize,
                               kUserAvatarSize);
        const QPixmap avatar = avatarPixmap(index.data(AddContactSearchModel::UserAvatarRole).toString(),
                                            avatarRect.size(),
                                            painter->device()->devicePixelRatioF());
        if (!avatar.isNull()) {
            drawRoundPixmap(painter, avatarRect, avatar, 8);
        }

        painter->setFont(AppFonts::applicationPixelSizedFont(14));
        painter->setPen(primaryText);
        const int textLeft = avatarRect.right() + 14;
        const int textRight = buttonRect.left() - kActionButtonTextGap;
        const QRect textRect(textLeft,
                             option.rect.top(),
                             qMax(0, textRight - textLeft + 1),
                             option.rect.height());
        const QFontMetrics metrics(painter->font());
        painter->drawText(textRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(index.data(AddContactSearchModel::UserNameRole).toString(),
                                             Qt::ElideRight,
                                             textRect.width()));
        drawActionButton(painter, buttonRect, actionText, actionHovered, actionEnabled);
        painter->restore();
        return;
    }

    const QRect avatarRect(option.rect.left() + kAvatarLeftMargin,
                           option.rect.top() + (option.rect.height() - kGroupAvatarSize) / 2,
                           kGroupAvatarSize,
                           kGroupAvatarSize);
    const QPixmap avatar = avatarPixmap(index.data(AddContactSearchModel::GroupAvatarRole).toString(),
                                        avatarRect.size(),
                                        painter->device()->devicePixelRatioF());
    if (!avatar.isNull()) {
        drawRoundPixmap(painter, avatarRect, avatar, 9);
    }

    const int textLeft = avatarRect.right() + 14;
    const int textRight = buttonRect.left() - kActionButtonTextGap;
    const int textWidth = qMax(0, textRight - textLeft + 1);

    painter->setFont(AppFonts::applicationPixelWeightedFont(14, QFont::Medium));
    painter->setPen(primaryText);
    QFontMetrics nameMetrics(painter->font());
    const QRect nameRect(textLeft, option.rect.top() + 12, textWidth, 20);
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(AddContactSearchModel::GroupNameRole).toString(),
                                             Qt::ElideRight,
                                             textWidth));

    const QRect iconRect(textLeft, option.rect.top() + 38, 14, 14);
    painter->drawPixmap(iconRect,
                        ImageService::instance().scaled(QStringLiteral(":/resources/icon/friend_selected.png"),
                                                        iconRect.size(),
                                                        Qt::IgnoreAspectRatio,
                                                        painter->device()->devicePixelRatioF()));
    painter->setFont(AppFonts::applicationPixelSizedFont(12));
    painter->setPen(secondaryText);
    const QString countText = QStringLiteral("%1人").arg(index.data(AddContactSearchModel::GroupMemberCountRole).toInt());
    painter->drawText(QRect(iconRect.right() + 5, iconRect.top() - 1, textWidth - iconRect.width() - 5, 17),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      countText);

    const QString introduction = index.data(AddContactSearchModel::GroupIntroductionRole).toString().trimmed();
    if (!introduction.isEmpty()) {
        painter->setPen(tertiaryText);
        const QRect introRect(textLeft, option.rect.top() + 61, textWidth, 18);
        QFontMetrics introMetrics(painter->font());
        painter->drawText(introRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          introMetrics.elidedText(introduction, Qt::ElideRight, textWidth));
    }
    drawActionButton(painter, buttonRect, actionText, actionHovered, actionEnabled);
    painter->restore();
}

bool AddContactSearchDelegate::editorEvent(QEvent* event,
                                           QAbstractItemModel* model,
                                           const QStyleOptionViewItem& option,
                                           const QModelIndex& index)
{
    Q_UNUSED(model);
    if (!index.isValid()) {
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton
                && actionButtonRect(option).contains(mouseEvent->pos())) {
            if (event->type() == QEvent::MouseButtonRelease
                    && index.data(AddContactSearchModel::ActionEnabledRole).toBool()) {
                emit actionRequested(index);
            }
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::MouseMove && option.widget) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        auto* widget = const_cast<QWidget*>(option.widget);
        if (index.data(AddContactSearchModel::ActionEnabledRole).toBool()
                && actionButtonRect(option).contains(mouseEvent->pos())) {
            widget->setCursor(Qt::PointingHandCursor);
        } else {
            widget->unsetCursor();
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

AddContactModeBar::AddContactModeBar(QWidget* parent)
    : QWidget(parent)
    , m_liquidGlass(new QtFallbackLiquidGlassController(this))
    , m_selectionAnimation(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_segments = {
            {QStringLiteral("用户"), QRect()},
            {QStringLiteral("群聊"), QRect()}
    };

    m_liquidGlass->setEffectMode(QtFallbackLiquidGlassController::EffectMode::GaussianBlur);
    m_liquidGlass->setShape(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            kModeBarCornerRadius);

#ifdef Q_OS_MACOS
    m_usesNativeBar = MacPostBarBridge::appearance() != MacPostBarBridge::Appearance::Unsupported;
    setProperty(kUsesSystemFloatingBarBridgeProperty, m_usesNativeBar);
#endif

    m_selectionAnimation->setDuration(240);
    m_selectionAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_selectionAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_highlightX = value.toInt();
        if (!m_selectedRect.isEmpty()) {
            m_selectedRect.moveLeft(m_highlightX);
        }
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        syncPlatformBar();
        updatePanelShadow();
        updateQtFallbackLiquidGlassState();
        update();
    });

    resize(sizeHint());
    layoutSegments();
    updatePanelShadow();
}

AddContactModeBar::~AddContactModeBar()
{
    releaseQtFallbackLiquidGlassResources(false);
#ifdef Q_OS_MACOS
    if (m_usesNativeBar) {
        MacPostBarBridge::clearBar(this);
    }
#endif
}

bool AddContactModeBar::event(QEvent* event)
{
    const bool handled = QWidget::event(event);
#ifdef Q_OS_MACOS
    if (m_usesNativeBar) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WinIdChange:
        case QEvent::ParentChange:
        case QEvent::ZOrderChange:
            syncPlatformBar();
            break;
        case QEvent::Hide:
            MacPostBarBridge::clearBar(this);
            break;
        default:
            break;
        }
        return handled;
    }
#endif

    switch (event->type()) {
    case QEvent::Show:
        updateQtFallbackLiquidGlassState();
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
        scheduleLiquidGlassUpdate(QtFallbackLiquidGlassController::refreshDelayMs());
        break;
    case QEvent::Hide:
        releaseQtFallbackLiquidGlassResources();
        break;
    default:
        break;
    }
    if (m_liquidGlass) {
        m_liquidGlass->handleHostEvent(event);
    }

    return handled;
}

void AddContactModeBar::setMode(Mode mode, bool animate)
{
    setSelectedIndex(mode == Mode::Users ? 0 : 1, animate);
}

void AddContactModeBar::setLiquidGlassSourceWidget(QWidget* widget)
{
    if (m_liquidGlass) {
        m_liquidGlass->setSourceWidget(widget);
        updateQtFallbackLiquidGlassState();
    }
}

void AddContactModeBar::scheduleLiquidGlassUpdate(int delayMs)
{
    if (m_liquidGlass && shouldUseQtFallbackLiquidGlass()) {
        m_liquidGlass->scheduleUpdate(delayMs);
    }
}

void AddContactModeBar::paintEvent(QPaintEvent*)
{
    if (m_usesNativeBar) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);
    painter.setPen(Qt::NoPen);

    const QRectF contentRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (shouldUseQtFallbackLiquidGlass() && m_liquidGlass) {
        m_liquidGlass->setShape(contentRect, kModeBarCornerRadius);
        m_liquidGlass->paint(painter);
    } else {
        QColor background = ThemeManager::instance().color(ThemeColor::PanelBackground);
        background.setAlpha(ThemeManager::instance().isDark() ? 190 : 205);
        painter.setBrush(background);
        painter.drawRoundedRect(contentRect, kModeBarCornerRadius, kModeBarCornerRadius);
    }

    if (!m_selectedRect.isEmpty()) {
        painter.setBrush(ThemeManager::instance().postBarItemSelectedBackgroundColor());
        painter.drawRoundedRect(m_selectedRect, 10, 10);
    }

    painter.setFont(AppFonts::applicationPixelSizedFont(13));
    for (int i = 0; i < m_segments.size(); ++i) {
        painter.setPen(i == selectedIndex()
                       ? ThemeManager::instance().color(ThemeColor::PrimaryText)
                       : ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter.drawText(m_segments.at(i).rect, Qt::AlignCenter, m_segments.at(i).label);
    }

    if (!shouldUseQtFallbackLiquidGlass()) {
        QPen border(ThemeManager::instance().color(ThemeColor::Accent), 2.0);
        border.setJoinStyle(Qt::RoundJoin);
        painter.setPen(border);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(contentRect.adjusted(1, 1, -1, -1),
                                kModeBarCornerRadius - 1,
                                kModeBarCornerRadius - 1);
    }
}

void AddContactModeBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutSegments();
    if (m_liquidGlass) {
        m_liquidGlass->setShape(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                kModeBarCornerRadius);
    }
    scheduleLiquidGlassUpdate(0);
}

void AddContactModeBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_usesNativeBar) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int index = indexAtPosition(event->pos());
    if (m_hoveredIndex != index) {
        m_hoveredIndex = index;
        if (index >= 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void AddContactModeBar::leaveEvent(QEvent* event)
{
    m_hoveredIndex = -1;
    unsetCursor();
    QWidget::leaveEvent(event);
}

void AddContactModeBar::mousePressEvent(QMouseEvent* event)
{
    if (m_usesNativeBar) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int index = indexAtPosition(event->pos());
    if (index >= 0) {
        setSelectedIndex(index, true);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AddContactModeBar::onNativeSelectionChanged(int index)
{
    setSelectedIndex(index, true);
}

void AddContactModeBar::layoutSegments()
{
    const int margin = 6;
    const int itemHeight = qMax(0, height() - margin * 2);
    const int itemWidth = m_segments.isEmpty()
            ? 0
            : qMax(0, (width() - margin * 2) / m_segments.size());
    for (int i = 0; i < m_segments.size(); ++i) {
        m_segments[i].rect = QRect(margin + i * itemWidth,
                                   margin,
                                   i == m_segments.size() - 1
                                           ? qMax(0, width() - margin - (margin + i * itemWidth))
                                           : itemWidth,
                                   itemHeight);
    }

    const int currentIndex = selectedIndex();
    if (currentIndex >= 0 && currentIndex < m_segments.size()) {
        m_selectedRect = m_segments.at(currentIndex).rect.adjusted(4, 2, -4, -2);
        m_highlightX = m_selectedRect.x();
    }
}

void AddContactModeBar::syncPlatformBar()
{
#ifdef Q_OS_MACOS
    if (!m_usesNativeBar) {
        return;
    }
    if (property(kSystemFloatingBarsSuppressedProperty).toBool()) {
        MacPostBarBridge::clearBar(this);
        return;
    }
    MacPostBarBridge::syncBar(this, labels(), selectedIndex(), false, 1.0);
#endif
}

void AddContactModeBar::updatePanelShadow()
{
    if (m_usesNativeBar || shouldUseQtFallbackLiquidGlass()) {
        if (graphicsEffect()) {
            setGraphicsEffect(nullptr);
        }
        return;
    }

    auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (!shadow) {
        shadow = new QGraphicsDropShadowEffect(this);
        setGraphicsEffect(shadow);
    }
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::FloatingPanelShadow));
}

void AddContactModeBar::updateQtFallbackLiquidGlassState()
{
    updatePanelShadow();
    if (m_liquidGlass) {
        m_liquidGlass->setEnabled(shouldUseQtFallbackLiquidGlass() && isVisible());
    }
}

void AddContactModeBar::releaseQtFallbackLiquidGlassResources(bool updateWidget)
{
    if (m_liquidGlass) {
        m_liquidGlass->release(updateWidget);
    }
}

bool AddContactModeBar::shouldUseQtFallbackLiquidGlass() const
{
    return !m_usesNativeBar
            && ThemeManager::instance().postBarQtFallbackLiquidGlassEnabled();
}

int AddContactModeBar::indexAtPosition(const QPoint& pos) const
{
    for (int i = 0; i < m_segments.size(); ++i) {
        if (m_segments.at(i).rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

int AddContactModeBar::selectedIndex() const
{
    return m_mode == Mode::Users ? 0 : 1;
}

QStringList AddContactModeBar::labels() const
{
    QStringList result;
    result.reserve(m_segments.size());
    for (const Segment& segment : m_segments) {
        result.push_back(segment.label);
    }
    return result;
}

void AddContactModeBar::setSelectedIndex(int index, bool animate)
{
    if (index < 0 || index >= m_segments.size()) {
        return;
    }
    const Mode newMode = index == 0 ? Mode::Users : Mode::Groups;
    if (newMode == m_mode && !m_selectedRect.isEmpty()) {
        return;
    }

    const QRect targetRect = m_segments.at(index).rect.adjusted(4, 2, -4, -2);
    const int startX = m_selectedRect.isEmpty() ? targetRect.x() : m_selectedRect.x();
    m_mode = newMode;

    if (m_usesNativeBar) {
        m_selectedRect = targetRect;
        m_highlightX = targetRect.x();
#ifdef Q_OS_MACOS
        MacPostBarBridge::syncBar(this, labels(), selectedIndex(), animate, 1.0);
#endif
    } else if (animate) {
        m_selectionAnimation->stop();
        m_selectedRect = targetRect;
        m_selectedRect.moveLeft(startX);
        m_selectionAnimation->setStartValue(startX);
        m_selectionAnimation->setEndValue(targetRect.x());
        m_selectionAnimation->start();
    } else {
        m_selectionAnimation->stop();
        m_selectedRect = targetRect;
        m_highlightX = targetRect.x();
        update();
    }

    emit modeChanged(m_mode);
}

AddContactSearchWindow::AddContactSearchWindow(InitialMode mode, QWidget* parent)
    : SystemWindow(parent)
    , m_content(new QWidget(this))
    , m_titleLabel(new QLabel(titleTextForMode(mode == InitialMode::Users
                                               ? AddContactModeBar::Mode::Users
                                               : AddContactModeBar::Mode::Groups),
                              m_content))
    , m_searchInput(new IconLineEdit(m_content))
    , m_divider(new QWidget(m_content))
    , m_resultView(new OverlayScrollListView(m_content))
    , m_model(new AddContactSearchModel(this))
    , m_modeBar(new AddContactModeBar(m_content))
    , m_debounceTimer(new QTimer(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("添加联系人"));
    resize(kWindowWidth, kWindowHeight);
    setMinimumSize(420, 520);
    setCompactTrafficLightsEnabled(true);
    setDragTitleBar(m_content);

#ifdef Q_OS_WIN
    m_minimizeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Minimize, m_content);
    m_maximizeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Maximize, m_content);
    m_closeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Close, m_content);
    connect(m_minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QAbstractButton::clicked, this, [this]() {
        toggleSystemMaximized();
    });
    connect(m_closeButton, &QAbstractButton::clicked, this, &QWidget::close);
    setSystemMaximizeButton(m_maximizeButton);
#endif

    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_titleLabel->setFont(AppFonts::applicationPixelWeightedFont(16, QFont::DemiBold));

    m_searchInput->setFixedHeight(kSearchHeight);
    m_searchInput->setPlaceholderText(searchPlaceholderForMode(m_mode));

    m_resultView->setModel(m_model);
    auto* delegate = new AddContactSearchDelegate(m_resultView);
    m_resultView->setItemDelegate(delegate);
    m_resultView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultView->setUniformItemSizes(false);
    m_resultView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(delegate, &AddContactSearchDelegate::actionRequested,
            this, &AddContactSearchWindow::handleResultAction);
    connect(m_resultView->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            &AddContactSearchWindow::maybeLoadMoreResults);
    connect(m_resultView->verticalScrollBar(),
            &QScrollBar::rangeChanged,
            this,
            [this]() {
                maybeLoadMoreResults();
            });

    m_modeBar->setLiquidGlassSourceWidget(m_content);

    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(kSearchDebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, &AddContactSearchWindow::performSearch);
    connect(m_searchInput, &QLineEdit::textChanged, this, &AddContactSearchWindow::scheduleSearch);
    connect(m_modeBar, &AddContactModeBar::modeChanged, this, &AddContactSearchWindow::setMode);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });

    applyTheme();
    setMode(mode == InitialMode::Users
            ? AddContactModeBar::Mode::Users
            : AddContactModeBar::Mode::Groups);
    m_modeBar->setMode(m_mode, false);
    m_searchInput->setFocus();
}

AddContactSearchWindow::~AddContactSearchWindow() = default;

void AddContactSearchWindow::open(InitialMode mode, QWidget* anchor)
{
    auto* window = new AddContactSearchWindow(mode);
    const QSize size = window->size();
    QPoint pos;
    if (anchor && anchor->window()) {
        const QRect hostRect(anchor->window()->mapToGlobal(QPoint(0, 0)), anchor->window()->size());
        pos = hostRect.center() - QPoint(size.width() / 2, size.height() / 2);
    } else if (QScreen* screen = QGuiApplication::screenAt(QCursor::pos())) {
        pos = screen->availableGeometry().center() - QPoint(size.width() / 2, size.height() / 2);
    } else if (QScreen* screen = QGuiApplication::primaryScreen()) {
        pos = screen->availableGeometry().center() - QPoint(size.width() / 2, size.height() / 2);
    }
    window->move(pos);
    window->show();
    window->raise();
    window->activateWindow();
}

bool AddContactSearchWindow::openUserRequest(const QString& userId, QWidget* anchor)
{
    if (userId.isEmpty() || CurrentUser::instance().isCurrentUserId(userId)) {
        return false;
    }

    const User user = UserRepository::instance().requestUserDetail({userId});
    if (user.id.isEmpty() || user.isFriend) {
        return false;
    }

    const QMap<QString, QString> friendGroups = UserRepository::instance().requestFriendGroups();
    const QString defaultGroupId = friendGroups.contains(QStringLiteral("default"))
            ? QStringLiteral("default")
            : (friendGroups.isEmpty() ? QStringLiteral("default") : friendGroups.firstKey());
    const QString defaultGroupName = friendGroups.value(defaultGroupId, QStringLiteral("默认分组"));
    const ContactRequestData request = showRequestPopup(anchor,
                                                        QStringLiteral("申请添加好友"),
                                                        user.avatarPath,
                                                        user.nick,
                                                        QStringLiteral("ID %1").arg(user.id),
                                                        QStringLiteral("输入申请信息"),
                                                        user.remark,
                                                        friendGroups.isEmpty()
                                                                ? QMap<QString, QString>{{QStringLiteral("default"), QStringLiteral("默认分组")}}
                                                                : friendGroups,
                                                        defaultGroupId,
                                                        defaultGroupName,
                                                        QStringLiteral("备注"),
                                                        QStringLiteral("好友分组"),
                                                        true,
                                                        QStringLiteral("发送申请"));
    return applyFriendRequest(user, request);
}

bool AddContactSearchWindow::openGroupRequest(const QString& groupId, QWidget* anchor)
{
    if (groupId.isEmpty()) {
        return false;
    }

    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    if (group.groupId.isEmpty() || currentUserHasJoinedGroup(group)) {
        return false;
    }

    QMap<QString, QString> categories = GroupRepository::instance().requestGroupCategories();
    QMap<QString, QString> orderedCategories;
    for (const QString& categoryId : editableCategoryOrder()) {
        if (categories.contains(categoryId)) {
            orderedCategories.insert(categoryId, categories.value(categoryId));
        }
    }
    if (orderedCategories.isEmpty()) {
        orderedCategories.insert(QStringLiteral("gg_joined"), QStringLiteral("我加入的群聊"));
    }

    const QString defaultCategoryId = orderedCategories.contains(QStringLiteral("gg_joined"))
            ? QStringLiteral("gg_joined")
            : orderedCategories.firstKey();
    const QString defaultCategoryName = orderedCategories.value(defaultCategoryId,
                                                               QStringLiteral("我加入的群聊"));
    const ContactRequestData request = showRequestPopup(anchor,
                                                        QStringLiteral("申请加入群聊"),
                                                        group.groupAvatarPath,
                                                        group.groupName,
                                                        QStringLiteral("ID %1").arg(group.groupId),
                                                        QStringLiteral("输入入群申请信息"),
                                                        group.remark,
                                                        orderedCategories,
                                                        defaultCategoryId,
                                                        defaultCategoryName,
                                                        QStringLiteral("备注"),
                                                        QStringLiteral("群分组"),
                                                        true,
                                                        QStringLiteral("发送申请"));
    return applyGroupRequest(group, request);
}

bool AddContactSearchWindow::openFriendApproval(const QString& userId,
                                                QWidget* anchor,
                                                QString* remark,
                                                QString* groupId,
                                                QString* groupName)
{
    const User user = UserRepository::instance().requestUserDetail({userId});
    if (user.id.isEmpty()) {
        return false;
    }

    const QMap<QString, QString> friendGroups = UserRepository::instance().requestFriendGroups();
    const QMap<QString, QString> groups = friendGroups.isEmpty()
            ? QMap<QString, QString>{{QStringLiteral("default"), QStringLiteral("默认分组")}}
            : friendGroups;
    const QString defaultGroupId = groups.contains(user.friendGroupId)
            ? user.friendGroupId
            : (groups.contains(QStringLiteral("default")) ? QStringLiteral("default") : groups.firstKey());
    const QString defaultGroupName = groups.value(defaultGroupId, QStringLiteral("默认分组"));

    const ContactRequestData request = showRequestPopup(anchor,
                                                        QStringLiteral("同意好友申请"),
                                                        user.avatarPath,
                                                        user.nick,
                                                        QStringLiteral("ID %1").arg(user.id),
                                                        QString(),
                                                        user.remark,
                                                        groups,
                                                        defaultGroupId,
                                                        defaultGroupName,
                                                        QStringLiteral("备注"),
                                                        QStringLiteral("好友分组"),
                                                        false,
                                                        QStringLiteral("同意"));
    if (!request.accepted) {
        return false;
    }
    if (remark) {
        *remark = request.remark;
    }
    if (groupId) {
        *groupId = request.groupId;
    }
    if (groupName) {
        *groupName = request.groupName;
    }
    return true;
}

bool AddContactSearchWindow::openGroupApproval(const QString& groupId,
                                               QWidget* anchor,
                                               QString* remark,
                                               QString* categoryId,
                                               QString* categoryName)
{
    const Group group = GroupRepository::instance().requestGroupDetail({groupId});
    if (group.groupId.isEmpty()) {
        return false;
    }

    const QMap<QString, QString> categories = GroupRepository::instance().requestGroupCategories();
    QMap<QString, QString> orderedCategories;
    for (const QString& id : editableCategoryOrder()) {
        if (categories.contains(id)) {
            orderedCategories.insert(id, categories.value(id));
        }
    }
    if (orderedCategories.isEmpty()) {
        orderedCategories.insert(QStringLiteral("gg_joined"), QStringLiteral("我加入的群聊"));
    }

    const QString currentCategoryId = orderedCategories.contains(group.listGroupId)
            ? group.listGroupId
            : (orderedCategories.contains(QStringLiteral("gg_joined")) ? QStringLiteral("gg_joined") : orderedCategories.firstKey());
    const QString currentCategoryName = orderedCategories.value(currentCategoryId,
                                                               QStringLiteral("我加入的群聊"));

    const ContactRequestData request = showRequestPopup(anchor,
                                                        QStringLiteral("同意入群申请"),
                                                        group.groupAvatarPath,
                                                        group.groupName,
                                                        QStringLiteral("ID %1").arg(group.groupId),
                                                        QString(),
                                                        group.remark,
                                                        orderedCategories,
                                                        currentCategoryId,
                                                        currentCategoryName,
                                                        QStringLiteral("备注"),
                                                        QStringLiteral("群分组"),
                                                        false,
                                                        QStringLiteral("同意"));
    if (!request.accepted) {
        return false;
    }
    if (remark) {
        *remark = request.remark;
    }
    if (categoryId) {
        *categoryId = request.groupId;
    }
    if (categoryName) {
        *categoryName = request.groupName;
    }
    return true;
}

bool AddContactSearchWindow::event(QEvent* event)
{
    const bool handled = SystemWindow::event(event);
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ZOrderChange:
        m_modeBar->scheduleLiquidGlassUpdate(QtFallbackLiquidGlassController::refreshDelayMs());
        break;
    default:
        break;
    }
    return handled;
}

void AddContactSearchWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    SystemWindow::keyPressEvent(event);
}

void AddContactSearchWindow::resizeEvent(QResizeEvent* event)
{
    SystemWindow::resizeEvent(event);
    updateLayout();
}

void AddContactSearchWindow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::instance().color(ThemeColor::PanelBackground));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);
}

void AddContactSearchWindow::applyTheme()
{
    QPalette palette = m_content->palette();
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::PanelBackground));
    m_content->setPalette(palette);
    m_content->setAutoFillBackground(false);

    QPalette titlePalette = m_titleLabel->palette();
    titlePalette.setColor(QPalette::WindowText,
                          ThemeManager::instance().color(ThemeColor::PrimaryText));
    titlePalette.setColor(QPalette::Text,
                          ThemeManager::instance().color(ThemeColor::PrimaryText));
    m_titleLabel->setPalette(titlePalette);

    m_divider->setAutoFillBackground(true);
    QPalette dividerPalette = m_divider->palette();
    dividerPalette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::Divider));
    m_divider->setPalette(dividerPalette);

    update();
    m_resultView->viewport()->update();
    m_modeBar->update();
}

void AddContactSearchWindow::updateLayout()
{
    m_content->setGeometry(rect());

    const int contentWidth = qMax(0, width() - kHorizontalMargin * 2);
    m_titleLabel->setGeometry(0, kTitleTopMargin, width(), kTitleHeight);

    const int searchY = kTitleTopMargin + kTitleHeight + kTitleSearchGap;
    m_searchInput->setGeometry(kHorizontalMargin,
                               searchY,
                               contentWidth,
                               kSearchHeight);

    const int dividerY = m_searchInput->geometry().bottom() + kDividerTopGap;
    m_divider->setGeometry(kHorizontalMargin, dividerY, contentWidth, 1);

    const int modeBarX = (width() - kModeBarWidth) / 2;
    const int modeBarY = height() - kModeBarBottomMargin - kModeBarHeight;
    m_modeBar->setGeometry(modeBarX, modeBarY, kModeBarWidth, kModeBarHeight);

    const int resultY = dividerY + 1;
    m_resultView->setGeometry(0, resultY, width(), qMax(0, height() - resultY));
    m_modeBar->raise();
#ifdef Q_OS_WIN
    if (m_closeButton && m_maximizeButton && m_minimizeButton) {
        m_closeButton->setGeometry(width() - m_closeButton->width(),
                                   0,
                                   m_closeButton->width(),
                                   m_closeButton->height());
        m_maximizeButton->setGeometry(m_closeButton->x() - m_maximizeButton->width(),
                                      0,
                                      m_maximizeButton->width(),
                                      m_maximizeButton->height());
        m_minimizeButton->setGeometry(m_maximizeButton->x() - m_minimizeButton->width(),
                                      0,
                                      m_minimizeButton->width(),
                                      m_minimizeButton->height());
        m_minimizeButton->raise();
        m_maximizeButton->raise();
        m_closeButton->raise();
    }
#endif
}

void AddContactSearchWindow::setMode(AddContactModeBar::Mode mode)
{
    m_titleLabel->setText(titleTextForMode(mode));
    m_searchInput->setPlaceholderText(searchPlaceholderForMode(mode));

    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    resetSearchState();
    scheduleSearch();
}

void AddContactSearchWindow::scheduleSearch()
{
    if (m_searchInput->text().trimmed().isEmpty()) {
        m_debounceTimer->stop();
        resetSearchState();
        return;
    }
    m_debounceTimer->start();
}

void AddContactSearchWindow::performSearch()
{
    const QString keyword = m_searchInput->text().trimmed();
    if (keyword.isEmpty()) {
        resetSearchState();
        return;
    }

    resetSearchState();
    m_searchResults = m_mode == AddContactModeBar::Mode::Users
            ? searchUsers(keyword)
            : searchGroups(keyword);
    loadMoreResults();
}

QVector<AddContactSearchItem> AddContactSearchWindow::searchUsers(const QString& keyword) const
{
    QVector<AddContactSearchItem> items;
    const QVector<User> users = UserRepository::instance().requestUserSearch(keyword, -1);
    items.reserve(qMin(users.size(), kSearchResultLimit));
    for (const User& user : users) {
        if (user.isFriend || CurrentUser::instance().isCurrentUserId(user.id)) {
            continue;
        }
        AddContactSearchItem item;
        item.type = AddContactSearchItem::Type::User;
        item.user = user;
        items.push_back(item);
        if (items.size() >= kSearchResultLimit) {
            break;
        }
    }
    return items;
}

QVector<AddContactSearchItem> AddContactSearchWindow::searchGroups(const QString& keyword) const
{
    QVector<AddContactSearchItem> items;
    const QVector<Group> groups = GroupRepository::instance().requestGroupSearch(keyword, -1);
    items.reserve(qMin(groups.size(), kSearchResultLimit));
    int visibleGroupIndex = 0;
    for (const Group& group : groups) {
        if (currentUserHasJoinedGroup(group)) {
            continue;
        }
        AddContactSearchItem item;
        item.type = AddContactSearchItem::Type::Group;
        item.group = group;
        if (visibleGroupIndex % 3 == 1) {
            item.group.introduction.clear();
        }
        items.push_back(item);
        ++visibleGroupIndex;
        if (items.size() >= kSearchResultLimit) {
            break;
        }
    }
    return items;
}

void AddContactSearchWindow::handleResultAction(const QModelIndex& index)
{
    if (!index.isValid() || !index.data(AddContactSearchModel::ActionEnabledRole).toBool()) {
        return;
    }

    const AddContactSearchItem item = m_model->itemAt(index.row());
    bool changed = false;
    if (item.type == AddContactSearchItem::Type::User) {
        changed = openUserRequest(item.user.id, this);
    } else {
        changed = openGroupRequest(item.group.groupId, this);
    }

    if (changed) {
        performSearch();
    }
}

void AddContactSearchWindow::loadMoreResults()
{
    if (m_isLoadingMore || m_loadedResultCount >= m_searchResults.size()) {
        return;
    }

    m_isLoadingMore = true;
    const int remaining = m_searchResults.size() - m_loadedResultCount;
    const int count = qMin(kSearchPageSize, remaining);
    const QVector<AddContactSearchItem> nextItems = m_searchResults.mid(m_loadedResultCount, count);
    m_loadedResultCount += count;
    m_model->appendItems(nextItems);
    m_isLoadingMore = false;
}

void AddContactSearchWindow::maybeLoadMoreResults()
{
    if (m_searchResults.isEmpty() || m_loadedResultCount >= m_searchResults.size()) {
        return;
    }

    QScrollBar* scrollBar = m_resultView->verticalScrollBar();
    if (!scrollBar) {
        return;
    }

    if (scrollBar->maximum() <= scrollBar->minimum()) {
        return;
    }

    if (scrollBar->maximum() - scrollBar->value() <= kLoadMoreThresholdPx) {
        loadMoreResults();
    }
}

void AddContactSearchWindow::resetSearchState()
{
    m_searchResults.clear();
    m_loadedResultCount = 0;
    m_isLoadingMore = false;
    m_model->clear();
    if (QScrollBar* scrollBar = m_resultView->verticalScrollBar()) {
        scrollBar->setValue(scrollBar->minimum());
    }
}
