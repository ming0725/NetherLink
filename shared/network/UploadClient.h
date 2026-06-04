#pragma once

#include "NetworkTypes.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class UploadClient : public QObject
{
    Q_OBJECT

public:
    static UploadClient& instance();

    BackendEnvironment environment() const;
    void setEnvironment(const BackendEnvironment& environment);

    QString uploadFile(const QString& path,
                       const QString& targetType,
                       const QString& contentHash = {},
                       const QVariantMap& query = {});
    QString uploadAvatar(const QString& path,
                         int expectedVersion = 0,
                         const QString& contentHash = {});

signals:
    void uploadStarted(const QString& requestId, const QString& path);
    void uploadProgress(const QString& requestId, qint64 sent, qint64 total);
    void uploadSucceeded(const QString& requestId, const NetworkResponse& response);
    void uploadFailed(const QString& requestId, const NetworkError& error);

private:
    explicit UploadClient(QObject* parent = nullptr);
    Q_DISABLE_COPY(UploadClient)

    QNetworkAccessManager* m_manager = nullptr;
    BackendEnvironment m_environment;
};
