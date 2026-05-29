#include "GroupNotificationDelegate.h"
#include "shared/services/AppFonts.h"

#include <QApplication>
#include <QPainter>

#include "app/state/CurrentUser.h"
#include "features/friend/model/GroupNotificationListModel.h"
#include "features/friend/ui/FriendSessionController.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/NotificationDelegateRenderer.h"

namespace {

constexpr int kLineSpacing = 4;
constexpr int kFontSize = 12;
constexpr int kInlineTimeGap = 4;
constexpr int kJoinUserMaxWidth = 150;
constexpr int kJoinGroupMaxWidth = 180;
constexpr int kJoinActionWidthOffset = 12;
constexpr int kMeasuredTextWidthOffset = 8;
constexpr int kInlineNameMaxWidth = 150;
constexpr int kStatusSegmentGap = 0;
constexpr int kHandledStatusMaxWidth = 220;
constexpr int kHandledStatusTextGap = 8;

struct TextSegment {
    QString text;
    QColor color;
};

NotificationDelegateRenderer::CardMetrics cardMetrics()
{
    return {};
}

QFont textFont()
{
    return AppFonts::applicationPixelSizedFont(kFontSize);
}

QFont emphFont()
{
    return AppFonts::applicationPixelWeightedFont(kFontSize, QFont::Medium);
}

QFont buttonFont()
{
    return AppFonts::applicationPixelWeightedFont(kFontSize, QFont::Medium);
}

QFontMetrics textFm()
{
    return AppFonts::applicationPixelSizedMetrics(kFontSize);
}

QString userNickname(const FriendSessionController* controller, const QString& userId)
{
    return controller ? controller->userNickname(userId) : userId;
}

QString groupDisplayName(const FriendSessionController* controller, const QString& groupId)
{
    return controller ? controller->groupDisplayName(groupId) : groupId;
}

QString groupNicknameFor(const FriendSessionController* controller,
                         const QString& groupId,
                         const QString& userId)
{
    return controller ? controller->groupNicknameFor(groupId, userId) : userNickname(controller, userId);
}

QString managerRoleTextFor(const FriendSessionController* controller,
                           const QString& groupId,
                           const QString& userId)
{
    return controller ? controller->groupManagerRoleText(groupId, userId) : QStringLiteral("管理员");
}

QRect cardRect(const QStyleOptionViewItem& option)
{
    return NotificationDelegateRenderer::cardRect(option.rect, cardMetrics());
}

QRect avatarRect(const QRect& card)
{
    return NotificationDelegateRenderer::avatarRect(card, cardMetrics());
}

QRect contentBounds(const QRect& card)
{
    return NotificationDelegateRenderer::contentBounds(card, cardMetrics());
}

int lineHeight()
{
    return textFm().height();
}

int lineHeightAt(int line)
{
    return line == 1
            ? qMax(lineHeight(), cardMetrics().buttonHeight)
            : lineHeight();
}

QRect lineRect(const QRect& content, int line, int lineCount)
{
    int totalHeight = 0;
    for (int i = 0; i < lineCount; ++i) {
        totalHeight += lineHeightAt(i);
    }
    totalHeight += (lineCount - 1) * kLineSpacing;

    const int top = content.top() + qMax(0, (content.height() - totalHeight) / 2);
    int y = top;
    for (int i = 0; i < line; ++i) {
        y += lineHeightAt(i) + kLineSpacing;
    }
    return QRect(content.left(),
                 y,
                 content.width(),
                 lineHeightAt(line));
}

QRect acceptBtnRect(const QRect& card, const QRect& content)
{
    Q_UNUSED(content)
    const int y = card.top() + (card.height() - cardMetrics().buttonHeight) / 2;
    return NotificationDelegateRenderer::acceptButtonRect(card, y, cardMetrics());
}

QRect rejectBtnRect(const QRect& card, const QRect& content)
{
    Q_UNUSED(content)
    const int y = card.top() + (card.height() - cardMetrics().buttonHeight) / 2;
    return NotificationDelegateRenderer::rejectButtonRect(card, y, cardMetrics());
}

QRect actionRect(const QRect& card, const QRect& content)
{
    const QRect accept = acceptBtnRect(card, content);
    const QRect reject = rejectBtnRect(card, content);
    return NotificationDelegateRenderer::actionRect(accept, reject);
}

int textRightBeforeAction(const QRect& line, const QRect& card, const QRect& content)
{
    return qMin(line.right(), actionRect(card, content).left() - cardMetrics().buttonGap);
}

bool isHandledByCurrentUser(const QString& operatorId)
{
    return !operatorId.isEmpty() && operatorId == CurrentUser::instance().getUserId();
}

int naturalTextWidth(const QString& text)
{
    return text.isEmpty() ? 0 : textFm().horizontalAdvance(text);
}

int drawElidedText(QPainter* painter,
                   int x,
                   const QRect& line,
                   int maxWidth,
                   const QString& text,
                   const QColor& color)
{
    if (maxWidth <= 0 || text.isEmpty()) {
        return 0;
    }
    const int naturalWidth = naturalTextWidth(text);
    const bool fitsNaturally = naturalWidth + kMeasuredTextWidthOffset <= maxWidth;
    const QString displayText = fitsNaturally
            ? text
            : textFm().elidedText(text, Qt::ElideRight, maxWidth);
    const int width = fitsNaturally
            ? naturalWidth
            : qMin(maxWidth, textFm().horizontalAdvance(displayText));
    painter->setPen(color);
    painter->drawText(QRect(x,
                            line.top(),
                            qMin(maxWidth, width + kMeasuredTextWidthOffset),
                            line.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      displayText);
    return width;
}

const QString& joinActionText()
{
    static const QString text = QStringLiteral(" 申请加入 ");
    return text;
}

int joinActionWidth()
{
    static const int width = naturalTextWidth(joinActionText()) + kJoinActionWidthOffset;
    return width;
}

int cappedTextWidth(const QString& text, int maxWidth, int available)
{
    const int measuredWidth = naturalTextWidth(text) + kMeasuredTextWidthOffset;
    return qMin(measuredWidth, qMin(maxWidth, qMax(0, available)));
}

int drawInlineTime(QPainter* painter,
                   int x,
                   const QRect& line,
                   const QString& timeText,
                   const QColor& color)
{
    if (timeText.isEmpty()) {
        return 0;
    }
    painter->setFont(textFont());
    const int available = qMax(0, line.right() - x + 1);
    if (available <= 0) {
        return 0;
    }
    const QString elided = textFm().elidedText(timeText, Qt::ElideRight, available);
    const int timeWidth = qMin(available, textFm().horizontalAdvance(elided));
    painter->setPen(color);
    painter->drawText(QRect(x,
                            line.top(),
                            timeWidth,
                            line.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      elided);
    return timeWidth;
}

void drawJoinTitle(QPainter* painter,
                   const QRect& rect,
                   const QString& userName,
                   const QString& groupName,
                   const QString& timeText,
                   const QColor& accent,
                   const QColor& secondary,
                   const QColor& tertiary)
{
    painter->setFont(textFont());
    const int actionWidth = joinActionWidth();
    int x = rect.left();
    int remaining = rect.width();

    const int userWidth = cappedTextWidth(userName,
                                          kJoinUserMaxWidth,
                                          qMax(0, remaining - actionWidth));
    const int drawnUserWidth = drawElidedText(painter, x, rect, userWidth, userName, accent);
    x += drawnUserWidth;
    remaining -= drawnUserWidth;

    const int drawnActionWidth = drawElidedText(painter,
                                                x,
                                                rect,
                                                qMin(actionWidth, remaining),
                                                joinActionText(),
                                                secondary);
    x += drawnActionWidth;
    remaining -= drawnActionWidth;

    const int groupWidth = cappedTextWidth(groupName, kJoinGroupMaxWidth, remaining);
    const int drawnGroupWidth = drawElidedText(painter, x, rect, groupWidth, groupName, accent);
    x += drawnGroupWidth;
    remaining -= drawnGroupWidth;

    if (remaining > kInlineTimeGap) {
        drawInlineTime(painter, x + kInlineTimeGap, rect, timeText, tertiary);
    }
}

void drawPrefixNameLine(QPainter* painter,
                        const QRect& rect,
                        const QString& prefix,
                        const QString& name,
                        const QString& timeText,
                        const QColor& accent,
                        const QColor& secondary,
                        const QColor& tertiary)
{
    painter->setFont(textFont());
    const int prefixWidth = naturalTextWidth(prefix);
    int x = rect.left();
    int remaining = rect.width();

    const int drawnPrefixWidth = drawElidedText(painter,
                                                x,
                                                rect,
                                                qMin(prefixWidth, remaining),
                                                prefix,
                                                secondary);
    x += drawnPrefixWidth;
    remaining -= drawnPrefixWidth;

    const int nameWidth = cappedTextWidth(name, kInlineNameMaxWidth, remaining);
    const int drawnNameWidth = drawElidedText(painter, x, rect, nameWidth, name, accent);
    x += drawnNameWidth;
    remaining -= drawnNameWidth;

    if (remaining > kInlineTimeGap) {
        drawInlineTime(painter, x + kInlineTimeGap, rect, timeText, tertiary);
    }
}

void drawActionGroupLine(QPainter* painter,
                         const QRect& rect,
                         const QString& prefix,
                         const QString& groupName,
                         const QString& suffix,
                         const QColor& accent,
                         const QColor& secondary)
{
    painter->setFont(textFont());
    const int prefixWidth = textFm().horizontalAdvance(prefix);
    const int suffixWidth = textFm().horizontalAdvance(suffix);
    const int groupWidth = qMax(0, rect.width() - prefixWidth - suffixWidth);
    int x = rect.left();
    x += drawElidedText(painter, x, rect, prefixWidth, prefix, secondary);
    x += drawElidedText(painter, x, rect, groupWidth, groupName, accent);
    drawElidedText(painter, x, rect, suffixWidth, suffix, secondary);
}

void drawTextWithTrailingTime(QPainter* painter,
                              const QRect& rect,
                              const QVector<TextSegment>& segments,
                              const QString& timeText,
                              const QColor& timeColor)
{
    painter->setFont(textFont());
    int x = rect.left();
    int remaining = rect.width();
    for (const TextSegment& segment : segments) {
        const int width = cappedTextWidth(segment.text, kInlineNameMaxWidth, remaining);
        const int drawnWidth = drawElidedText(painter, x, rect, width, segment.text, segment.color);
        x += drawnWidth;
        remaining -= drawnWidth;
    }

    if (remaining > kInlineTimeGap) {
        drawInlineTime(painter, x + kInlineTimeGap, rect, timeText, timeColor);
    }
}

QRect handledStatusRect(const QRect& card,
                        const QRect& content,
                        const QString& groupId,
                        const QString& operatorId,
                        GroupNotificationStatus status,
                        const FriendSessionController* controller)
{
    const QRect action = actionRect(card, content);
    const QString statusText = status == GroupNotificationStatus::Accepted
            ? QStringLiteral("已同意")
            : QStringLiteral("已拒绝");
    const int statusWidth = textFm().horizontalAdvance(statusText);

    if (isHandledByCurrentUser(operatorId) || operatorId.isEmpty()) {
        return action;
    }

    const QString roleText = managerRoleTextFor(controller, groupId, operatorId);
    const QString name = groupNicknameFor(controller, groupId, operatorId);
    const int naturalWidth = textFm().horizontalAdvance(roleText)
            + textFm().horizontalAdvance(name)
            + statusWidth;
    const int centerX = action.center().x();
    const int maxByRightEdge = qMax(0, (card.right() - centerX + 1) * 2);
    const int maxByLeftEdge = qMax(0, (centerX - content.left() + 1) * 2);
    const int maxWidth = qMax(action.width(),
                              qMin(kHandledStatusMaxWidth,
                                   qMin(maxByRightEdge, maxByLeftEdge)));
    const int width = qBound(action.width(), naturalWidth, maxWidth);
    return QRect(centerX - width / 2, action.top(), width, action.height());
}

int textRightBeforeHandledStatus(const QRect& line,
                                 const QRect& card,
                                 const QRect& content,
                                 const QString& groupId,
                                 const QString& operatorId,
                                 GroupNotificationStatus status,
                                 const FriendSessionController* controller)
{
    return qMin(line.right(),
                handledStatusRect(card, content, groupId, operatorId, status, controller).left()
                    - kHandledStatusTextGap);
}

void drawHandledStatus(QPainter* painter,
                       const QRect& rect,
                       const QString& groupId,
                       const QString& operatorId,
                       GroupNotificationStatus status,
                       const QColor& accent,
                       const QColor& secondary,
                       const FriendSessionController* controller)
{
    const QString statusText = status == GroupNotificationStatus::Accepted
            ? QStringLiteral("已同意")
            : QStringLiteral("已拒绝");

    painter->setFont(buttonFont());
    if (!isHandledByCurrentUser(operatorId) && !operatorId.isEmpty()) {
        const QString roleText = managerRoleTextFor(controller, groupId, operatorId);
        const QString name = groupNicknameFor(controller, groupId, operatorId);
        const int roleWidth = textFm().horizontalAdvance(roleText);
        const int statusWidth = textFm().horizontalAdvance(statusText);
        const int maxNameWidth = qMax(0, rect.width() - roleWidth - statusWidth - kStatusSegmentGap);
        const QString displayName = textFm().elidedText(name, Qt::ElideRight, maxNameWidth);
        const int nameWidth = qMin(maxNameWidth,
                                   textFm().horizontalAdvance(displayName));
        const int totalWidth = roleWidth + nameWidth + statusWidth + kStatusSegmentGap;
        int x = rect.left() + (rect.width() - totalWidth) / 2;

        painter->setPen(secondary);
        painter->drawText(QRect(x, rect.top(), roleWidth, rect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          roleText);
        x += roleWidth;

        painter->setPen(accent);
        painter->drawText(QRect(x, rect.top(), nameWidth, rect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          displayName);
        x += nameWidth + kStatusSegmentGap;

        painter->setPen(secondary);
        painter->drawText(QRect(x, rect.top(), statusWidth, rect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          statusText);
        return;
    }

    const int statusWidth = textFm().horizontalAdvance(statusText);
    const QRect statusRect(rect.left() + (rect.width() - statusWidth) / 2,
                           rect.top(),
                           statusWidth,
                           rect.height());
    painter->setPen(secondary);
    painter->drawText(statusRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      statusText);
}

} // namespace

GroupNotificationDelegate::GroupNotificationDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void GroupNotificationDelegate::setController(FriendSessionController* controller)
{
    m_controller = controller;
}

int GroupNotificationDelegate::buttonAt(const QStyleOptionViewItem& option,
                                        const QModelIndex& index,
                                        const QPoint& point) const
{
    if (index.data(GroupNotificationListModel::BottomSpaceRole).toBool()) {
        return -1;
    }

    const auto type = static_cast<GroupNotificationType>(
        index.data(GroupNotificationListModel::TypeRole).toInt());
    const auto status = static_cast<GroupNotificationStatus>(
        index.data(GroupNotificationListModel::StatusRole).toInt());
    if (type != GroupNotificationType::JoinRequest || status != GroupNotificationStatus::Pending) {
        return -1;
    }

    const QRect card = cardRect(option);
    const QRect content = contentBounds(card);
    if (acceptBtnRect(card, content).contains(point)) return 0;
    if (rejectBtnRect(card, content).contains(point)) return 1;
    return -1;
}

void GroupNotificationDelegate::paint(QPainter* painter,
                                      const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);

    painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PageBackground));
    if (index.data(GroupNotificationListModel::BottomSpaceRole).toBool()) {
        painter->restore();
        return;
    }

    const QRect card = cardRect(option);
    const QRect content = contentBounds(card);
    const QString groupId = index.data(GroupNotificationListModel::GroupIdRole).toString();
    const QString actorId = index.data(GroupNotificationListModel::ActorUserIdRole).toString();
    const QString operatorId = index.data(GroupNotificationListModel::OperatorUserIdRole).toString();
    const QString message = index.data(GroupNotificationListModel::MessageRole).toString();
    const QString date = NotificationDelegateRenderer::formatTime(
        index.data(GroupNotificationListModel::CreatedAtRole).toDateTime());
    const auto type = static_cast<GroupNotificationType>(
        index.data(GroupNotificationListModel::TypeRole).toInt());
    const auto status = static_cast<GroupNotificationStatus>(
        index.data(GroupNotificationListModel::StatusRole).toInt());
    const int hovered = index.data(GroupNotificationListModel::HoveredButtonRole).toInt();
    const bool pendingJoinRequest = type == GroupNotificationType::JoinRequest
            && status == GroupNotificationStatus::Pending;
    const bool pendingActionsVisible = pendingJoinRequest
            && hovered != GroupNotificationListModel::kNoHoveredButton;
    NotificationDelegateRenderer::drawCard(painter, card, cardMetrics());

    const QRect avatar = avatarRect(card);
    const QString avatarPath = type == GroupNotificationType::JoinRequest
            ? (m_controller ? m_controller->userAvatarPath(actorId) : QString())
            : (m_controller ? m_controller->groupAvatarPath(groupId) : QString());
    NotificationDelegateRenderer::drawAvatar(painter, avatar, avatarPath, cardMetrics().avatarSize);

    const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
    const QColor secondary = ThemeManager::instance().color(ThemeColor::SecondaryText);
    const QColor tertiary = ThemeManager::instance().color(ThemeColor::TertiaryText);

    const int lineCount = 2;

    QRect l1 = lineRect(content, 0, lineCount);
    if (type == GroupNotificationType::JoinRequest) {
        int textRight = l1.right();
        if (status == GroupNotificationStatus::Pending) {
            textRight = textRightBeforeAction(l1, card, content);
        } else {
            textRight = textRightBeforeHandledStatus(l1,
                                                     card,
                                                     content,
                                                     groupId,
                                                     operatorId,
                                                     status,
                                                     m_controller);
        }
        l1.setRight(qMax(l1.left(), textRight));
    }
    if (type == GroupNotificationType::JoinRequest) {
        drawJoinTitle(painter,
                      l1,
                      userNickname(m_controller, actorId),
                      groupDisplayName(m_controller, groupId),
                      date,
                      accent,
                      secondary,
                      tertiary);
    } else if (type == GroupNotificationType::MemberExited) {
        drawPrefixNameLine(painter,
                           l1,
                           {},
                           groupNicknameFor(m_controller, groupId, actorId),
                           date,
                           accent,
                           secondary,
                           tertiary);
    } else {
        drawTextWithTrailingTime(painter,
                                 l1,
                                 {{groupNicknameFor(m_controller, groupId, operatorId), accent}},
                                 date,
                                 tertiary);
    }

    if (type == GroupNotificationType::JoinRequest) {
        QRect l2 = lineRect(content, 1, lineCount);
        int textRight = l2.right();
        if (status == GroupNotificationStatus::Pending) {
            textRight = textRightBeforeAction(l2, card, content);
        } else {
            textRight = textRightBeforeHandledStatus(l2,
                                                    card,
                                                    content,
                                                    groupId,
                                                    operatorId,
                                                    status,
                                                    m_controller);
        }
        l2.setRight(qMax(l2.left(), textRight));
        painter->setFont(textFont());
        painter->setPen(secondary);
        painter->drawText(l2, Qt::AlignLeft | Qt::AlignVCenter,
                          textFm().elidedText(QStringLiteral("留言：%1").arg(message),
                                               Qt::ElideRight,
                                               l2.width()));

        if (status == GroupNotificationStatus::Pending) {
            if (pendingActionsVisible) {
                const QRect accR = acceptBtnRect(card, content);
                const QRect rejR = rejectBtnRect(card, content);
                NotificationDelegateRenderer::drawActionButtons(
                    painter, accR, rejR, hovered, buttonFont(), cardMetrics());
            } else {
                NotificationDelegateRenderer::drawPendingDots(
                    painter, actionRect(card, content), tertiary);
            }
        } else {
            drawHandledStatus(painter,
                              handledStatusRect(card, content, groupId, operatorId, status, m_controller),
                              groupId,
                              operatorId,
                              status,
                              accent,
                              secondary,
                              m_controller);
        }
    } else if (type == GroupNotificationType::MemberExited) {
        QRect l2 = lineRect(content, 1, lineCount);
        drawActionGroupLine(painter,
                            l2,
                            QStringLiteral("退出 "),
                            groupDisplayName(m_controller, groupId),
                            {},
                            accent,
                            secondary);
    } else if (type == GroupNotificationType::AdminAssigned) {
        QRect l2 = lineRect(content, 1, lineCount);
        drawActionGroupLine(painter,
                            l2,
                            QStringLiteral("将你设为 "),
                            groupDisplayName(m_controller, groupId),
                            QStringLiteral(" 管理员"),
                            accent,
                            secondary);
    } else {
        QRect l2 = lineRect(content, 1, lineCount);
        drawActionGroupLine(painter,
                            l2,
                            QStringLiteral("将 "),
                            groupDisplayName(m_controller, groupId),
                            QStringLiteral(" 转让给你"),
                            accent,
                            secondary);
    }

    painter->restore();
}

QSize GroupNotificationDelegate::sizeHint(const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const
{
    if (index.data(GroupNotificationListModel::BottomSpaceRole).toBool()) {
        return QSize(option.rect.width() > 0 ? option.rect.width() : 300,
                     GroupNotificationListModel::kBottomSpaceHeight);
    }
    return QSize(option.rect.width() > 0 ? option.rect.width() : 300, kItemHeight);
}
