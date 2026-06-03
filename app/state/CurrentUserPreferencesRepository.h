#pragma once

#include "CurrentUserPreferences.h"

#include <QMutex>
#include <QObject>
#include <QJsonObject>

class CurrentUserPreferencesRepository final : public QObject
{
    Q_OBJECT

public:
    static CurrentUserPreferencesRepository& instance();

    CurrentUserPreferences currentPreferences() const;
    void saveCurrentUserPreferences(const CurrentUserPreferences& preferences);
    void saveCurrentUserPreferencesObject(const QJsonObject& object, const QString& responseEtag = {});

signals:
    void currentUserPreferencesChanged();

private:
    explicit CurrentUserPreferencesRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(CurrentUserPreferencesRepository)

    CurrentUserPreferences m_preferences;
    mutable QMutex m_mutex;
};
