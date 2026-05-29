#include "FriendNotificationDelegate.h"
#include "shared/services/AppFonts.h"

#include <QApplication>
#include <QPainter>

#include "features/friend/model/FriendNotificationListModel.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/NotificationDelegateRenderer.h"

namespace {

constexpr int kLineSpacing = 1;
constexpr int kUnifiedFontSize = 12;

NotificationDelegateRenderer::CardMetrics cardMetrics()
{
    return {};
}

QFont nickFont()
{
    return AppFonts::applicationPixelWeightedFont(kUnifiedFontSize, QFont::Medium);
}

QFont textFont()
{
    return AppFonts::applicationPixelWeightedFont(kUnifiedFontSize, QFont::Normal);
}

QFont buttonFont()
{
    return AppFonts::applicationPixelWeightedFont(12, QFont::Medium);
}

QFontMetrics nickFm() { return AppFonts::applicationPixelWeightedMetrics(kUnifiedFontSize, QFont::Medium); }
QFontMetrics textFm() { return AppFonts::applicationPixelWeightedMetrics(kUnifiedFontSize, QFont::Normal); }

} // namespace

FriendNotificationDelegate::FriendNotificationDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

// --- geometry helpers ---

static QRect cardRect(const QStyleOptionViewItem& option)
{
    return NotificationDelegateRenderer::cardRect(option.rect, cardMetrics());
}

static QRect avatarRect(const QRect& card)
{
    return NotificationDelegateRenderer::avatarRect(card, cardMetrics());
}

static QRect contentBounds(const QRect& card)
{
    return NotificationDelegateRenderer::contentBounds(card, cardMetrics());
}

static int textLineHeight() { return textFm().height(); }
static int line1Height() { return nickFm().height(); }
static int line2Height()
{
    return qMax(textLineHeight(), cardMetrics().buttonHeight);
}

static int layoutTop(const QRect& content)
{
    const int totalHeight = line1Height() + kLineSpacing + line2Height()
                            + kLineSpacing + textLineHeight();
    return content.top() + qMax(0, (content.height() - totalHeight) / 2);
}

static QRect line1Rect(const QRect& content)
{
    return QRect(content.left(), layoutTop(content), content.width(), line1Height());
}

static QRect line2Rect(const QRect& content)
{
    const int y = line1Rect(content).bottom() + 1 + kLineSpacing;
    return QRect(content.left(), y, content.width(), line2Height());
}

static QRect line3Rect(const QRect& content)
{
    const int y = line2Rect(content).bottom() + 1 + kLineSpacing;
    return QRect(content.left(), y, content.width(), textLineHeight());
}

static QRect acceptBtnRect(const QRect& card, const QRect& content)
{
    const QRect l2 = line2Rect(content);
    const int y = l2.top() + (l2.height() - cardMetrics().buttonHeight) / 2;
    return NotificationDelegateRenderer::acceptButtonRect(card, y, cardMetrics());
}

static QRect rejectBtnRect(const QRect& card, const QRect& content)
{
    const QRect l2 = line2Rect(content);
    const int y = l2.top() + (l2.height() - cardMetrics().buttonHeight) / 2;
    return NotificationDelegateRenderer::rejectButtonRect(card, y, cardMetrics());
}

static QRect actionRect(const QRect& card, const QRect& content)
{
    const QRect accept = acceptBtnRect(card, content);
    const QRect reject = rejectBtnRect(card, content);
    return NotificationDelegateRenderer::actionRect(accept, reject);
}

static int textRightBeforeAction(const QRect& line, const QRect& card, const QRect& content)
{
    return qMin(line.right(), actionRect(card, content).left() - cardMetrics().buttonGap);
}

int FriendNotificationDelegate::buttonAt(const QStyleOptionViewItem& option,
                                          const QModelIndex& index,
                                          const QPoint& point) const
{
    if (index.data(FriendNotificationListModel::BottomSpaceRole).toBool()) {
        return -1;
    }

    const QRect card = cardRect(option);
    const QRect c = contentBounds(card);
    const auto st = static_cast<NotificationStatus>(
        index.data(FriendNotificationListModel::StatusRole).toInt());
    if (st != NotificationStatus::Pending) return -1;
    if (acceptBtnRect(card, c).contains(point)) return 0;
    if (rejectBtnRect(card, c).contains(point)) return 1;
    return -1;
}

// ==================== paint ====================

void FriendNotificationDelegate::paint(QPainter* painter,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);

    painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PageBackground));
    if (index.data(FriendNotificationListModel::BottomSpaceRole).toBool()) {
        painter->restore();
        return;
    }

    const QRect card = cardRect(option);
    const QRect c = contentBounds(card);
    const QRect avt = avatarRect(card);

    NotificationDelegateRenderer::drawCard(painter, card, cardMetrics());

    const QString avPath = index.data(FriendNotificationListModel::AvatarPathRole).toString();
    NotificationDelegateRenderer::drawAvatar(painter, avt, avPath, cardMetrics().avatarSize);

    const QString nick   = index.data(FriendNotificationListModel::DisplayNameRole).toString();
    const QString date   = NotificationDelegateRenderer::formatTime(
        index.data(FriendNotificationListModel::RequestDateRole).toDateTime());
    const QString msg    = index.data(FriendNotificationListModel::MessageRole).toString();
    const QString src    = index.data(FriendNotificationListModel::SourceTextRole).toString();
    const auto status    = static_cast<NotificationStatus>(
                               index.data(FriendNotificationListModel::StatusRole).toInt());
    const int hovered = index.data(FriendNotificationListModel::HoveredButtonRole).toInt();
    const bool pending = status == NotificationStatus::Pending;
    const bool pendingActionsVisible = pending
            && hovered != FriendNotificationListModel::kNoHoveredButton;

    // 4. Line 1:  nick(Accent) + " 请求添加为好友 " + date   — all inline, same font size
    {
        const QRect l1 = line1Rect(c);
        const int textRight = pending ? textRightBeforeAction(l1, card, c) : l1.right();
        const int textWidth = qMax(0, textRight - l1.left() + 1);
        const QString action = QStringLiteral(" 请求添加为好友 ");
        int x = l1.left();

        // Nick
        const int maxNickW = textWidth / 3;
        const QString elidedNick = nickFm().elidedText(nick, Qt::ElideRight, maxNickW);
        const int nickW = nickFm().horizontalAdvance(elidedNick);
        painter->setFont(nickFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::Accent));
        painter->drawText(QRect(x, l1.top(), nickW, l1.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedNick);
        x += nickW;

        // Action text
        const int availForAction = qMax(0, textRight - x + 1);
        const QString elidedAction = textFm().elidedText(action, Qt::ElideRight, availForAction);
        const int actionW = textFm().horizontalAdvance(elidedAction);
        painter->setFont(textFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(x, l1.top(), actionW, l1.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedAction);
        x += actionW;

        // Date — inline right after action text
        const int availForDate = qMax(0, textRight - x + 1);
        if (availForDate > 0) {
            const QString elidedDate = textFm().elidedText(date, Qt::ElideRight, availForDate);
            painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
            painter->drawText(QRect(x, l1.top(), availForDate, l1.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, elidedDate);
        }
    }

    // 5. Line 2:  留言：message
    {
        const QRect l2 = line2Rect(c);
        const QString label = QStringLiteral("留言：");
        const int labelW = textFm().horizontalAdvance(label);

        painter->setFont(textFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(l2.left(), l2.top(), labelW, l2.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, label);

        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        const int msgRight = pending ? textRightBeforeAction(l2, card, c) : l2.right();
        const int msgW = qMax(0, msgRight - (l2.left() + labelW) + 1);
        painter->drawText(QRect(l2.left() + labelW, l2.top(), msgW, l2.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          textFm().elidedText(msg, Qt::ElideRight, msgW));
    }

    // 6. Line 3:  来源：source
    {
        const QRect l3 = line3Rect(c);
        const QString label = QStringLiteral("来源：");
        const int labelW = textFm().horizontalAdvance(label);

        painter->setFont(textFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(l3.left(), l3.top(), labelW, l3.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, label);

        const int srcRight = pending ? textRightBeforeAction(l3, card, c) : l3.right();
        const int srcW = qMax(0, srcRight - (l3.left() + labelW) + 1);
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(l3.left() + labelW, l3.top(), srcW, l3.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          textFm().elidedText(src, Qt::ElideRight, srcW));
    }

    // 7. Buttons or status text
    if (pending && pendingActionsVisible) {
        const QRect accR = acceptBtnRect(card, c);
        const QRect rejR = rejectBtnRect(card, c);
        NotificationDelegateRenderer::drawActionButtons(
            painter, accR, rejR, hovered, buttonFont(), cardMetrics());
    } else if (pending) {
        NotificationDelegateRenderer::drawPendingDots(
            painter,
            actionRect(card, c),
            ThemeManager::instance().color(ThemeColor::TertiaryText));
    } else if (status == NotificationStatus::Accepted) {
        painter->setFont(buttonFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(actionRect(card, c),
                          Qt::AlignCenter, QStringLiteral("已同意"));
    } else if (status == NotificationStatus::Rejected) {
        painter->setFont(buttonFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(actionRect(card, c),
                          Qt::AlignCenter, QStringLiteral("已拒绝"));
    }

    painter->restore();
}

QSize FriendNotificationDelegate::sizeHint(const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    if (index.data(FriendNotificationListModel::BottomSpaceRole).toBool()) {
        return QSize(option.rect.width() > 0 ? option.rect.width() : 300,
                     FriendNotificationListModel::kBottomSpaceHeight);
    }
    return QSize(option.rect.width() > 0 ? option.rect.width() : 300, kItemHeight);
}
