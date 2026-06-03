#pragma once

#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QString>

class AuthSession : public QObject
{
    Q_OBJECT

public:
    static AuthSession& instance();

    QString accessToken() const;
    QString refreshToken() const;
    QString deviceId() const;
    QDateTime expiresAt() const;
    bool hasAccessToken() const;
    bool hasRefreshToken() const;

    void setDevice(const QString& deviceId, const QString& deviceName = {});
    QString deviceName() const;

public slots:
    void setTokens(const QString& accessToken,
                   const QString& refreshToken,
                   int expiresInSeconds);
    void updateAccessToken(const QString& accessToken, int expiresInSeconds);
    void clear();

signals:
    void tokensChanged();
    void sessionCleared();

private:
    explicit AuthSession(QObject* parent = nullptr);
    Q_DISABLE_COPY(AuthSession)

    mutable QMutex m_mutex;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_deviceId;
    QString m_deviceName;
    QDateTime m_expiresAt;
};
