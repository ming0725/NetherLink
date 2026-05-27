#include "StyledActionMenu.h"
#include "shared/services/AppFonts.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QProxyStyle>
#include <QScreen>
#include <QShowEvent>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QVariant>

#include <functional>
#include <utility>

#include "shared/theme/ThemeManager.h"

#ifdef Q_OS_MACOS
#include "platform/macos/MacStyledActionMenuBridge_p.h"
#endif

#ifdef Q_OS_WIN
#include "platform/windows/WindowsPopupChrome.h"
#endif

namespace {
constexpr auto ActionTextColorProperty = "_styledActionMenu_textColor";
constexpr auto ActionHoverTextColorProperty = "_styledActionMenu_hoverTextColor";
constexpr auto ActionHoverBackgroundColorProperty = "_styledActionMenu_hoverBackgroundColor";
constexpr auto MenuHoverColorProperty = "_styledActionMenu_hoverColor";
constexpr auto MenuSeparatorColorProperty = "_styledActionMenu_separatorColor";

constexpr int ShadowMargin = 4;
#ifdef Q_OS_WIN
constexpr int PopupChromeMargin = 0;
#else
constexpr int PopupChromeMargin = ShadowMargin;
#endif
constexpr int MenuRadius = 7;
constexpr int MenuPadding = 4;
constexpr int ItemRadius = 4;
constexpr int ItemHeight = 28;
constexpr int ItemHorizontalPadding = 10;
constexpr int ItemIconGap = 6;
constexpr int ArrowWidth = 10;
constexpr int ArrowTextGap = 12;
constexpr int ItemFontSize = 13;
constexpr int SeparatorHeight = 3;
constexpr int MaxMouseReleaseWaitAttempts = 40;
constexpr int MouseReleasePollIntervalMs = 4;
#ifdef Q_OS_WIN
constexpr int HoverTrackerIntervalMs = 16;
constexpr int SubmenuSourcePadding = 8;
constexpr int SubmenuTargetPadding = 28;
constexpr int SubmenuBridgeVerticalPadding = 18;
#endif

QColor readColor(const QVariant& value, const QColor& fallback)
{
    return value.canConvert<QColor>() ? value.value<QColor>() : fallback;
}

QString menuText(const QString& text)
{
    QString label = text.split(QLatin1Char('\t')).constFirst();
    label.remove(QLatin1Char('&'));
    return label;
}

QString shortcutText(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char('\t'));
    return parts.size() > 1 ? parts.constLast() : QString();
}

QFont menuItemFont(const QFont& base)
{
    return AppFonts::pixelSizedFont(base, ItemFontSize);
}

int menuItemRowWidth(const QFontMetrics& metrics,
                     const QString& text,
                     bool checkable,
                     bool hasIcon,
                     bool hasShortcut,
                     int shortcutWidth)
{
    int width = MenuPadding * 2 + ItemHorizontalPadding * 2 + ArrowTextGap + ArrowWidth;
    if (checkable) {
        width += 12 + ItemIconGap + 2;
    }
    if (hasIcon) {
        width += 18 + ItemIconGap;
    }

    width += metrics.horizontalAdvance(menuText(text));
    if (hasShortcut) {
        width += 12 + shortcutWidth;
    }
    return width;
}

int menuItemRowWidth(const QFontMetrics& metrics, QAction* action)
{
    if (!action || action->isSeparator()) {
        return 0;
    }

    const QString shortcut = shortcutText(action->text());
    return menuItemRowWidth(metrics,
                            action->text(),
                            action->isCheckable(),
                            !action->icon().isNull(),
                            !shortcut.isEmpty(),
                            metrics.horizontalAdvance(shortcut));
}

class StyledActionMenuSizeStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                           const QSize& size, const QWidget* widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type != CT_MenuItem) {
            return result;
        }

        const auto* menuItem = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if (menuItem && menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
            result.setHeight(SeparatorHeight);
            return result;
        }

        if (menuItem) {
            const QFontMetrics metrics(menuItemFont(widget ? widget->font() : QApplication::font()));
            const QString shortcut = shortcutText(menuItem->text);
            result.setWidth(qMax(result.width(),
                                 menuItemRowWidth(metrics,
                                                  menuItem->text,
                                                  menuItem->checkType != QStyleOptionMenuItem::NotCheckable,
                                                  !menuItem->icon.isNull(),
                                                  !shortcut.isEmpty(),
                                                  menuItem->reservedShortcutWidth > 0
                                                          ? menuItem->reservedShortcutWidth
                                                          : metrics.horizontalAdvance(shortcut))));
        }
        result.setHeight(ItemHeight);
        return result;
    }
};

void paintMenuPanel(QPainter& painter,
                    QWidget* widget,
                    const QList<QAction*>& actions,
                    const std::function<QRect(QAction*)>& actionGeometry,
                    const std::function<bool(QAction*)>& isSelected,
                    const std::function<bool(QAction*)>& hasSubmenu)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);

    const QRectF panelRect = QRectF(widget->rect()).adjusted(PopupChromeMargin + 0.5,
                                                            PopupChromeMargin + 0.5,
                                                            -PopupChromeMargin - 0.5,
                                                            -PopupChromeMargin - 0.5);
    painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::ContextMenuBorder), 1));
    painter.setBrush(ThemeManager::instance().color(ThemeColor::ContextMenuBackground));
    painter.drawRoundedRect(panelRect, MenuRadius, MenuRadius);

    painter.setFont(menuItemFont(painter.font()));

    const QColor separatorColor = readColor(widget->property(MenuSeparatorColorProperty),
                                           ThemeManager::instance().color(ThemeColor::ContextMenuSeparator));

    for (QAction* action : actions) {
        if (!action || !action->isVisible()) {
            continue;
        }

        const QRect actionRect = actionGeometry(action);
        if (!actionRect.isValid()) {
            continue;
        }

        if (action->isSeparator()) {
            const int y = actionRect.center().y();
            painter.setPen(QPen(separatorColor, 1));
            painter.drawLine(actionRect.left() + ItemHorizontalPadding, y,
                             actionRect.right() - ItemHorizontalPadding, y);
            continue;
        }

        const bool enabled = action->isEnabled();
        const bool selected = enabled && isSelected(action);
        const QRect itemRect = actionRect.adjusted(MenuPadding, 2, -MenuPadding, -2);
        const QColor defaultHoverColor = readColor(widget->property(MenuHoverColorProperty),
                                                  ThemeManager::instance().color(ThemeColor::ContextMenuHover));
        const QColor hoverColor = readColor(action->property(ActionHoverBackgroundColorProperty),
                                            defaultHoverColor);

        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(hoverColor);
            painter.drawRoundedRect(itemRect, ItemRadius, ItemRadius);
        }

        const QColor defaultTextColor = enabled
                ? ThemeManager::instance().color(ThemeColor::ContextMenuText)
                : ThemeManager::instance().color(ThemeColor::ContextMenuDisabledText);
        const QColor normalTextColor = readColor(action->property(ActionTextColorProperty),
                                                 defaultTextColor);
        const QColor hoverTextColor = readColor(action->property(ActionHoverTextColorProperty),
                                                normalTextColor);
        painter.setPen(selected ? hoverTextColor : normalTextColor);

        QRect textRect = itemRect.adjusted(ItemHorizontalPadding, 0,
                                           -ItemHorizontalPadding - ArrowTextGap - ArrowWidth, 0);
        if (action->isCheckable()) {
            const int checkSize = 12;
            const QRect checkRect(itemRect.left() + ItemHorizontalPadding,
                                  itemRect.top() + (itemRect.height() - checkSize) / 2,
                                  checkSize,
                                  checkSize);
            if (action->isChecked()) {
                QPen checkPen(selected ? hoverTextColor : normalTextColor,
                              1.8,
                              Qt::SolidLine,
                              Qt::RoundCap,
                              Qt::RoundJoin);
                painter.setPen(checkPen);
                painter.drawLine(QPointF(checkRect.left() + 2.0, checkRect.center().y() + 1.0),
                                 QPointF(checkRect.left() + 5.0, checkRect.bottom() - 2.0));
                painter.drawLine(QPointF(checkRect.left() + 5.0, checkRect.bottom() - 2.0),
                                 QPointF(checkRect.right() - 1.0, checkRect.top() + 2.0));
                painter.setPen(selected ? hoverTextColor : normalTextColor);
            }
            textRect.setLeft(checkRect.right() + ItemIconGap);
        }

        const QIcon actionIcon = action->icon();
        if (!actionIcon.isNull()) {
            const int iconSize = qMin(18, itemRect.height() - 8);
            const QRect iconRect(itemRect.left() + ItemHorizontalPadding,
                                 itemRect.top() + (itemRect.height() - iconSize) / 2,
                                 iconSize, iconSize);
            actionIcon.paint(&painter, iconRect, Qt::AlignCenter,
                             enabled ? QIcon::Normal : QIcon::Disabled);
            textRect.setLeft(iconRect.right() + 1 + ItemIconGap);
        }

        const QString shortcut = shortcutText(action->text());
        QRect shortcutRect;
        if (!shortcut.isEmpty()) {
            const int shortcutWidth = painter.fontMetrics().horizontalAdvance(shortcut);
            shortcutRect = QRect(textRect.right() - shortcutWidth, textRect.top(),
                                 shortcutWidth, textRect.height());
            textRect.setRight(shortcutRect.left() - 12);
        }

        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                         menuText(action->text()));

        if (!shortcut.isEmpty()) {
            painter.drawText(shortcutRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine,
                             shortcut);
        }

        if (hasSubmenu(action)) {
            const qreal tipX = itemRect.right() - ItemHorizontalPadding - 3.0;
            const qreal centerY = itemRect.top() + itemRect.height() / 2.0;
            QPainterPath arrow;
            arrow.moveTo(tipX - 6.0, centerY - 5.0);
            arrow.lineTo(tipX, centerY);
            arrow.lineTo(tipX - 6.0, centerY + 5.0);
            painter.setPen(QPen(selected ? hoverTextColor : ThemeManager::instance().color(ThemeColor::ContextMenuShortcutText), 1.6,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(arrow);
        }
    }
}

#ifndef Q_OS_WIN
class StyledQtMenu final : public QMenu
{
public:
    explicit StyledQtMenu(QWidget* parent = nullptr)
        : QMenu(parent)
    {
        setProperty(MenuHoverColorProperty, ThemeManager::instance().color(ThemeColor::ContextMenuHover));
        setProperty(MenuSeparatorColorProperty, ThemeManager::instance().color(ThemeColor::ContextMenuSeparator));
        setAttribute(Qt::WA_TranslucentBackground, true);
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setContentsMargins(ShadowMargin + MenuPadding,
                           ShadowMargin + MenuPadding,
                           ShadowMargin + MenuPadding,
                           ShadowMargin + MenuPadding);

        auto* shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setBlurRadius(22);
        shadowEffect->setOffset(0, 1);
        shadowEffect->setColor(ThemeManager::instance().color(ThemeColor::PopupShadow));
        setGraphicsEffect(shadowEffect);

        auto* menuStyle = new StyledActionMenuSizeStyle;
        menuStyle->setParent(this);
        setStyle(menuStyle);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        paintMenuPanel(painter,
                       this,
                       actions(),
                       [this](QAction* action) { return actionGeometry(action); },
                       [this](QAction* action) { return activeAction() == action; },
                       [](QAction* action) { return action->menu(); });
    }
};
#endif

Qt::WindowFlags menuWindowFlags()
{
#ifdef Q_OS_WIN
    return Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
#else
    return Qt::Widget;
#endif
}
} // namespace

StyledActionMenu::StyledActionMenu(QWidget* parent)
    : QWidget(parent, menuWindowFlags())
{
    setProperty(MenuHoverColorProperty, ThemeManager::instance().color(ThemeColor::ContextMenuHover));
    setProperty(MenuSeparatorColorProperty, ThemeManager::instance().color(ThemeColor::ContextMenuSeparator));
    setAttribute(Qt::WA_TranslucentBackground, true);

#ifdef Q_OS_WIN
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_menuAction = new QAction(this);
    m_hoverTracker = new QTimer(this);
    m_hoverTracker->setInterval(HoverTrackerIntervalMs);
    connect(m_hoverTracker, &QTimer::timeout, this, [this]() {
        updateHoverFromGlobalPosition(QCursor::pos());
    });
#else
    m_qtMenu = new StyledQtMenu(this);
    m_menuAction = m_qtMenu->menuAction();
    connect(m_qtMenu, &QMenu::aboutToShow, this, &StyledActionMenu::aboutToShow);
    connect(m_qtMenu, &QMenu::aboutToHide, this, &StyledActionMenu::aboutToHide);
    connect(m_qtMenu, &QMenu::triggered, this, &StyledActionMenu::triggered);
    connect(m_qtMenu, &QMenu::hovered, this, &StyledActionMenu::hovered);
#endif
}

StyledActionMenu::~StyledActionMenu() = default;

QAction* StyledActionMenu::addAction(const QString& text)
{
    auto* action = new QAction(text, this);
    addAction(action);
    return action;
}

QAction* StyledActionMenu::addAction(const QIcon& icon, const QString& text)
{
    auto* action = new QAction(icon, text, this);
    addAction(action);
    return action;
}

void StyledActionMenu::addAction(QAction* action)
{
    if (!action) {
        return;
    }

    QWidget::addAction(action);
#ifdef Q_OS_WIN
    connect(action, &QAction::changed, this, [this]() {
        updateMenuSize();
        update();
    });
    updateMenuSize();
#else
    m_qtMenu->addAction(action);
#endif
}

QAction* StyledActionMenu::addSeparator()
{
    auto* action = new QAction(this);
    action->setSeparator(true);
    addAction(action);
    return action;
}

StyledActionMenu* StyledActionMenu::addStyledMenu(const QString& title)
{
    auto* menu = new StyledActionMenu(this);
    menu->setTitle(title);
#ifdef Q_OS_WIN
    menu->m_parentMenu = this;
    addAction(menu->menuAction());
    m_submenus.insert(menu->menuAction(), menu);
#else
    QWidget::addAction(menu->menuAction());
    m_qtMenu->addMenu(menu->qtMenu());
#endif
    return menu;
}

void StyledActionMenu::clear()
{
#ifdef Q_OS_WIN
    hideChildMenus();
    const QList<QAction*> currentActions = actions();
    for (QAction* action : currentActions) {
        if (StyledActionMenu* submenu = m_submenus.take(action)) {
            submenu->deleteLater();
        }
        removeAction(action);
        if (action && action->parent() == this) {
            action->deleteLater();
        }
    }
    m_activeAction = nullptr;
    m_submenuCorridorAction = nullptr;
    updateMenuSize();
    update();
#else
    const QList<QAction*> currentActions = actions();
    for (QAction* action : currentActions) {
        QWidget::removeAction(action);
        m_qtMenu->removeAction(action);
        if (action && action->parent() == this) {
            action->deleteLater();
        }
    }
    m_qtMenu->clear();
#endif
}

QAction* StyledActionMenu::menuAction() const
{
    return m_menuAction;
}

void StyledActionMenu::setTitle(const QString& title)
{
    m_title = title;
#ifdef Q_OS_WIN
    if (m_menuAction) {
        m_menuAction->setText(title);
    }
#else
    m_qtMenu->setTitle(title);
#endif
}

QString StyledActionMenu::title() const
{
#ifdef Q_OS_WIN
    return m_title;
#else
    return m_qtMenu->title();
#endif
}

QSize StyledActionMenu::sizeHint() const
{
#ifdef Q_OS_WIN
    const QFontMetrics metrics(menuItemFont(font()));
    int maxRowWidth = 0;
    int totalHeight = 0;

    for (QAction* action : visibleActions()) {
        if (action->isSeparator()) {
            totalHeight += SeparatorHeight;
            continue;
        }

        maxRowWidth = qMax(maxRowWidth, menuItemRowWidth(metrics, action));
        totalHeight += ItemHeight;
    }

    const int edge = PopupChromeMargin + MenuPadding;
    return QSize(qMax(1, maxRowWidth) + edge * 2, qMax(ItemHeight, totalHeight) + edge * 2);
#else
    m_qtMenu->ensurePolished();
    return m_qtMenu->sizeHint();
#endif
}

void StyledActionMenu::setFixedWidth(int width)
{
    setMinimumWidth(width);
    setMaximumWidth(QWIDGETSIZE_MAX);
#ifndef Q_OS_WIN
    m_qtMenu->setMinimumWidth(width);
    m_qtMenu->setMaximumWidth(QWIDGETSIZE_MAX);
#endif
}

void StyledActionMenu::popup(const QPoint& pos, QAction* atAction)
{
#ifdef Q_OS_WIN
    Q_UNUSED(atAction);

    updateMenuSize();
    QPoint popupPos = pos;
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen) {
        const QRect available = screen->availableGeometry().adjusted(8, 8, -8, -8);
        popupPos.setX(qBound(available.left(), popupPos.x(), available.right() - width() + 1));
        popupPos.setY(qBound(available.top(), popupPos.y(), available.bottom() - height() + 1));
    }

    const bool wasVisible = isVisible();
    if (!wasVisible) {
        m_aboutToHideEmitted = false;
        emit aboutToShow();
    }
    m_pressedInside = false;
    move(popupPos);
    if (!wasVisible) {
        show();
    }
    raise();
    rootMenu()->startHoverTracking();
    if (!m_parentMenu) {
        activateWindow();
        setFocus(Qt::PopupFocusReason);
    }
    return;
#else
#ifdef Q_OS_MACOS
    if (usesNativeMenu()) {
        Q_UNUSED(atAction);
        bool hideEmitted = false;
        const auto emitNativeHide = [this, &hideEmitted]() {
            if (hideEmitted) {
                return;
            }

            hideEmitted = true;
            emit aboutToHide();
        };

        emit aboutToShow();
        MacStyledActionMenuBridge::popupMenu(this, m_qtMenu, pos, emitNativeHide);
        emitNativeHide();
        return;
    }
#endif

    m_qtMenu->popup(pos, atAction);
#endif
}

void StyledActionMenu::popupWhenMouseReleased(const QPoint& pos, QAction* atAction)
{
    popupWhenMouseReleased(pos, atAction, 0);
}

void StyledActionMenu::popupWhenMouseReleased(const QPoint& pos, QAction* atAction, int attempt)
{
    const Qt::MouseButtons buttons = QGuiApplication::mouseButtons();
    const bool hasPressedButton = buttons.testFlag(Qt::LeftButton) ||
            buttons.testFlag(Qt::RightButton) ||
            buttons.testFlag(Qt::MiddleButton);

    if (!hasPressedButton || attempt >= MaxMouseReleaseWaitAttempts) {
        popup(pos, atAction);
        return;
    }

    QTimer::singleShot(MouseReleasePollIntervalMs, this, [this, pos, atAction, attempt]() {
        popupWhenMouseReleased(pos, atAction, attempt + 1);
    });
}

bool StyledActionMenu::isUsingNativeMenu() const
{
    return usesNativeMenu();
}

void StyledActionMenu::setItemHoverColor(const QColor& color)
{
    setProperty(MenuHoverColorProperty, color);
#ifndef Q_OS_WIN
    m_qtMenu->setProperty(MenuHoverColorProperty, color);
#endif
    update();
}

void StyledActionMenu::setSeparatorColor(const QColor& color)
{
    setProperty(MenuSeparatorColorProperty, color);
#ifndef Q_OS_WIN
    m_qtMenu->setProperty(MenuSeparatorColorProperty, color);
#endif
    update();
}

void StyledActionMenu::setActionTextColor(QAction* action, const QColor& color)
{
    if (action) {
        action->setProperty(ActionTextColorProperty, color);
    }
}

void StyledActionMenu::setActionHoverTextColor(QAction* action, const QColor& color)
{
    if (action) {
        action->setProperty(ActionHoverTextColorProperty, color);
    }
}

void StyledActionMenu::setActionHoverBackgroundColor(QAction* action, const QColor& color)
{
    if (action) {
        action->setProperty(ActionHoverBackgroundColorProperty, color);
    }
}

void StyledActionMenu::setActionColors(QAction* action, const QColor& textColor,
                                       const QColor& hoverTextColor)
{
    setActionTextColor(action, textColor);
    setActionHoverTextColor(action, hoverTextColor);
}

void StyledActionMenu::setActionColors(QAction* action, const QColor& textColor,
                                       const QColor& hoverTextColor,
                                       const QColor& hoverBackgroundColor)
{
    setActionColors(action, textColor, hoverTextColor);
    setActionHoverBackgroundColor(action, hoverBackgroundColor);
}

void StyledActionMenu::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
#ifdef Q_OS_WIN
    QPainter painter(this);
    paintMenuPanel(painter,
                   this,
                   actions(),
                   [this](QAction* action) { return actionGeometry(action); },
                   [this](QAction* action) { return m_activeAction == action; },
                   [this](QAction* action) { return m_submenus.contains(action); });
#endif
}

void StyledActionMenu::showEvent(QShowEvent* event)
{
#ifdef Q_OS_WIN
    QWidget::showEvent(event);
    WindowsPopupChrome::applyModernShadow(this);
#else
    QWidget::showEvent(event);
#endif
}

void StyledActionMenu::hideEvent(QHideEvent* event)
{
#ifdef Q_OS_WIN
    QWidget::hideEvent(event);
    hideChildMenus();
    m_activeAction = nullptr;
    m_pressedInside = false;
    m_submenuCorridorAction = nullptr;
    if (!m_parentMenu) {
        stopHoverTracking();
    }
    if (!m_aboutToHideEmitted) {
        m_aboutToHideEmitted = true;
        emit aboutToHide();
    }
#else
    QWidget::hideEvent(event);
#endif
}

#ifndef Q_OS_WIN
QMenu* StyledActionMenu::qtMenu() const
{
    return m_qtMenu;
}
#endif

#ifdef Q_OS_WIN
void StyledActionMenu::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        rootMenu()->closeMenuTree();
        event->accept();
        return;
    case Qt::Key_Down:
        setActiveAction(nextSelectableAction(1));
        event->accept();
        return;
    case Qt::Key_Up:
        setActiveAction(nextSelectableAction(-1));
        event->accept();
        return;
    case Qt::Key_Right:
        showSubmenuForAction(m_activeAction);
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        triggerAction(m_activeAction);
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void StyledActionMenu::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    const QPoint globalPos = QCursor::pos();
    if (!childMenuContainsGlobalPoint(globalPos)
            && !isInOpenSubmenuHoverCorridor(globalPos)) {
        setActiveAction(nullptr);
    }
}

void StyledActionMenu::mouseMoveEvent(QMouseEvent* event)
{
    updateHoverBranchFromGlobalPosition(mapToGlobal(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void StyledActionMenu::mousePressEvent(QMouseEvent* event)
{
    m_pressedInside = event->button() == Qt::LeftButton && actionAt(event->pos());
    QWidget::mousePressEvent(event);
}

void StyledActionMenu::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QAction* action = actionAt(event->pos());
        if (action && m_pressedInside) {
            triggerAction(action);
            event->accept();
            return;
        }
    }

    QWidget::mouseReleaseEvent(event);
}

QRect StyledActionMenu::actionGeometry(QAction* action) const
{
    if (!action || !action->isVisible()) {
        return {};
    }

    const int edge = PopupChromeMargin + MenuPadding;
    int y = edge;
    const int rowWidth = qMax(0, width() - edge * 2);
    for (QAction* candidate : visibleActions()) {
        const int rowHeight = candidate->isSeparator() ? SeparatorHeight : ItemHeight;
        if (candidate == action) {
            return QRect(edge, y, rowWidth, rowHeight);
        }
        y += rowHeight;
    }
    return {};
}

QAction* StyledActionMenu::actionAt(const QPoint& pos) const
{
    for (QAction* action : visibleActions()) {
        if (action->isSeparator() || !action->isEnabled()) {
            continue;
        }
        if (actionGeometry(action).contains(pos)) {
            return action;
        }
    }
    return nullptr;
}

QList<QAction*> StyledActionMenu::visibleActions() const
{
    QList<QAction*> result;
    for (QAction* action : actions()) {
        if (action && action->isVisible()) {
            result.append(action);
        }
    }
    return result;
}

void StyledActionMenu::closeMenuTree()
{
    hideChildMenus();
    hide();
}

void StyledActionMenu::hideChildMenus(StyledActionMenu* except)
{
    for (StyledActionMenu* submenu : std::as_const(m_submenus)) {
        if (!submenu || submenu == except) {
            continue;
        }
        submenu->hideChildMenus();
        submenu->hide();
    }
}

void StyledActionMenu::setActiveAction(QAction* action)
{
    if (m_activeAction == action) {
        return;
    }

    m_activeAction = action;
    if (action) {
        emit hovered(action);
    }
    update();
}

void StyledActionMenu::showSubmenuForAction(QAction* action)
{
    StyledActionMenu* submenu = m_submenus.value(action, nullptr);
    if (!submenu) {
        return;
    }

    const QRect actionRect = actionGeometry(action);
    if (!actionRect.isValid()) {
        return;
    }

    hideChildMenus(submenu);
    const QPoint globalCursor = QCursor::pos();
    const QRect globalActionRect(mapToGlobal(actionRect.topLeft()), actionRect.size());
    m_submenuCorridorAnchorGlobalPos = globalActionRect.contains(globalCursor)
            ? globalCursor
            : QPoint(globalActionRect.right(), globalActionRect.center().y());
    m_submenuCorridorAction = action;

    QPoint pos = mapToGlobal(QPoint(actionRect.right() - 2,
                                    actionRect.top() - PopupChromeMargin));
    if (submenu->isVisible()) {
        if (submenu->pos() != pos) {
            submenu->move(pos);
        }
        submenu->raise();
        return;
    }
    submenu->popup(pos);
}

StyledActionMenu* StyledActionMenu::rootMenu()
{
    StyledActionMenu* menu = this;
    while (menu->m_parentMenu) {
        menu = menu->m_parentMenu;
    }
    return menu;
}

bool StyledActionMenu::containsGlobalPoint(const QPoint& globalPos) const
{
    return isVisible() && rect().contains(mapFromGlobal(globalPos));
}

bool StyledActionMenu::childMenuContainsGlobalPoint(const QPoint& globalPos) const
{
    for (StyledActionMenu* submenu : m_submenus) {
        if (!submenu) {
            continue;
        }
        if (submenu->containsGlobalPoint(globalPos) || submenu->childMenuContainsGlobalPoint(globalPos)) {
            return true;
        }
    }
    return false;
}

bool StyledActionMenu::hasReachedSubmenuCorridorTarget(QAction* action) const
{
    if (m_submenuCorridorAction != action || m_submenuCorridorAnchorGlobalPos.isNull()) {
        return false;
    }

    StyledActionMenu* submenu = m_submenus.value(action, nullptr);
    if (!submenu || !submenu->isVisible()) {
        return false;
    }

    const QRect submenuRect(submenu->mapToGlobal(QPoint(0, 0)), submenu->size());
    return submenuRect.contains(m_submenuCorridorAnchorGlobalPos);
}

bool StyledActionMenu::isInSubmenuHoverCorridor(QAction* action, const QPoint& globalPos) const
{
    StyledActionMenu* submenu = m_submenus.value(action, nullptr);
    if (!submenu || !submenu->isVisible()) {
        return false;
    }

    const QRect localActionRect = actionGeometry(action);
    if (!localActionRect.isValid()) {
        return false;
    }

    const QRect rawActionRect(mapToGlobal(localActionRect.topLeft()), localActionRect.size());
    const QRect rawSubmenuRect(submenu->mapToGlobal(QPoint(0, 0)), submenu->size());
    const bool submenuOnRight = rawSubmenuRect.center().x() >= rawActionRect.center().x();
    QRect actionRect = rawActionRect;
    actionRect.adjust(-SubmenuSourcePadding,
                      -SubmenuSourcePadding,
                      SubmenuSourcePadding,
                      SubmenuSourcePadding);
    const bool targetReached = hasReachedSubmenuCorridorTarget(action);
    QRect submenuRect = rawSubmenuRect;
    if (submenuOnRight) {
        submenuRect.adjust(targetReached ? 0 : -SubmenuTargetPadding,
                           -SubmenuTargetPadding,
                           SubmenuTargetPadding,
                           SubmenuTargetPadding);
    } else {
        submenuRect.adjust(-SubmenuTargetPadding,
                           -SubmenuTargetPadding,
                           targetReached ? 0 : SubmenuTargetPadding,
                           SubmenuTargetPadding);
    }

    if (actionRect.contains(globalPos) || submenuRect.contains(globalPos)) {
        return true;
    }

    const int bridgeLeft = submenuOnRight ? actionRect.right() : submenuRect.right();
    const int bridgeRight = submenuOnRight ? submenuRect.left() : actionRect.left();
    const QRect bridgeRect(qMin(bridgeLeft, bridgeRight),
                           qMin(actionRect.top(), submenuRect.top()) - SubmenuBridgeVerticalPadding,
                           qMax(1, qAbs(bridgeRight - bridgeLeft)),
                           qMax(actionRect.bottom(), submenuRect.bottom())
                                   - qMin(actionRect.top(), submenuRect.top())
                                   + SubmenuBridgeVerticalPadding * 2);
    if (bridgeRect.contains(globalPos)) {
        return true;
    }

    QPointF apex = (m_submenuCorridorAction == action && !m_submenuCorridorAnchorGlobalPos.isNull())
            ? QPointF(m_submenuCorridorAnchorGlobalPos)
            : QPointF(submenuOnRight ? actionRect.right() : actionRect.left(), actionRect.center().y());
    if (submenuOnRight) {
        apex.setX(qMax<qreal>(apex.x(), actionRect.right() - 2));
    } else {
        apex.setX(qMin<qreal>(apex.x(), actionRect.left() + 2));
    }

    QPolygonF triangle;
    if (submenuOnRight) {
        triangle << apex
                 << QPointF(submenuRect.left(), submenuRect.top())
                 << QPointF(submenuRect.left(), submenuRect.bottom());
    } else {
        triangle << apex
                 << QPointF(submenuRect.right(), submenuRect.top())
                 << QPointF(submenuRect.right(), submenuRect.bottom());
    }
    return triangle.containsPoint(QPointF(globalPos), Qt::OddEvenFill);
}

bool StyledActionMenu::isInOpenSubmenuHoverCorridor(const QPoint& globalPos) const
{
    for (auto it = m_submenus.constBegin(); it != m_submenus.constEnd(); ++it) {
        StyledActionMenu* submenu = it.value();
        if (!submenu || !submenu->isVisible()) {
            continue;
        }

        if (isInSubmenuHoverCorridor(it.key(), globalPos)
                || submenu->isInOpenSubmenuHoverCorridor(globalPos)) {
            return true;
        }
    }

    return false;
}

void StyledActionMenu::updateSubmenuCorridorAnchor(QAction* action, const QPoint& globalPos)
{
    if (m_submenuCorridorAction != action) {
        return;
    }

    StyledActionMenu* submenu = m_submenus.value(action, nullptr);
    if (!submenu || !submenu->isVisible()) {
        return;
    }

    if (submenu->rect().contains(submenu->mapFromGlobal(globalPos))) {
        m_submenuCorridorAnchorGlobalPos = globalPos;
    }
}

void StyledActionMenu::startHoverTracking()
{
    if (m_hoverTracker && !m_hoverTracker->isActive()) {
        m_hoverTracker->start();
    }
}

void StyledActionMenu::stopHoverTracking()
{
    if (m_hoverTracker) {
        m_hoverTracker->stop();
    }
}

void StyledActionMenu::updateHoverFromGlobalPosition(const QPoint& globalPos)
{
    if (!isVisible()) {
        stopHoverTracking();
        return;
    }

    if (QGuiApplication::mouseButtons() != Qt::NoButton) {
        return;
    }

    if (updateHoverBranchFromGlobalPosition(globalPos)) {
        return;
    }

    if (!isInOpenSubmenuHoverCorridor(globalPos)) {
        hideChildMenus();
        setActiveAction(nullptr);
        m_submenuCorridorAction = nullptr;
    }
}

bool StyledActionMenu::updateHoverBranchFromGlobalPosition(const QPoint& globalPos)
{
    if (!isVisible()) {
        return false;
    }

    const QPoint localPos = mapFromGlobal(globalPos);
    if (rect().contains(localPos)) {
        QAction* action = actionAt(localPos);
        if (action != m_activeAction
                && m_activeAction
                && !hasReachedSubmenuCorridorTarget(m_activeAction)
                && isInSubmenuHoverCorridor(m_activeAction, globalPos)) {
            return true;
        }

        setActiveAction(action);
        if (action && m_submenus.contains(action)) {
            showSubmenuForAction(action);
        } else if (action) {
            hideChildMenus();
            m_submenuCorridorAction = nullptr;
        } else {
            hideChildMenus();
            m_submenuCorridorAction = nullptr;
        }
        return true;
    }

    for (auto it = m_submenus.constBegin(); it != m_submenus.constEnd(); ++it) {
        QAction* action = it.key();
        StyledActionMenu* submenu = it.value();
        if (!submenu || !submenu->isVisible()) {
            continue;
        }

        if (submenu->updateHoverBranchFromGlobalPosition(globalPos)) {
            updateSubmenuCorridorAnchor(action, globalPos);
            setActiveAction(action);
            hideChildMenus(submenu);
            return true;
        }
    }

    return isInOpenSubmenuHoverCorridor(globalPos);
}

QAction* StyledActionMenu::nextSelectableAction(int direction) const
{
    const QList<QAction*> candidates = visibleActions();
    if (candidates.isEmpty()) {
        return nullptr;
    }

    int start = candidates.indexOf(m_activeAction);
    if (start < 0) {
        start = direction > 0 ? -1 : candidates.size();
    }

    for (int step = 1; step <= candidates.size(); ++step) {
        const int index = (start + direction * step + candidates.size()) % candidates.size();
        QAction* action = candidates.at(index);
        if (action && action->isEnabled() && !action->isSeparator()) {
            return action;
        }
    }
    return nullptr;
}

void StyledActionMenu::triggerAction(QAction* action)
{
    if (!action || action->isSeparator() || !action->isEnabled()) {
        return;
    }

    if (m_submenus.contains(action)) {
        showSubmenuForAction(action);
        return;
    }

    action->trigger();
    emit rootMenu()->triggered(action);
    rootMenu()->closeMenuTree();
}

void StyledActionMenu::updateMenuSize()
{
    resize(sizeHint());
}
#endif

bool StyledActionMenu::usesNativeMenu() const
{
#ifdef Q_OS_MACOS
    return MacStyledActionMenuBridge::isSupported();
#else
    return false;
#endif
}
