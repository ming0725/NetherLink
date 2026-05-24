#pragma once

#include "platform/SystemWindow.h"

#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QVariantAnimation>

#include "shared/types/Group.h"
#include "shared/types/User.h"

class IconLineEdit;
class QLabel;
class OverlayScrollListView;
class QtFallbackLiquidGlassController;
class QTimer;

struct AddContactSearchItem {
    enum class Type {
        User,
        Group
    };

    Type type = Type::User;
    User user;
    Group group;
};

class AddContactSearchModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        UserIdRole,
        UserNameRole,
        UserAvatarRole,
        GroupIdRole,
        GroupNameRole,
        GroupAvatarRole,
        GroupMemberCountRole,
        GroupIntroductionRole,
        ActionTextRole,
        ActionEnabledRole
    };

    explicit AddContactSearchModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setItems(QVector<AddContactSearchItem> items);
    void appendItems(const QVector<AddContactSearchItem>& items);
    void clear();
    AddContactSearchItem itemAt(int row) const;

private:
    QVector<AddContactSearchItem> m_items;
};

class AddContactSearchDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit AddContactSearchDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

signals:
    void actionRequested(const QModelIndex& index);
};

class AddContactModeBar final : public QWidget
{
    Q_OBJECT

public:
    enum class Mode {
        Users,
        Groups
    };

    explicit AddContactModeBar(QWidget* parent = nullptr);
    ~AddContactModeBar() override;

    void setMode(Mode mode, bool animate = true);
    Mode mode() const { return m_mode; }
    void setLiquidGlassSourceWidget(QWidget* widget);
    void scheduleLiquidGlassUpdate(int delayMs = 0);

signals:
    void modeChanged(AddContactModeBar::Mode mode);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onNativeSelectionChanged(int index);

private:
    struct Segment {
        QString label;
        QRect rect;
    };

    void layoutSegments();
    void syncPlatformBar();
    void updatePanelShadow();
    void updateQtFallbackLiquidGlassState();
    void releaseQtFallbackLiquidGlassResources(bool updateWidget = true);
    bool shouldUseQtFallbackLiquidGlass() const;
    int indexAtPosition(const QPoint& pos) const;
    int selectedIndex() const;
    QStringList labels() const;
    void setSelectedIndex(int index, bool animate);

    QtFallbackLiquidGlassController* m_liquidGlass = nullptr;
    QVector<Segment> m_segments;
    QVariantAnimation* m_selectionAnimation = nullptr;
    Mode m_mode = Mode::Users;
    QRect m_selectedRect;
    int m_highlightX = 0;
    int m_hoveredIndex = -1;
    bool m_usesNativeBar = false;
};

class AddContactSearchWindow final : public SystemWindow
{
    Q_OBJECT

public:
    enum class InitialMode {
        Users,
        Groups
    };

    explicit AddContactSearchWindow(InitialMode mode, QWidget* parent = nullptr);
    ~AddContactSearchWindow() override;

    static void open(InitialMode mode, QWidget* anchor);
    static bool openUserRequest(const QString& userId, QWidget* anchor);
    static bool openGroupRequest(const QString& groupId, QWidget* anchor);
    static bool openFriendApproval(const QString& userId,
                                   QWidget* anchor,
                                   QString* remark,
                                   QString* groupId,
                                   QString* groupName);
    static bool openGroupApproval(const QString& groupId,
                                  QWidget* anchor,
                                  QString* remark,
                                  QString* categoryId,
                                  QString* categoryName);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void applyTheme();
    void updateLayout();
    void setMode(AddContactModeBar::Mode mode);
    void scheduleSearch();
    void performSearch();
    void loadMoreResults();
    void maybeLoadMoreResults();
    void resetSearchState();
    QVector<AddContactSearchItem> searchUsers(const QString& keyword) const;
    QVector<AddContactSearchItem> searchGroups(const QString& keyword) const;
    void handleResultAction(const QModelIndex& index);

    QWidget* m_content = nullptr;
    QLabel* m_titleLabel = nullptr;
    IconLineEdit* m_searchInput = nullptr;
    QWidget* m_divider = nullptr;
    OverlayScrollListView* m_resultView = nullptr;
    AddContactSearchModel* m_model = nullptr;
    AddContactModeBar* m_modeBar = nullptr;
    QTimer* m_debounceTimer = nullptr;
    AddContactModeBar::Mode m_mode = AddContactModeBar::Mode::Users;
    QVector<AddContactSearchItem> m_searchResults;
    int m_loadedResultCount = 0;
    bool m_isLoadingMore = false;
};
