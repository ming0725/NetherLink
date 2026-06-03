#pragma once

#include <QJsonObject>
#include <QString>

struct CurrentUserPreferences {
    QString themeColor;
    QString fontMode;
    QJsonObject inputEffects;
    QJsonObject settings;
    int version = 0;
    QString etag;

    bool isValid() const
    {
        return !themeColor.isEmpty() ||
               !fontMode.isEmpty() ||
               !inputEffects.isEmpty() ||
               !settings.isEmpty() ||
               version > 0 ||
               !etag.isEmpty();
    }
};
