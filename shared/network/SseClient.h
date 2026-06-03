#pragma once

#include "NetworkTypes.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class SseClient : public QObject
{
    Q_OBJECT

public:
    explicit SseClient(QObject* parent = nullptr);

    void setEnvironment(const BackendEnvironment& environment);
    QString start(const NetworkRequest& request);
    bool isRunning() const;

public slots:
    void cancel();

signals:
    void streamStarted(const QString& requestId);
    void eventReceived(const QString& requestId, const QString& eventName, const QJsonObject& data);
    void streamFinished(const QString& requestId);
    void streamFailed(const QString& requestId, const NetworkError& error);

private:
    void parseBufferedEvents();
    void emitEventBlock(const QByteArray& block);

    QNetworkAccessManager* m_manager = nullptr;
    QNetworkReply* m_reply = nullptr;
    BackendEnvironment m_environment;
    QString m_requestId;
    QByteArray m_buffer;
};
