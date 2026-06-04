#include "UnreadStateRepository.h"

#include "shared/data/LocalDataStore.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

constexpr auto kUnreadDomain = "unread_state";

QJsonObject unreadScopeToJson(const QString& scope, const QSet<QString>& items)
{
    QJsonArray array;
    for (const QString& itemId : items) {
        array.append(itemId);
    }
    return {
            {QStringLiteral("scope"), scope},
            {QStringLiteral("items"), array}
    };
}

QSet<QString> unreadScopeFromJson(const QJsonObject& object)
{
    QSet<QString> result;
    for (const QJsonValue& value : object.value(QStringLiteral("items")).toArray()) {
        const QString itemId = value.toString();
        if (!itemId.isEmpty()) {
            result.insert(itemId);
        }
    }
    return result;
}

} // namespace

UnreadStateRepository::UnreadStateRepository(QObject* parent)
    : QObject(parent)
{
    reloadFromStore();

    connect(&LocalDataStore::instance(),
            &LocalDataStore::activeAccountChanged,
            this,
            [this](const QString&) {
                reloadFromStore();
            });
    connect(&LocalDataStore::instance(),
            &LocalDataStore::domainChanged,
            this,
            [this](const QString& domain) {
                if (domain == QString::fromLatin1(kUnreadDomain)) {
                    reloadFromStore();
                }
            },
            Qt::QueuedConnection);
}

UnreadStateRepository& UnreadStateRepository::instance()
{
    static UnreadStateRepository repo;
    return repo;
}

void UnreadStateRepository::reloadFromStore()
{
    QHash<QString, QSet<QString>> nextUnreadByScope;
    for (const QJsonObject& object : LocalDataStore::instance().values(QString::fromLatin1(kUnreadDomain))) {
        const QString scope = object.value(QStringLiteral("scope")).toString();
        if (!scope.isEmpty()) {
            nextUnreadByScope.insert(scope, unreadScopeFromJson(object));
        }
    }

    QSet<QString> oldScopes;
    for (auto it = m_unreadByScope.keyBegin(); it != m_unreadByScope.keyEnd(); ++it) {
        oldScopes.insert(*it);
    }
    QSet<QString> nextScopes;
    for (auto it = nextUnreadByScope.keyBegin(); it != nextUnreadByScope.keyEnd(); ++it) {
        nextScopes.insert(*it);
    }
    m_unreadByScope = nextUnreadByScope;

    QSet<QString> scopes = oldScopes;
    scopes.unite(nextScopes);
    for (const QString& scope : scopes) {
        emit unreadCountChanged(scope, m_unreadByScope.value(scope).size());
    }
}

bool UnreadStateRepository::isUnread(const QString& scope, const QString& itemId) const
{
    return m_unreadByScope.value(scope).contains(itemId);
}

int UnreadStateRepository::unreadCount(const QString& scope) const
{
    return m_unreadByScope.value(scope).size();
}

void UnreadStateRepository::setUnread(const QString& scope, const QString& itemId, bool unread)
{
    if (scope.isEmpty() || itemId.isEmpty()) {
        return;
    }

    QSet<QString>& items = m_unreadByScope[scope];
    const bool wasUnread = items.contains(itemId);
    if (wasUnread == unread) {
        return;
    }

    if (unread) {
        items.insert(itemId);
    } else {
        items.remove(itemId);
    }
    LocalDataStore::instance().upsertValue(QString::fromLatin1(kUnreadDomain),
                                           scope,
                                           unreadScopeToJson(scope, items));
    emit unreadCountChanged(scope, items.size());
}

void UnreadStateRepository::markAllRead(const QString& scope)
{
    auto it = m_unreadByScope.find(scope);
    if (it == m_unreadByScope.end() || it->isEmpty()) {
        return;
    }

    it->clear();
    LocalDataStore::instance().upsertValue(QString::fromLatin1(kUnreadDomain),
                                           scope,
                                           unreadScopeToJson(scope, *it));
    emit unreadCountChanged(scope, 0);
}
