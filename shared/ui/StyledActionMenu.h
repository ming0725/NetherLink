#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QPoint>
#include <QWidget>

class QAction;
class QPaintEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QShowEvent;
class QTimer;
class QMenu;

class StyledActionMenu : public QWidget
{
    Q_OBJECT

public:
    explicit StyledActionMenu(QWidget* parent = nullptr);
    ~StyledActionMenu() override;

    QAction* addAction(const QString& text);
    QAction* addAction(const QIcon& icon, const QString& text);
    void addAction(QAction* action);
    QAction* addSeparator();
    StyledActionMenu* addStyledMenu(const QString& title);
    void clear();
    void popup(const QPoint& pos, QAction* atAction = nullptr);
    void popupWhenMouseReleased(const QPoint& pos, QAction* atAction = nullptr);
    QAction* menuAction() const;
    void setTitle(const QString& title);
    QString title() const;
    QSize sizeHint() const override;
    void setFixedWidth(int width);
    bool isUsingNativeMenu() const;

    void setItemHoverColor(const QColor& color);
    void setSeparatorColor(const QColor& color);

    static void setActionTextColor(QAction* action, const QColor& color);
    static void setActionHoverTextColor(QAction* action, const QColor& color);
    static void setActionHoverBackgroundColor(QAction* action, const QColor& color);
    static void setActionColors(QAction* action, const QColor& textColor,
                                const QColor& hoverTextColor);
    static void setActionColors(QAction* action, const QColor& textColor,
                                const QColor& hoverTextColor,
                                const QColor& hoverBackgroundColor);

signals:
    void aboutToShow();
    void aboutToHide();
    void triggered(QAction* action);
    void hovered(QAction* action);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
#ifdef Q_OS_WIN
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
#endif

private:
    bool usesNativeMenu() const;
    void popupWhenMouseReleased(const QPoint& pos, QAction* atAction, int attempt);

#ifndef Q_OS_WIN
    QMenu* qtMenu() const;
#endif

#ifdef Q_OS_WIN
    QRect actionGeometry(QAction* action) const;
    QAction* actionAt(const QPoint& pos) const;
    QList<QAction*> visibleActions() const;
    void closeMenuTree();
    void hideChildMenus(StyledActionMenu* except = nullptr);
    void setActiveAction(QAction* action);
    void showSubmenuForAction(QAction* action);
    StyledActionMenu* rootMenu();
    bool containsGlobalPoint(const QPoint& globalPos) const;
    bool childMenuContainsGlobalPoint(const QPoint& globalPos) const;
    bool hasReachedSubmenuCorridorTarget(QAction* action) const;
    bool isInSubmenuHoverCorridor(QAction* action, const QPoint& globalPos) const;
    bool isInOpenSubmenuHoverCorridor(const QPoint& globalPos) const;
    void updateSubmenuCorridorAnchor(QAction* action, const QPoint& globalPos);
    void startHoverTracking();
    void stopHoverTracking();
    void updateHoverFromGlobalPosition(const QPoint& globalPos);
    bool updateHoverBranchFromGlobalPosition(const QPoint& globalPos);
    QAction* nextSelectableAction(int direction) const;
    void triggerAction(QAction* action);
    void updateMenuSize();

    QAction* m_activeAction = nullptr;
    StyledActionMenu* m_parentMenu = nullptr;
    QHash<QAction*, StyledActionMenu*> m_submenus;
    QTimer* m_hoverTracker = nullptr;
    QPoint m_submenuCorridorAnchorGlobalPos;
    QAction* m_submenuCorridorAction = nullptr;
    bool m_pressedInside = false;
    bool m_aboutToHideEmitted = true;
#else
    QMenu* m_qtMenu = nullptr;
#endif

    QAction* m_menuAction = nullptr;
    QString m_title;
};
