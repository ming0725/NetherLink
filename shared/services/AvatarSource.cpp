#include "AvatarSource.h"

#include <QUrl>

namespace {

bool isRemoteLike(const QString& source)
{
    if (source.startsWith(QLatin1Char('/'))) {
        return true;
    }

    const QUrl url(source);
    return url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https");
}

QString cacheToken(int version, const QString& etag, const QString& contentHash)
{
    if (!contentHash.isEmpty()) {
        return QStringLiteral("hash:%1").arg(contentHash);
    }
    if (version > 0) {
        return QStringLiteral("v:%1").arg(version);
    }
    if (!etag.isEmpty()) {
        return QStringLiteral("etag:%1").arg(etag);
    }
    return {};
}

} // namespace

namespace AvatarSource {

QString fromAvatarFileId(const QString& fileId)
{
    const QString trimmed = fileId.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QString source = QStringLiteral("/api/v1/avatar-files/%1?variant=thumb")
            .arg(QString::fromLatin1(QUrl::toPercentEncoding(trimmed)));
    return source;
}

QString versioned(const QString& source, int version, const QString& etag, const QString& contentHash)
{
    if (source.isEmpty() || !isRemoteLike(source)) {
        return source;
    }

    const QString token = cacheToken(version, etag, contentHash);
    if (token.isEmpty()) {
        return source;
    }

    QUrl url(source);
    url.setFragment(QStringLiteral("nl-cache=%1").arg(token));
    return url.toString();
}

QString cleanForIo(const QString& source)
{
    if (source.isEmpty()) {
        return {};
    }

    QUrl url(source);
    if (url.isValid() && !url.fragment().isEmpty()) {
        url.setFragment(QString());
        return url.toString();
    }
    return source;
}

} // namespace AvatarSource
