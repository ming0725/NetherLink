#pragma once

#include <QObject>
#include <QString>

class AiChatTitleClient : public QObject
{
    Q_OBJECT

public:
    explicit AiChatTitleClient(QObject* parent = nullptr);

    QString generateTitle(const QString& firstUserMessage) const;

private:
    static QString fallbackTitle(const QString& firstUserMessage);
};
