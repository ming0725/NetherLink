#pragma once

#include "shared/types/Group.h"
#include "shared/types/GroupBot.h"

#include <QPointer>
#include <QWidget>

#include <functional>

class InWindowPopupOverlay;
class InlineEditableText;
class ProfileAvatarPreview;
class QCheckBox;
class QImage;
class QPlainTextEdit;
class QSpinBox;
class StatefulPushButton;
struct NetworkError;
struct NetworkResponse;

class GroupBotCreatePopupContent final : public QWidget
{
public:
    explicit GroupBotCreatePopupContent(const Group& group, QWidget* parent = nullptr);

    std::function<void(const GroupBotCreateRequest&)> createRequested;
    std::function<void()> cancelRequested;

private:
    void applyTheme();
    void openAvatarCrop(const QImage& image);
    void startAvatarUpload(const QString& path);
    void handleAvatarUploadSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleAvatarUploadFailed(const QString& requestId, const NetworkError& error);
    void requestAvatarStatus();
    void handleAvatarStatusSucceeded(const QString& requestId, const NetworkResponse& response);
    void handleAvatarStatusFailed(const QString& requestId, const NetworkError& error);
    void updateCreateButtonState();
    QStringList selectedTools() const;
    QStringList allowedDomains() const;
    void submit();

    Group m_group;
    QString m_avatarLocalPath;
    QString m_avatarUploadedFileId;
    QString m_avatarFileId;
    QString m_avatarUploadRequestId;
    QString m_avatarStatusRequestId;
    int m_avatarStatusPollAttempts = 0;
    bool m_avatarUploadInProgress = false;
    bool m_avatarProcessingInProgress = false;
    ProfileAvatarPreview* m_avatarPreview = nullptr;
    InlineEditableText* m_nameEdit = nullptr;
    InlineEditableText* m_modelEdit = nullptr;
    InlineEditableText* m_domainsEdit = nullptr;
    QPlainTextEdit* m_promptEdit = nullptr;
    QCheckBox* m_localTimeCheck = nullptr;
    QCheckBox* m_calculateCheck = nullptr;
    QCheckBox* m_calendarCheck = nullptr;
    QCheckBox* m_webSearchCheck = nullptr;
    QCheckBox* m_webReadCheck = nullptr;
    QCheckBox* m_confirmSideEffectsCheck = nullptr;
    QSpinBox* m_cooldownSpin = nullptr;
    StatefulPushButton* m_cancelButton = nullptr;
    StatefulPushButton* m_createButton = nullptr;
    QPointer<InWindowPopupOverlay> m_avatarCropPopup;
};
