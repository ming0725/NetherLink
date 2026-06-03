#include "AuthSession.h"

#include <QCoreApplication>
#include <QMutexLocker>
#include <QUuid>

AuthSession& AuthSession::instance()
{
    static AuthSession session;
    return session;
}

AuthSession::AuthSession(QObject* parent)
    : QObject(parent)
    , m_deviceId(QStringLiteral("qt-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    m_deviceName = QCoreApplication::applicationName().isEmpty()
            ? QStringLiteral("NetherLink Qt")
            : QCoreApplication::applicationName();
}

QString AuthSession::accessToken() const
{
    QMutexLocker locker(&m_mutex);
    return m_accessToken;
}

QString AuthSession::refreshToken() const
{
    QMutexLocker locker(&m_mutex);
    return m_refreshToken;
}

QString AuthSession::deviceId() const
{
    QMutexLocker locker(&m_mutex);
    return m_deviceId;
}

QDateTime AuthSession::expiresAt() const
{
    QMutexLocker locker(&m_mutex);
    return m_expiresAt;
}

bool AuthSession::hasAccessToken() const
{
    QMutexLocker locker(&m_mutex);
    return !m_accessToken.isEmpty();
}

bool AuthSession::hasRefreshToken() const
{
    QMutexLocker locker(&m_mutex);
    return !m_refreshToken.isEmpty();
}

void AuthSession::setDevice(const QString& deviceId, const QString& deviceName)
{
    {
        QMutexLocker locker(&m_mutex);
        if (!deviceId.trimmed().isEmpty()) {
            m_deviceId = deviceId.trimmed();
        }
        if (!deviceName.trimmed().isEmpty()) {
            m_deviceName = deviceName.trimmed();
        }
    }
    emit tokensChanged();
}

QString AuthSession::deviceName() const
{
    QMutexLocker locker(&m_mutex);
    return m_deviceName;
}

void AuthSession::setTokens(const QString& accessToken,
                            const QString& refreshToken,
                            int expiresInSeconds)
{
    {
        QMutexLocker locker(&m_mutex);
        m_accessToken = accessToken;
        m_refreshToken = refreshToken;
        m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(qMax(0, expiresInSeconds));
    }
    emit tokensChanged();
}

void AuthSession::updateAccessToken(const QString& accessToken, int expiresInSeconds)
{
    {
        QMutexLocker locker(&m_mutex);
        m_accessToken = accessToken;
        m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(qMax(0, expiresInSeconds));
    }
    emit tokensChanged();
}

void AuthSession::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_accessToken.clear();
        m_refreshToken.clear();
        m_expiresAt = {};
    }
    emit sessionCleared();
}
