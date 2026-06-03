#include "CurrentUserRemoteDataSource.h"

#include "CurrentUserPreferencesRepository.h"
#include "CurrentUserProfileRepository.h"
#include "shared/network/HttpClient.h"

#include <QJsonObject>
#include <QJsonValue>

namespace {

QJsonObject profilePatchBody(const CurrentUserProfile& profile)
{
    QJsonObject body{
            {QStringLiteral("nickName"), profile.nickName},
            {QStringLiteral("signature"), profile.signature.isEmpty()
                                               ? QJsonValue(QJsonValue::Null)
                                               : QJsonValue(profile.signature)},
            {QStringLiteral("region"), profile.region.isEmpty()
                                          ? QJsonValue(QJsonValue::Null)
                                          : QJsonValue(profile.region)}
    };
    if (profile.version > 0) {
        body.insert(QStringLiteral("expectedVersion"), profile.version);
    }
    return body;
}

QJsonObject preferencesPatchBody(const CurrentUserPreferences& preferences)
{
    QJsonObject body{
            {QStringLiteral("themeColor"), preferences.themeColor.isEmpty()
                                                ? QJsonValue(QJsonValue::Null)
                                                : QJsonValue(preferences.themeColor)},
            {QStringLiteral("fontMode"), preferences.fontMode.isEmpty()
                                            ? QJsonValue(QJsonValue::Null)
                                            : QJsonValue(preferences.fontMode)},
            {QStringLiteral("inputEffects"), preferences.inputEffects},
            {QStringLiteral("settings"), preferences.settings}
    };
    if (preferences.version > 0) {
        body.insert(QStringLiteral("expectedVersion"), preferences.version);
    }
    return body;
}

CurrentUserProfile profileFromResponseObject(QJsonObject object, const QString& responseEtag)
{
    if (object.value(QStringLiteral("user")).isObject()) {
        object = object.value(QStringLiteral("user")).toObject();
    }
    object.insert(QStringLiteral("avatarPath"),
                  object.value(QStringLiteral("avatarPath")).toString(
                          object.value(QStringLiteral("avatarUrl")).toString()));
    if (!responseEtag.isEmpty() && object.value(QStringLiteral("etag")).toString().isEmpty()) {
        object.insert(QStringLiteral("etag"), responseEtag);
    }

    CurrentUserProfileRepository::instance().saveCurrentUserProfileObject(object);
    return CurrentUserProfileRepository::instance().requestCurrentUserProfile({
            object.value(QStringLiteral("userId")).toString(
                    object.value(QStringLiteral("userUuid")).toString())});
}

CurrentUserPreferences preferencesFromResponseObject(const QJsonObject& object, const QString& responseEtag)
{
    CurrentUserPreferencesRepository::instance().saveCurrentUserPreferencesObject(object, responseEtag);
    return CurrentUserPreferencesRepository::instance().currentPreferences();
}

NetworkRequest conditionalRequest(HttpMethod method,
                                  const QString& path,
                                  const QJsonObject& body,
                                  const QString& etag)
{
    NetworkRequest request = NetworkRequest::json(method, path, body);
    request.maxRetries = method == HttpMethod::Get ? 3 : 0;
    if (!etag.isEmpty()) {
        request.headers.insert("If-Match", etag.toUtf8());
    }
    return request;
}

} // namespace

CurrentUserRemoteDataSource& CurrentUserRemoteDataSource::instance()
{
    static CurrentUserRemoteDataSource dataSource;
    return dataSource;
}

CurrentUserRemoteDataSource::CurrentUserRemoteDataSource(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<CurrentUserProfile>("CurrentUserProfile");
    qRegisterMetaType<CurrentUserPreferences>("CurrentUserPreferences");

    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            &CurrentUserRemoteDataSource::handleRequestSucceeded);
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            &CurrentUserRemoteDataSource::handleRequestFailed);
}

QString CurrentUserRemoteDataSource::fetchProfile()
{
    NetworkRequest request = NetworkRequest::json(HttpMethod::Get, QStringLiteral("/me"));
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingRequests.insert(requestId, RequestKind::FetchProfile);
    return requestId;
}

QString CurrentUserRemoteDataSource::updateProfile(const CurrentUserProfile& profile)
{
    const QString requestId = HttpClient::instance().send(conditionalRequest(HttpMethod::Patch,
                                                                            QStringLiteral("/me"),
                                                                            profilePatchBody(profile),
                                                                            profile.etag));
    m_pendingRequests.insert(requestId, RequestKind::UpdateProfile);
    return requestId;
}

QString CurrentUserRemoteDataSource::fetchPreferences()
{
    NetworkRequest request = NetworkRequest::json(HttpMethod::Get, QStringLiteral("/me/preferences"));
    request.maxRetries = 3;
    const QString requestId = HttpClient::instance().send(request);
    m_pendingRequests.insert(requestId, RequestKind::FetchPreferences);
    return requestId;
}

QString CurrentUserRemoteDataSource::updatePreferences(const CurrentUserPreferences& preferences)
{
    const QString requestId = HttpClient::instance().send(conditionalRequest(HttpMethod::Patch,
                                                                            QStringLiteral("/me/preferences"),
                                                                            preferencesPatchBody(preferences),
                                                                            preferences.etag));
    m_pendingRequests.insert(requestId, RequestKind::UpdatePreferences);
    return requestId;
}

void CurrentUserRemoteDataSource::handleRequestSucceeded(const QString& requestId, const NetworkResponse& response)
{
    if (!m_pendingRequests.contains(requestId)) {
        return;
    }

    const RequestKind kind = m_pendingRequests.take(requestId);
    switch (kind) {
    case RequestKind::FetchProfile: {
        const CurrentUserProfile profile = profileFromResponseObject(response.object(), response.etag);
        emit profileFetched(requestId, profile);
        break;
    }
    case RequestKind::UpdateProfile: {
        const CurrentUserProfile profile = profileFromResponseObject(response.object(), response.etag);
        emit profileUpdated(requestId, profile);
        break;
    }
    case RequestKind::FetchPreferences: {
        const CurrentUserPreferences preferences = preferencesFromResponseObject(response.object(), response.etag);
        emit preferencesFetched(requestId, preferences);
        break;
    }
    case RequestKind::UpdatePreferences: {
        const CurrentUserPreferences preferences = preferencesFromResponseObject(response.object(), response.etag);
        emit preferencesUpdated(requestId, preferences);
        break;
    }
    }
}

void CurrentUserRemoteDataSource::handleRequestFailed(const QString& requestId, const NetworkError& error)
{
    if (!m_pendingRequests.contains(requestId)) {
        return;
    }

    const RequestKind kind = m_pendingRequests.take(requestId);
    switch (kind) {
    case RequestKind::FetchProfile:
        emit profileFetchFailed(requestId, error);
        break;
    case RequestKind::UpdateProfile:
        emit profileUpdateFailed(requestId, error);
        break;
    case RequestKind::FetchPreferences:
        emit preferencesFetchFailed(requestId, error);
        break;
    case RequestKind::UpdatePreferences:
        emit preferencesUpdateFailed(requestId, error);
        break;
    }
}
