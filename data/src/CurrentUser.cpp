#include "CurrentUser.h"
#include <QDebug>

CurrentUser::CurrentUser(QObject* parent)
    : QObject(parent)
{
    qDebug() << "CurrentUser initialized";
}

CurrentUser& CurrentUser::instance()
{
    static CurrentUser instance;
    return instance;
}

void CurrentUser::setUserInfo(const QString& userId, const QString& token, const QString& userName, const QString& avatarPath)
{
    bool changed = (m_userId != userId);

    m_userId = userId;
    m_token = token;

    if (!userName.isEmpty()) {
        m_userName = userName;
    }

    if (!avatarPath.isEmpty()) {
        m_avatarPath = avatarPath;
    }

    if (changed) {
        qDebug() << "CurrentUser changed to:" << m_userId;
        emit userChanged(m_userId);
    }
}

void CurrentUser::clear()
{
    QString oldId = m_userId;
    m_userId = "";
    m_userName = "";
    m_avatarPath = "";
    m_token = "";

    if (!oldId.isEmpty()) {
        qDebug() << "CurrentUser cleared";
        emit userChanged("");
    }
}
