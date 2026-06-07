#include "NetworkLog.h"

#include "AuthSession.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrlQuery>

#include <algorithm>
#include <cstdio>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace {

QFile* s_logFile = nullptr;
QtMessageHandler s_previousMessageHandler = nullptr;

QString placeholder(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("<empty>") : value;
}

QString shortId(const QString& requestId)
{
    return requestId.size() <= 8 ? placeholder(requestId) : requestId.left(8);
}

bool isSensitiveKey(const QString& key)
{
    const QString normalized = key.toLower();
    return normalized.contains(QStringLiteral("token")) ||
           normalized.contains(QStringLiteral("password")) ||
           normalized.contains(QStringLiteral("secret")) ||
           normalized == QStringLiteral("authorization") ||
           normalized == QStringLiteral("cookie");
}

QString redactedValue(const QString& key, const QString& value)
{
    if (!isSensitiveKey(key)) {
        return placeholder(value);
    }
    if (key.compare(QStringLiteral("authorization"), Qt::CaseInsensitive) == 0 &&
        value.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) {
        return QStringLiteral("Bearer <redacted>");
    }
    return value.isEmpty() ? QStringLiteral("<empty>") : QStringLiteral("<redacted>");
}

QString methodName(HttpMethod method)
{
    switch (method) {
    case HttpMethod::Get:
        return QStringLiteral("GET");
    case HttpMethod::Post:
        return QStringLiteral("POST");
    case HttpMethod::Patch:
        return QStringLiteral("PATCH");
    case HttpMethod::Delete:
        return QStringLiteral("DELETE");
    }
    return QStringLiteral("GET");
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString scalarValueText(const QString& key, const QJsonValue& value)
{
    if (isSensitiveKey(key)) {
        return value.isNull() || value.isUndefined() ? QStringLiteral("<empty>") : QStringLiteral("<redacted>");
    }

    if (value.isString()) {
        return placeholder(value.toString());
    }
    if (value.isBool()) {
        return boolText(value.toBool());
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        const qint64 integer = static_cast<qint64>(number);
        return qAbs(number - static_cast<double>(integer)) < 0.000001
                ? QString::number(integer)
                : QString::number(number, 'g', 12);
    }
    if (value.isNull() || value.isUndefined()) {
        return QStringLiteral("<null>");
    }
    return QString();
}

void appendJsonValue(QStringList& lines,
                     const QString& key,
                     const QJsonValue& value,
                     int indent,
                     int depth);

void appendJsonObject(QStringList& lines, const QJsonObject& object, int indent, int depth)
{
    if (object.isEmpty()) {
        lines.append(QString(indent, QLatin1Char(' ')) + QStringLiteral("<empty>"));
        return;
    }

    const QStringList keys = object.keys();
    for (const QString& key : keys) {
        appendJsonValue(lines, key, object.value(key), indent, depth);
    }
}

void appendJsonArray(QStringList& lines, const QJsonArray& array, int indent, int depth)
{
    if (array.isEmpty()) {
        lines.append(QString(indent, QLatin1Char(' ')) + QStringLiteral("<empty array>"));
        return;
    }

    const int visibleItems = qMin(array.size(), 8);
    for (int i = 0; i < visibleItems; ++i) {
        appendJsonValue(lines, QStringLiteral("[%1]").arg(i), array.at(i), indent, depth);
    }
    if (array.size() > visibleItems) {
        lines.append(QString(indent, QLatin1Char(' ')) +
                     QStringLiteral("... %1 more item(s)").arg(array.size() - visibleItems));
    }
}

void appendJsonValue(QStringList& lines,
                     const QString& key,
                     const QJsonValue& value,
                     int indent,
                     int depth)
{
    const QString prefix = QString(indent, QLatin1Char(' ')) + key + QStringLiteral(": ");
    const QString scalar = scalarValueText(key, value);
    if (!scalar.isEmpty()) {
        lines.append(prefix + scalar);
        return;
    }

    if (depth >= 4) {
        lines.append(prefix + QStringLiteral("<nested value>"));
        return;
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        lines.append(prefix + QStringLiteral("{%1 field(s)}").arg(object.size()));
        appendJsonObject(lines, object, indent + 2, depth + 1);
        return;
    }

    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        lines.append(prefix + QStringLiteral("[%1 item(s)]").arg(array.size()));
        appendJsonArray(lines, array, indent + 2, depth + 1);
        return;
    }

    lines.append(prefix + QStringLiteral("<unsupported>"));
}

void appendJsonDocument(QStringList& lines,
                        const QString& title,
                        const QJsonDocument& document,
                        const QByteArray& rawBody = {})
{
    lines.append(QStringLiteral("  %1:").arg(title));
    if (document.isObject()) {
        appendJsonObject(lines, document.object(), 4, 0);
        return;
    }
    if (document.isArray()) {
        appendJsonArray(lines, document.array(), 4, 0);
        return;
    }
    if (!rawBody.trimmed().isEmpty()) {
        const QString text = QString::fromUtf8(rawBody.left(240)).simplified();
        lines.append(QStringLiteral("    text: %1").arg(placeholder(text)));
        lines.append(QStringLiteral("    bytes: %1").arg(rawBody.size()));
        return;
    }
    lines.append(QStringLiteral("    <empty>"));
}

void appendJsonObjectSection(QStringList& lines, const QString& title, const QJsonObject& object)
{
    lines.append(QStringLiteral("  %1:").arg(title));
    appendJsonObject(lines, object, 4, 0);
}

void appendHeaders(QStringList& lines, const NetworkRequest& request)
{
    lines.append(QStringLiteral("  headers:"));
    QHash<QByteArray, QByteArray> headers = request.headers;
    if (!headers.contains("Accept")) {
        headers.insert("Accept", "application/json");
    }
    if (request.hasJsonBody) {
        headers.insert("Content-Type", "application/json");
    } else if (!request.contentType.isEmpty()) {
        headers.insert("Content-Type", request.contentType);
    }
    if (request.requiresAuth && AuthSession::instance().hasAccessToken()) {
        headers.insert("Authorization", "Bearer <redacted>");
    }
    if (headers.isEmpty()) {
        lines.append(QStringLiteral("    <empty>"));
        return;
    }
    QList<QByteArray> keys = headers.keys();
    std::sort(keys.begin(), keys.end());
    for (const QByteArray& keyBytes : keys) {
        const QString key = QString::fromUtf8(keyBytes);
        const QString value = QString::fromUtf8(headers.value(keyBytes));
        lines.append(QStringLiteral("    %1: %2").arg(key, redactedValue(key, value)));
    }
}

void appendQuery(QStringList& lines, const QVariantMap& query)
{
    lines.append(QStringLiteral("  query:"));
    if (query.isEmpty()) {
        lines.append(QStringLiteral("    <empty>"));
        return;
    }
    const QStringList keys = query.keys();
    for (const QString& key : keys) {
        lines.append(QStringLiteral("    %1: %2").arg(key, redactedValue(key, query.value(key).toString())));
    }
}

QString block(const QString& title, const QStringList& fields)
{
    QStringList lines;
    lines.append(title);
    lines.append(fields);
    return lines.join(QLatin1Char('\n'));
}

void writeBlock(QtMsgType type, const QString& title, const QStringList& fields)
{
    const QString message = block(title, fields);
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        qWarning().noquote() << message;
    } else {
        qInfo().noquote() << message;
    }
}

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO ");
    case QtWarningMsg:
        return QStringLiteral("WARN ");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO ");
}

QString formattedMessage(QtMsgType type, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    return QStringLiteral("%1 %2 %3\n").arg(timestamp, levelName(type), message);
}

void appMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString output = formattedMessage(type, message);
    FILE* stream = (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) ? stderr : stdout;
    std::fputs(output.toUtf8().constData(), stream);
    std::fflush(stream);

    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->write(output.toUtf8());
        s_logFile->flush();
    }

    if (s_previousMessageHandler) {
        Q_UNUSED(context);
    }

    if (type == QtFatalMsg) {
        std::abort();
    }
}

QString escapedAppleScriptString(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return value;
}

bool hasAttachedTerminal()
{
#if defined(Q_OS_WIN)
    return GetConsoleWindow() != nullptr;
#elif defined(Q_OS_UNIX)
    return isatty(STDERR_FILENO) || isatty(STDOUT_FILENO);
#else
    return true;
#endif
}

bool shouldOpenLogTerminal()
{
    if (qEnvironmentVariableIsSet("QTC_RUN") ||
        qEnvironmentVariableIsSet("QTCREATOR") ||
        qEnvironmentVariableIsSet("QT_CREATOR_PID")) {
        return false;
    }
    if (qEnvironmentVariableIsSet("NETHERLINK_LOG_TERMINAL")) {
        return qEnvironmentVariableIntValue("NETHERLINK_LOG_TERMINAL") != 0;
    }
    return !hasAttachedTerminal();
}

void openLogTerminal(const QString& path)
{
    if (!shouldOpenLogTerminal()) {
        return;
    }

#if defined(Q_OS_WIN)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    FILE* ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
    Q_UNUSED(path);
#elif defined(Q_OS_MACOS)
    const QString script = QStringLiteral("tell application \"Terminal\" to do script \"tail -f \" & quoted form of \"%1\"")
                                   .arg(escapedAppleScriptString(path));
    QProcess::startDetached(QStringLiteral("/usr/bin/osascript"),
                            QStringList{QStringLiteral("-e"), script});
#elif defined(Q_OS_LINUX)
    const QString command = QStringLiteral("tail -f '%1'").arg(QString(path).replace(QLatin1Char('\''), QStringLiteral("'\\''")));
    if (QProcess::startDetached(QStringLiteral("x-terminal-emulator"), QStringList{QStringLiteral("-e"), QStringLiteral("sh"), QStringLiteral("-lc"), command})) {
        return;
    }
    if (QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList{QStringLiteral("--"), QStringLiteral("sh"), QStringLiteral("-lc"), command})) {
        return;
    }
    QProcess::startDetached(QStringLiteral("xterm"), QStringList{QStringLiteral("-e"), command});
#else
    Q_UNUSED(path);
#endif
}

QString eventName(const QString& eventName)
{
    return eventName.isEmpty() ? QStringLiteral("<message>") : eventName;
}

} // namespace

namespace NetworkLog {

void installApplicationMessageHandler()
{
    if (!s_logFile) {
        const QString path = logFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        s_logFile = new QFile(path);
        if (!s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            delete s_logFile;
            s_logFile = nullptr;
        }
        openLogTerminal(path);
    }
    s_previousMessageHandler = qInstallMessageHandler(appMessageHandler);
    qInfo().noquote() << QStringLiteral("NetherLink log file: %1").arg(logFilePath());
}

QString logFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::tempPath() + QStringLiteral("/NetherLink");
    }
    return QDir(dir).filePath(QStringLiteral("NetherLink.log"));
}

QString redactedUrl(QUrl url)
{
    QUrlQuery query(url);
    const QStringList sensitiveKeys = {QStringLiteral("accessToken"),
                                       QStringLiteral("refreshToken"),
                                       QStringLiteral("token"),
                                       QStringLiteral("password")};
    for (const QString& key : sensitiveKeys) {
        if (query.hasQueryItem(key)) {
            query.removeAllQueryItems(key);
            query.addQueryItem(key, QStringLiteral("<redacted>"));
        }
    }
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

void httpRequest(const QString& requestId,
                 const NetworkRequest& request,
                 const QUrl& url,
                 int attempt)
{
    QStringList lines;
    lines.append(QStringLiteral("  method: %1").arg(methodName(request.method)));
    lines.append(QStringLiteral("  url: %1").arg(redactedUrl(url)));
    lines.append(QStringLiteral("  attempt: %1/%2").arg(attempt).arg(qMax(1, request.maxRetries + 1)));
    lines.append(QStringLiteral("  expectsJson: %1").arg(boolText(request.expectsJson)));
    appendQuery(lines, request.query);
    appendHeaders(lines, request);
    appendJsonDocument(lines, QStringLiteral("payload"), request.body, request.rawBody);
    writeBlock(QtInfoMsg, QStringLiteral("[HTTP] -> request %1").arg(shortId(requestId)), lines);
}

void httpResponse(const QString& requestId,
                  const NetworkRequest& request,
                  const NetworkResponse& response,
                  qint64 elapsedMs)
{
    Q_UNUSED(request);
    QStringList lines;
    lines.append(QStringLiteral("  status: %1").arg(response.httpStatus));
    lines.append(QStringLiteral("  replyTo: %1").arg(shortId(requestId)));
    lines.append(QStringLiteral("  serverRequestId: %1").arg(placeholder(response.requestId)));
    lines.append(QStringLiteral("  etag: %1").arg(placeholder(response.etag)));
    lines.append(QStringLiteral("  elapsedMs: %1").arg(elapsedMs));
    appendJsonDocument(lines, QStringLiteral("payload"), response.body, response.rawBody);
    writeBlock(QtInfoMsg, QStringLiteral("[HTTP] <- response %1").arg(shortId(requestId)), lines);
}

void httpError(const QString& requestId,
               const NetworkRequest& request,
               const NetworkError& error,
               qint64 elapsedMs)
{
    Q_UNUSED(request);
    QStringList lines;
    lines.append(QStringLiteral("  status: %1").arg(error.httpStatus));
    lines.append(QStringLiteral("  replyTo: %1").arg(shortId(requestId)));
    lines.append(QStringLiteral("  serverRequestId: %1").arg(placeholder(error.requestId)));
    lines.append(QStringLiteral("  code: %1").arg(placeholder(error.code)));
    lines.append(QStringLiteral("  message: %1").arg(placeholder(error.message)));
    lines.append(QStringLiteral("  elapsedMs: %1").arg(elapsedMs));
    appendJsonObjectSection(lines, QStringLiteral("details"), error.details);
    if (!error.rawBody.trimmed().isEmpty() && error.details.isEmpty()) {
        appendJsonDocument(lines, QStringLiteral("payload"), QJsonDocument::fromJson(error.rawBody), error.rawBody);
    }
    writeBlock(QtWarningMsg, QStringLiteral("[HTTP] <- error %1").arg(shortId(requestId)), lines);
}

void httpRetry(const QString& requestId, int nextAttempt, int delayMs)
{
    writeBlock(QtWarningMsg,
               QStringLiteral("[HTTP] retry scheduled %1").arg(shortId(requestId)),
               {QStringLiteral("  nextAttempt: %1").arg(nextAttempt),
                QStringLiteral("  delayMs: %1").arg(delayMs)});
}

void authRefreshQueued(int queuedCount)
{
    writeBlock(QtInfoMsg,
               QStringLiteral("[HTTP] auth refresh queued"),
               {QStringLiteral("  queuedRequests: %1").arg(queuedCount)});
}

void sseRequest(const QString& requestId, const NetworkRequest& request, const QUrl& url)
{
    QStringList lines;
    lines.append(QStringLiteral("  method: POST"));
    lines.append(QStringLiteral("  url: %1").arg(redactedUrl(url)));
    lines.append(QStringLiteral("  replyTo: %1").arg(shortId(requestId)));
    appendHeaders(lines, request);
    appendJsonDocument(lines, QStringLiteral("payload"), request.body, request.rawBody);
    writeBlock(QtInfoMsg, QStringLiteral("[SSE] -> request %1").arg(shortId(requestId)), lines);
}

void sseEvent(const QString& requestId, const QString& name, const QJsonObject& payload)
{
    QStringList lines;
    lines.append(QStringLiteral("  request: %1").arg(shortId(requestId)));
    lines.append(QStringLiteral("  event: %1").arg(eventName(name)));
    appendJsonObjectSection(lines, QStringLiteral("payload"), payload);
    writeBlock(QtInfoMsg, QStringLiteral("[SSE] <- event %1").arg(shortId(requestId)), lines);
}

void sseFinished(const QString& requestId, int httpStatus)
{
    writeBlock(QtInfoMsg,
               QStringLiteral("[SSE] stream finished %1").arg(shortId(requestId)),
               {QStringLiteral("  status: %1").arg(httpStatus),
                QStringLiteral("  request: %1").arg(shortId(requestId))});
}

void sseError(const QString& requestId, const NetworkError& error)
{
    QStringList lines;
    lines.append(QStringLiteral("  request: %1").arg(shortId(requestId)));
    lines.append(QStringLiteral("  status: %1").arg(error.httpStatus));
    lines.append(QStringLiteral("  serverRequestId: %1").arg(placeholder(error.requestId)));
    lines.append(QStringLiteral("  code: %1").arg(placeholder(error.code)));
    lines.append(QStringLiteral("  message: %1").arg(placeholder(error.message)));
    appendJsonObjectSection(lines, QStringLiteral("details"), error.details);
    writeBlock(QtWarningMsg, QStringLiteral("[SSE] stream failed %1").arg(shortId(requestId)), lines);
}

void realtimeState(const QString& state)
{
    writeBlock(QtInfoMsg, QStringLiteral("[Realtime] state changed"), {QStringLiteral("  state: %1").arg(state)});
}

void realtimeOpening(const QUrl& url, const QString& state, int attempt, qint64 lastEventSeq, const QString& lastEventId)
{
    writeBlock(QtInfoMsg,
               QStringLiteral("[Realtime] opening websocket"),
               {QStringLiteral("  state: %1").arg(state),
                QStringLiteral("  attempt: %1").arg(attempt),
                QStringLiteral("  lastEventSeq: %1").arg(lastEventSeq),
                QStringLiteral("  lastEventId: %1").arg(placeholder(lastEventId)),
                QStringLiteral("  url: %1").arg(redactedUrl(url))});
}

void realtimeConnected(const QUrl& url)
{
    writeBlock(QtInfoMsg, QStringLiteral("[Realtime] websocket connected"), {QStringLiteral("  url: %1").arg(redactedUrl(url))});
}

void realtimeClosed(const QString& reason,
                    const QUrl& url,
                    int closeCode,
                    const QString& closeReason,
                    const QString& socketError,
                    int httpStatus)
{
    QStringList lines;
    lines.append(QStringLiteral("  reason: %1").arg(reason));
    lines.append(QStringLiteral("  code: %1").arg(closeCode));
    lines.append(QStringLiteral("  closeReason: %1").arg(placeholder(closeReason)));
    if (httpStatus > 0) {
        lines.append(QStringLiteral("  httpStatus: %1").arg(httpStatus));
    }
    if (!socketError.isEmpty()) {
        lines.append(QStringLiteral("  socketError: %1").arg(socketError));
    }
    lines.append(QStringLiteral("  url: %1").arg(redactedUrl(url)));
    writeBlock(reason == QStringLiteral("client closed") ? QtInfoMsg : QtWarningMsg,
               QStringLiteral("[Realtime] websocket closed"),
               lines);
}

void realtimeError(int socketError, int httpStatus, const QString& message, const QUrl& url)
{
    writeBlock(QtWarningMsg,
               QStringLiteral("[Realtime] websocket error"),
               {QStringLiteral("  socketError: %1").arg(socketError),
                QStringLiteral("  httpStatus: %1").arg(httpStatus),
                QStringLiteral("  message: %1").arg(placeholder(message)),
                QStringLiteral("  url: %1").arg(redactedUrl(url))});
}

void realtimeReconnect(int attempt, int delayMs)
{
    writeBlock(QtWarningMsg,
               QStringLiteral("[Realtime] reconnect scheduled"),
               {QStringLiteral("  attempt: %1").arg(attempt),
                QStringLiteral("  delayMs: %1").arg(delayMs)});
}

void realtimeSend(const QJsonObject& message, const QUrl& url)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("ping")) {
        return;
    }
    QStringList lines;
    lines.append(QStringLiteral("  type: %1").arg(placeholder(type)));
    lines.append(QStringLiteral("  url: %1").arg(redactedUrl(url)));
    appendJsonObjectSection(lines, QStringLiteral("payload"), message.value(QStringLiteral("payload")).toObject());
    writeBlock(QtInfoMsg, QStringLiteral("[Realtime] -> message"), lines);
}

void realtimeSendSkipped(const QJsonObject& message, int socketState, const QUrl& url)
{
    writeBlock(QtWarningMsg,
               QStringLiteral("[Realtime] send skipped"),
               {QStringLiteral("  type: %1").arg(placeholder(message.value(QStringLiteral("type")).toString())),
                QStringLiteral("  socketState: %1").arg(socketState),
                QStringLiteral("  url: %1").arg(redactedUrl(url))});
}

void realtimeInvalidJson(const QString& error, qsizetype messageSize)
{
    writeBlock(QtWarningMsg,
               QStringLiteral("[Realtime] invalid JSON"),
               {QStringLiteral("  error: %1").arg(error),
                QStringLiteral("  messageSize: %1").arg(messageSize)});
}

void realtimeEvent(const RealtimeEvent& event)
{
    if (event.type == QStringLiteral("realtime.pong")) {
        return;
    }

    QStringList lines;
    lines.append(QStringLiteral("  type: %1").arg(placeholder(event.type)));
    lines.append(QStringLiteral("  eventSeq: %1").arg(event.eventSeq));
    lines.append(QStringLiteral("  eventId: %1").arg(placeholder(event.eventId)));
    lines.append(QStringLiteral("  emittedAt: %1").arg(event.emittedAt.isValid()
                                                       ? event.emittedAt.toString(Qt::ISODateWithMs)
                                                       : QStringLiteral("<empty>")));
    appendJsonObjectSection(lines, QStringLiteral("payload"), event.payload);
    writeBlock(event.type.isEmpty() ? QtWarningMsg : QtInfoMsg,
               QStringLiteral("[Realtime] <- event"),
               lines);
}

void realtimeClientError(const QString& code, const QString& message)
{
    writeBlock(QtWarningMsg,
               QStringLiteral("[Realtime] client.error"),
               {QStringLiteral("  code: %1").arg(placeholder(code)),
                QStringLiteral("  message: %1").arg(placeholder(message))});
}

} // namespace NetworkLog
