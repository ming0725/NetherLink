#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QWidget>

#include "shared/types/Group.h"
#include "shared/types/RepositoryTypes.h"
#include "shared/ui/OverlayScrollListView.h"

class IconLineEdit;
class StatefulPushButton;
class QVariantAnimation;

struct CreateGroupChatSourceGroup {
    QString groupId;
    QString groupName;
    QVector<FriendSummary> friends;
};

class CreateGroupChatContactModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UserIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        AvatarPathRole,
        StatusRole,
        SignatureRole,
        NickNameRole,
        RemarkRole,
        IsGroupRole,
        GroupIdRole,
        GroupNameRole,
        GroupFriendCountRole,
        GroupExpandedRole,
        GroupProgressRole,
        SelectedRole,
        DisabledRole
    };

    explicit CreateGroupChatContactModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void reload(const QString& keyword,
                const QVector<FriendSummary>& recentContacts,
                const QVector<CreateGroupChatSourceGroup>& friendGroups,
                const QSet<QString>& selectedUserIds,
                const QSet<QString>& disabledUserIds = {});
    void reloadContacts(const QString& keyword,
                        const QVector<FriendSummary>& contacts,
                        const QString& groupId,
                        const QString& groupName,
                        const QSet<QString>& selectedUserIds,
                        const QSet<QString>& disabledUserIds);
    void setSelectedUserIds(const QSet<QString>& selectedUserIds);
    void setGroupExpanded(const QString& groupId, bool expanded);
    void setGroupProgress(const QString& groupId, qreal progress);
    void pruneCollapsedRows();

    bool isGroupRow(const QModelIndex& index) const;
    bool isContactRow(const QModelIndex& index) const;
    QString userIdAt(const QModelIndex& index) const;
    FriendSummary contactAt(const QModelIndex& index) const;
    QString groupIdAt(const QModelIndex& index) const;
    bool isContactDisabled(const QModelIndex& index) const;
    int groupRowForRow(int row) const;
    int nextGroupRow(int row) const;
    bool isGroupExpanded(const QString& groupId) const;
    qreal groupProgress(const QString& groupId) const;

private:
    struct ContactGroup {
        QString groupId;
        QString groupName;
        QVector<FriendSummary> friends;
        int totalCount = 0;
        bool expanded = false;
        qreal progress = 0.0;
    };

    struct RowEntry {
        bool isGroup = true;
        int groupIndex = -1;
        int friendIndex = -1;
    };

    ContactGroup* groupForId(const QString& groupId);
    const ContactGroup* groupForId(const QString& groupId) const;
    void rebuildRows();
    int rowForGroup(const QString& groupId) const;
    int lastRowForGroup(const QString& groupId) const;

    QVector<ContactGroup> m_groups;
    QVector<RowEntry> m_rows;
    QSet<QString> m_selectedUserIds;
    QSet<QString> m_disabledUserIds;
};

class CreateGroupChatContactDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CreateGroupChatContactDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    void clearPaintCache();
    void invalidatePaintCache(const QModelIndex& topLeft,
                              const QModelIndex& bottomRight,
                              const QVector<int>& roles);

private:
    struct ThemePaintCache {
        bool valid = false;
        bool dark = false;
        QColor popupHover;
        QColor primaryText;
        QColor secondaryText;
        QColor tertiaryText;
    };

    struct ContactPaintCache {
        QString userId;
        QString displayName;
        QString avatarPath;
    };

    const ThemePaintCache& themePaintCache() const;
    ContactPaintCache contactPaintCache(const QModelIndex& index) const;

    mutable ThemePaintCache m_themePaintCache;
    mutable QHash<QString, ContactPaintCache> m_contactPaintCache;
};

class CreateGroupChatContactListView final : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit CreateGroupChatContactListView(QWidget* parent = nullptr);
    void reload(const QString& keyword,
                const QVector<FriendSummary>& recentContacts,
                const QVector<CreateGroupChatSourceGroup>& friendGroups,
                const QSet<QString>& selectedUserIds,
                const QSet<QString>& disabledUserIds = {});
    void reloadContacts(const QString& keyword,
                        const QVector<FriendSummary>& contacts,
                        const QString& groupId,
                        const QString& groupName,
                        const QSet<QString>& selectedUserIds,
                        const QSet<QString>& disabledUserIds);
    void setSelectedUserIds(const QSet<QString>& selectedUserIds);

signals:
    void contactToggled(const FriendSummary& contact);

protected:
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    struct StickyGroupData {
        QString groupId;
        QString title;
        int count = 0;
        qreal progress = 0.0;
    };

    struct StickyHeaderState {
        bool visible = false;
        StickyGroupData group;
        int offsetY = 0;
    };

    void toggleGroup(const QString& groupId);
    void toggleStickyGroupById(const QString& groupId);
    void setGroupExpandedAnimated(const QString& groupId, bool expanded);
    void updateStickyHeader();
    StickyHeaderState calculateStickyHeaderState() const;
    StickyGroupData stickyGroupDataForRow(int row) const;
    void drawStickyHeader() const;
    void drawStickyGroup(QPainter* painter, const QRect& rect, const StickyGroupData& group) const;

    CreateGroupChatContactModel* m_model = nullptr;
    CreateGroupChatContactDelegate* m_delegate = nullptr;
    QHash<QString, QPointer<QVariantAnimation>> m_groupAnimations;
    StickyGroupData m_stickyGroup;
    bool m_stickyVisible = false;
    int m_stickyOffsetY = 0;
};

class CreateGroupChatSelectedModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UserIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        AvatarPathRole,
        SignatureRole
    };

    explicit CreateGroupChatSelectedModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setContacts(QVector<FriendSummary> contacts);
    FriendSummary contactAt(const QModelIndex& index) const;

private:
    QVector<FriendSummary> m_contacts;
};

class CreateGroupChatSelectedDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CreateGroupChatSelectedDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    static QRect removeButtonRect(const QRect& itemRect);
};

class CreateGroupChatSelectedListView final : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit CreateGroupChatSelectedListView(QWidget* parent = nullptr);
    void setContacts(QVector<FriendSummary> contacts);

signals:
    void removeRequested(const QString& userId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    CreateGroupChatSelectedModel* m_model = nullptr;
    CreateGroupChatSelectedDelegate* m_delegate = nullptr;
};

class CreateGroupChatPopup final : public QWidget
{
    Q_OBJECT

public:
    explicit CreateGroupChatPopup(QWidget* parent = nullptr);
    static QString open(QWidget* parent);
    static QStringList openInviteMembers(QWidget* parent, const Group& group);
    static QStringList openPreviewInviteMembers(QWidget* parent, const Group& group);
    static QStringList openRemoveMembers(QWidget* parent, const Group& group);

signals:
    void accepted(const QString& groupId);
    void selectionAccepted(const QStringList& userIds);
    void rejected();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyTheme();
    void reloadContacts();
    void ensureContactCache();
    void toggleContact(const FriendSummary& contact);
    void removeContact(const QString& userId);
    void refreshSelectedView();
    void createGroup();
    void acceptSelection();
    void configureForInvite(const Group& group, bool commitEnabled);
    void configureForRemove(const Group& group);
    void setModeTitle(const QString& title, const QString& countUnit);
    static QStringList openSelectionPopup(QWidget* parent, CreateGroupChatPopup* content);

    IconLineEdit* m_searchInput = nullptr;
    CreateGroupChatContactListView* m_contactList = nullptr;
    CreateGroupChatSelectedListView* m_selectedList = nullptr;
    StatefulPushButton* m_cancelButton = nullptr;
    StatefulPushButton* m_okButton = nullptr;
    QVector<FriendSummary> m_selectedContacts;
    QSet<QString> m_selectedUserIds;
    QSet<QString> m_disabledUserIds;
    QVector<FriendSummary> m_sourceContacts;
    QVector<FriendSummary> m_recentContacts;
    QVector<CreateGroupChatSourceGroup> m_friendGroups;
    QString m_sourceGroupId;
    QString m_sourceGroupName;
    QString m_title = QStringLiteral("创建群聊");
    QString m_countUnit = QStringLiteral("好友");
    bool m_useCustomContacts = false;
    bool m_commitSelection = true;
    bool m_contactCacheLoaded = false;
};
