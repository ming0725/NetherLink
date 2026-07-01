#include "features/chat/ui/GroupBotCreatePopupContent.h"

#include "app/state/CurrentUserPopupStyle.h"
#include "app/state/ProfileAvatarPreview.h"
#include "shared/network/HttpClient.h"
#include "shared/network/UploadClient.h"
#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/avatar/AvatarCropPopupContent.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

using namespace CurrentUserPopupStyle;

namespace {

constexpr int kBotEditLabelWidth = 76;
constexpr int kBotEditInputHeight = 36;
constexpr int kAvatarStatusPollDelayMs = 600;
constexpr int kAvatarStatusMaxPollAttempts = 30;

QString saveAvatarImageToAppData(const QImage& image, const QString& groupId)
{
    if (image.isNull()) {
        return {};
    }

    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath() + QStringLiteral("/NetherLink");
    }

    const QString avatarDirPath = root + QStringLiteral("/bot-avatars");
    QDir avatarDir;
    if (!avatarDir.mkpath(avatarDirPath)) {
        return {};
    }

    const QString fileName = QStringLiteral("%1_%2.png")
            .arg(groupId.isEmpty() ? QStringLiteral("group_bot") : groupId,
                 QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString filePath = QDir(avatarDirPath).filePath(fileName);
    return image.save(filePath, "PNG") ? filePath : QString();
}

QWidget* createInputRow(QWidget* parent, const QString& title, InlineEditableText* edit)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* label = makeLabel(title, ThemeColor::SecondaryText, 13, row);
    label->setFixedWidth(kBotEditLabelWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    edit->setParent(row);
    edit->setMinimumHeight(kBotEditInputHeight);
    applyEditStyle(edit);

    layout->addWidget(label);
    layout->addWidget(edit, 1);
    return row;
}

QWidget* createPromptRow(QWidget* parent, QPlainTextEdit* edit)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* label = makeLabel(QStringLiteral("设定"), ThemeColor::SecondaryText, 13, row);
    label->setFixedWidth(kBotEditLabelWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setContentsMargins(0, 8, 0, 0);

    edit->setParent(row);
    edit->setMinimumHeight(110);
    layout->addWidget(label);
    layout->addWidget(edit, 1);
    return row;
}

QString cssColor(const QColor& color)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha());
}

void applyPlainTextEditStyle(QPlainTextEdit* edit)
{
    if (!edit) {
        return;
    }

    const QColor inputBackground = ThemeManager::instance().color(ThemeColor::InputBackground);
    const QColor text = ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor placeholder = ThemeManager::instance().color(ThemeColor::PlaceholderText);
    const QColor divider = ThemeManager::instance().color(ThemeColor::Divider);
    const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
    const QColor selection = ThemeManager::instance().color(ThemeColor::AccentTextSelection);
    const QColor selectedText = ThemeManager::textColorOn(
            ThemeManager::colorCompositedOver(selection, inputBackground));

    QPalette palette = edit->palette();
    palette.setColor(QPalette::Base, inputBackground);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::PlaceholderText, placeholder);
    palette.setColor(QPalette::Highlight, selection);
    palette.setColor(QPalette::HighlightedText, selectedText);
    edit->setPalette(palette);
    edit->setFont(AppFonts::applicationPixelSizedFont(14, false));
    edit->setFrameShape(QFrame::NoFrame);
    edit->setStyleSheet(QStringLiteral(
            "QPlainTextEdit {"
            "border: 1px solid %1;"
            "border-radius: 8px;"
            "padding: 8px 10px;"
            "background: %2;"
            "color: %3;"
            "}"
            "QPlainTextEdit:focus {"
            "border: 1px solid %4;"
            "}")
            .arg(cssColor(divider), cssColor(inputBackground), cssColor(text), cssColor(accent)));
}

void applyCheckBoxPalette(QCheckBox* checkBox)
{
    if (!checkBox) {
        return;
    }
    QPalette palette = checkBox->palette();
    palette.setColor(QPalette::WindowText, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::ButtonText, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
    checkBox->setPalette(palette);
    checkBox->setFont(AppFonts::applicationPixelSizedFont(13, false));
}

void applySpinBoxStyle(QSpinBox* spinBox)
{
    if (!spinBox) {
        return;
    }
    QPalette palette = spinBox->palette();
    palette.setColor(QPalette::Base, ThemeManager::instance().color(ThemeColor::InputBackground));
    palette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::ButtonText, ThemeManager::instance().color(ThemeColor::PrimaryText));
    palette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::AccentTextSelection));
    spinBox->setPalette(palette);
    spinBox->setFont(AppFonts::applicationPixelSizedFont(13, false));
}

QCheckBox* createToolCheckBox(const QString& text, QWidget* parent)
{
    auto* checkBox = new QCheckBox(text, parent);
    checkBox->setChecked(true);
    applyCheckBoxPalette(checkBox);
    return checkBox;
}

QStringList splitDomains(const QString& text)
{
    QStringList result;
    QSet<QString> seen;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                         Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString domain = part.trimmed().toLower();
        if (domain.isEmpty() || seen.contains(domain)) {
            continue;
        }
        result.push_back(domain);
        seen.insert(domain);
    }
    return result;
}

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QJsonObject fileObjectFromResponse(const NetworkResponse& response)
{
    QJsonObject object = response.object();
    const QJsonObject nestedFile = object.value(QStringLiteral("file")).toObject();
    return nestedFile.isEmpty() ? object : nestedFile;
}

QString avatarFileIdFromUploadResponse(const NetworkResponse& response)
{
    const QJsonObject object = fileObjectFromResponse(response);
    return firstString(object, {QStringLiteral("fileId"),
                                QStringLiteral("id"),
                                QStringLiteral("avatarFileId")});
}

QString processingStatusFromFileResponse(const NetworkResponse& response)
{
    return firstString(fileObjectFromResponse(response),
                       {QStringLiteral("processingStatus"),
                        QStringLiteral("storageStatus"),
                        QStringLiteral("status")}).trimmed().toLower();
}

bool isTerminalAvatarStatus(const QString& status)
{
    return status == QStringLiteral("failed") ||
           status == QStringLiteral("aborted") ||
           status == QStringLiteral("deleted") ||
           status == QStringLiteral("cleanup_pending");
}

} // namespace

GroupBotCreatePopupContent::GroupBotCreatePopupContent(const Group& group, QWidget* parent)
    : QWidget(parent)
    , m_group(group)
    , m_avatarPreview(new ProfileAvatarPreview(this))
    , m_nameEdit(new InlineEditableText(this))
    , m_modelEdit(new InlineEditableText(this))
    , m_domainsEdit(new InlineEditableText(this))
    , m_promptEdit(new QPlainTextEdit(this))
    , m_localTimeCheck(createToolCheckBox(QStringLiteral("local_time"), this))
    , m_calculateCheck(createToolCheckBox(QStringLiteral("calculate"), this))
    , m_calendarCheck(createToolCheckBox(QStringLiteral("calendar"), this))
    , m_webSearchCheck(createToolCheckBox(QStringLiteral("web_search"), this))
    , m_webReadCheck(createToolCheckBox(QStringLiteral("web_read"), this))
    , m_confirmSideEffectsCheck(new QCheckBox(QStringLiteral("操作需确认"), this))
    , m_cooldownSpin(new QSpinBox(this))
    , m_cancelButton(new StatefulPushButton(QStringLiteral("取消"), this))
    , m_createButton(new StatefulPushButton(QStringLiteral("创建"), this))
{
    setMinimumSize(460, 580);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(12);

    auto* title = makeLabel(QStringLiteral("创建机器人"), ThemeColor::PrimaryText, 20, this);
    title->setFont(AppFonts::applicationPixelSizedFont(20, true));
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    layout->addWidget(title);

    m_avatarPreview->avatarImageSelected = [this](const QImage& image) {
        openAvatarCrop(image);
    };
    layout->addWidget(m_avatarPreview, 0, Qt::AlignHCenter);

    m_nameEdit->setPlaceholderText(QStringLiteral("名称"));
    m_modelEdit->setPlaceholderText(QStringLiteral("默认模型"));
    m_domainsEdit->setPlaceholderText(QStringLiteral("允许域名"));
    m_promptEdit->setPlaceholderText(QStringLiteral("机器人设定"));
    layout->addWidget(createInputRow(this, QStringLiteral("名称"), m_nameEdit));
    layout->addWidget(createPromptRow(this, m_promptEdit));
    layout->addWidget(createInputRow(this, QStringLiteral("模型"), m_modelEdit));

    auto* toolsRow = new QWidget(this);
    auto* toolsLayout = new QHBoxLayout(toolsRow);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(12);
    auto* toolsLabel = makeLabel(QStringLiteral("工具"), ThemeColor::SecondaryText, 13, toolsRow);
    toolsLabel->setFixedWidth(kBotEditLabelWidth);
    toolsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    toolsLabel->setContentsMargins(0, 3, 0, 0);
    auto* toolGrid = new QGridLayout;
    toolGrid->setContentsMargins(0, 0, 0, 0);
    toolGrid->setHorizontalSpacing(14);
    toolGrid->setVerticalSpacing(6);
    toolGrid->addWidget(m_localTimeCheck, 0, 0);
    toolGrid->addWidget(m_calculateCheck, 0, 1);
    toolGrid->addWidget(m_calendarCheck, 1, 0);
    toolGrid->addWidget(m_webSearchCheck, 1, 1);
    toolGrid->addWidget(m_webReadCheck, 2, 0);
    toolsLayout->addWidget(toolsLabel);
    toolsLayout->addLayout(toolGrid, 1);
    layout->addWidget(toolsRow);

    layout->addWidget(createInputRow(this, QStringLiteral("域名"), m_domainsEdit));

    auto* policyRow = new QWidget(this);
    auto* policyLayout = new QHBoxLayout(policyRow);
    policyLayout->setContentsMargins(0, 0, 0, 0);
    policyLayout->setSpacing(12);
    auto* policyLabel = makeLabel(QStringLiteral("策略"), ThemeColor::SecondaryText, 13, policyRow);
    policyLabel->setFixedWidth(kBotEditLabelWidth);
    policyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_confirmSideEffectsCheck->setChecked(true);
    m_cooldownSpin->setRange(0, 3600);
    m_cooldownSpin->setSuffix(QStringLiteral(" 秒"));
    m_cooldownSpin->setFixedWidth(92);
    policyLayout->addWidget(policyLabel);
    policyLayout->addWidget(m_confirmSideEffectsCheck);
    policyLayout->addStretch(1);
    policyLayout->addWidget(makeLabel(QStringLiteral("冷却"), ThemeColor::SecondaryText, 13, policyRow));
    policyLayout->addWidget(m_cooldownSpin);
    layout->addWidget(policyRow);

    layout->addStretch(1);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch(1);
    m_cancelButton->setFixedSize(86, 32);
    m_createButton->setFixedSize(86, 32);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_createButton);
    layout->addLayout(buttonLayout);

    applyTheme();

    connect(m_cancelButton, &StatefulPushButton::clicked, this, [this]() {
        if (cancelRequested) {
            cancelRequested();
        }
    });
    connect(m_createButton, &StatefulPushButton::clicked, this, [this]() {
        submit();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });
    connect(&UploadClient::instance(),
            &UploadClient::uploadSucceeded,
            this,
            [this](const QString& requestId, const NetworkResponse& response) {
                handleAvatarUploadSucceeded(requestId, response);
            });
    connect(&UploadClient::instance(),
            &UploadClient::uploadFailed,
            this,
            [this](const QString& requestId, const NetworkError& error) {
                handleAvatarUploadFailed(requestId, error);
            });
    connect(&HttpClient::instance(),
            &HttpClient::requestSucceeded,
            this,
            [this](const QString& requestId, const NetworkResponse& response) {
                handleAvatarStatusSucceeded(requestId, response);
            });
    connect(&HttpClient::instance(),
            &HttpClient::requestFailed,
            this,
            [this](const QString& requestId, const NetworkError& error) {
                handleAvatarStatusFailed(requestId, error);
            });
}

void GroupBotCreatePopupContent::applyTheme()
{
    const QList<PaintedLabel*> labels = findChildren<PaintedLabel*>();
    for (PaintedLabel* label : labels) {
        const QVariant roleValue = label->property("themeTextRole");
        if (roleValue.isValid()) {
            label->setTextColor(ThemeManager::instance().color(
                    static_cast<ThemeColor>(roleValue.toInt())));
        }
    }

    applyEditStyle(m_nameEdit);
    applyEditStyle(m_modelEdit);
    applyEditStyle(m_domainsEdit);
    applyPlainTextEditStyle(m_promptEdit);
    for (QCheckBox* checkBox : findChildren<QCheckBox*>()) {
        applyCheckBoxPalette(checkBox);
    }
    applySpinBoxStyle(m_cooldownSpin);
    applyDefaultButtonStyle(m_cancelButton);
    m_createButton->setPrimaryStyle();
    updateCreateButtonState();
}

void GroupBotCreatePopupContent::openAvatarCrop(const QImage& image)
{
    if (image.isNull() || m_avatarCropPopup) {
        return;
    }

    auto* content = new AvatarCropPopupContent(image);
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(480, 560);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;
    m_avatarCropPopup = InWindowPopupOverlay::showPopup(this, content, options);
    if (!m_avatarCropPopup) {
        return;
    }

    content->accepted = [this](const QImage& croppedImage) {
        const QString savedPath = saveAvatarImageToAppData(croppedImage, m_group.groupId);
        if (!savedPath.isEmpty()) {
            m_avatarLocalPath = savedPath;
            m_avatarUploadedFileId.clear();
            m_avatarFileId.clear();
            m_avatarStatusRequestId.clear();
            m_avatarStatusPollAttempts = 0;
            m_avatarProcessingInProgress = false;
            m_avatarPreview->setAvatarSource(savedPath);
            startAvatarUpload(savedPath);
        } else {
            GlobalNotification::showFailure(this, QStringLiteral("头像保存失败"));
        }
        if (m_avatarCropPopup) {
            m_avatarCropPopup->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    content->cancelRequested = [this]() {
        if (m_avatarCropPopup) {
            m_avatarCropPopup->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    };
    connect(m_avatarCropPopup, &InWindowPopupOverlay::dismissed, this, [this]() {
        m_avatarCropPopup = nullptr;
    });
}

void GroupBotCreatePopupContent::startAvatarUpload(const QString& path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(trimmedPath);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) {
        GlobalNotification::showFailure(this, QStringLiteral("头像文件不可读取"));
        return;
    }

    m_avatarUploadedFileId.clear();
    m_avatarFileId.clear();
    m_avatarUploadRequestId.clear();
    m_avatarStatusRequestId.clear();
    m_avatarStatusPollAttempts = 0;
    m_avatarUploadInProgress = true;
    m_avatarProcessingInProgress = false;
    updateCreateButtonState();

    const QString requestId = UploadClient::instance().uploadFile(trimmedPath, QStringLiteral("avatar"));
    if (requestId.isEmpty()) {
        m_avatarUploadInProgress = false;
        updateCreateButtonState();
        GlobalNotification::showFailure(this, QStringLiteral("头像上传未启动"));
        return;
    }
    m_avatarUploadRequestId = requestId;
}

void GroupBotCreatePopupContent::handleAvatarUploadSucceeded(const QString& requestId,
                                                             const NetworkResponse& response)
{
    if (requestId != m_avatarUploadRequestId) {
        return;
    }

    m_avatarUploadRequestId.clear();
    m_avatarUploadInProgress = false;
    m_avatarUploadedFileId = avatarFileIdFromUploadResponse(response);
    if (m_avatarUploadedFileId.isEmpty()) {
        updateCreateButtonState();
        GlobalNotification::showFailure(this, QStringLiteral("头像上传成功但没有返回 fileId"));
        return;
    }

    const QString status = processingStatusFromFileResponse(response);
    if (status == QStringLiteral("ready")) {
        m_avatarFileId = m_avatarUploadedFileId;
        m_avatarProcessingInProgress = false;
        updateCreateButtonState();
        return;
    }
    if (isTerminalAvatarStatus(status)) {
        m_avatarProcessingInProgress = false;
        updateCreateButtonState();
        GlobalNotification::showFailure(this, QStringLiteral("头像处理失败"));
        return;
    }

    requestAvatarStatus();
}

void GroupBotCreatePopupContent::handleAvatarUploadFailed(const QString& requestId,
                                                          const NetworkError& error)
{
    if (requestId != m_avatarUploadRequestId) {
        return;
    }

    m_avatarUploadRequestId.clear();
    m_avatarUploadInProgress = false;
    m_avatarProcessingInProgress = false;
    updateCreateButtonState();

    const QString message = error.message.trimmed().isEmpty()
            ? QStringLiteral("头像上传失败")
            : error.message.trimmed();
    GlobalNotification::showFailure(this, message);
}

void GroupBotCreatePopupContent::requestAvatarStatus()
{
    if (m_avatarUploadedFileId.isEmpty() ||
        !m_avatarFileId.isEmpty() ||
        !m_avatarStatusRequestId.isEmpty()) {
        return;
    }

    if (m_avatarStatusPollAttempts >= kAvatarStatusMaxPollAttempts) {
        m_avatarStatusPollAttempts = 0;
        m_avatarProcessingInProgress = false;
        updateCreateButtonState();
        GlobalNotification::showFailure(this, QStringLiteral("头像处理超时，请稍后重试"));
        return;
    }

    ++m_avatarStatusPollAttempts;
    m_avatarProcessingInProgress = true;
    updateCreateButtonState();

    NetworkRequest request = NetworkRequest::json(
            HttpMethod::Get,
            QStringLiteral("/files/%1").arg(m_avatarUploadedFileId));
    request.maxRetries = 0;
    m_avatarStatusRequestId = HttpClient::instance().send(request);
}

void GroupBotCreatePopupContent::handleAvatarStatusSucceeded(const QString& requestId,
                                                             const NetworkResponse& response)
{
    if (requestId != m_avatarStatusRequestId) {
        return;
    }

    m_avatarStatusRequestId.clear();
    const QString status = processingStatusFromFileResponse(response);
    if (status == QStringLiteral("ready")) {
        m_avatarFileId = m_avatarUploadedFileId;
        m_avatarProcessingInProgress = false;
        updateCreateButtonState();
        return;
    }

    if (isTerminalAvatarStatus(status)) {
        m_avatarProcessingInProgress = false;
        updateCreateButtonState();
        GlobalNotification::showFailure(this, QStringLiteral("头像处理失败"));
        return;
    }

    if (m_avatarStatusPollAttempts >= kAvatarStatusMaxPollAttempts) {
        m_avatarStatusPollAttempts = 0;
        m_avatarProcessingInProgress = false;
        updateCreateButtonState();
        GlobalNotification::showFailure(this, QStringLiteral("头像处理超时，请稍后重试"));
        return;
    }

    const QString fileId = m_avatarUploadedFileId;
    QTimer::singleShot(kAvatarStatusPollDelayMs, this, [this, fileId]() {
        if (m_avatarUploadedFileId == fileId && m_avatarFileId.isEmpty()) {
            requestAvatarStatus();
        }
    });
}

void GroupBotCreatePopupContent::handleAvatarStatusFailed(const QString& requestId,
                                                          const NetworkError& error)
{
    if (requestId != m_avatarStatusRequestId) {
        return;
    }

    m_avatarStatusRequestId.clear();
    if (error.isRetryableServiceError() &&
        m_avatarStatusPollAttempts < kAvatarStatusMaxPollAttempts) {
        const QString fileId = m_avatarUploadedFileId;
        QTimer::singleShot(kAvatarStatusPollDelayMs, this, [this, fileId]() {
            if (m_avatarUploadedFileId == fileId && m_avatarFileId.isEmpty()) {
                requestAvatarStatus();
            }
        });
        return;
    }

    m_avatarProcessingInProgress = false;
    updateCreateButtonState();
    const QString message = error.message.trimmed().isEmpty()
            ? QStringLiteral("头像状态获取失败")
            : error.message.trimmed();
    GlobalNotification::showFailure(this, message);
}

void GroupBotCreatePopupContent::updateCreateButtonState()
{
    if (!m_createButton) {
        return;
    }

    if (m_avatarUploadInProgress) {
        m_createButton->setText(QStringLiteral("上传中"));
        m_createButton->setEnabled(false);
        return;
    }
    if (m_avatarProcessingInProgress) {
        m_createButton->setText(QStringLiteral("处理中"));
        m_createButton->setEnabled(false);
        return;
    }

    m_createButton->setText(QStringLiteral("创建"));
    m_createButton->setEnabled(true);
}

QStringList GroupBotCreatePopupContent::selectedTools() const
{
    QStringList tools;
    if (m_localTimeCheck->isChecked()) {
        tools.push_back(QStringLiteral("local_time"));
    }
    if (m_calculateCheck->isChecked()) {
        tools.push_back(QStringLiteral("calculate"));
    }
    if (m_calendarCheck->isChecked()) {
        tools.push_back(QStringLiteral("calendar"));
    }
    if (m_webSearchCheck->isChecked()) {
        tools.push_back(QStringLiteral("web_search"));
    }
    if (m_webReadCheck->isChecked()) {
        tools.push_back(QStringLiteral("web_read"));
    }
    return tools;
}

QStringList GroupBotCreatePopupContent::allowedDomains() const
{
    return splitDomains(m_domainsEdit->text());
}

void GroupBotCreatePopupContent::submit()
{
    GroupBotCreateRequest request;
    request.groupId = m_group.groupId;
    request.name = m_nameEdit->text().trimmed();
    request.avatarFileId = m_avatarFileId.trimmed();
    request.basePrompt = m_promptEdit->toPlainText().trimmed();
    request.model = m_modelEdit->text().trimmed();
    request.allowedTools = selectedTools();
    request.allowedDomains = allowedDomains();
    request.requireConfirmationForSideEffects = m_confirmSideEffectsCheck->isChecked();
    request.cooldownSeconds = m_cooldownSpin->value();

    if (request.groupId.isEmpty() || request.name.isEmpty() || request.basePrompt.isEmpty()) {
        GlobalNotification::showFailure(this, QStringLiteral("请填写机器人名称和设定"));
        return;
    }

    if (request.allowedTools.isEmpty()) {
        GlobalNotification::showFailure(this, QStringLiteral("请至少选择一个工具"));
        return;
    }

    if (!m_avatarLocalPath.isEmpty() && request.avatarFileId.isEmpty()) {
        if (m_avatarUploadedFileId.isEmpty() && !m_avatarUploadInProgress) {
            startAvatarUpload(m_avatarLocalPath);
        } else if (!m_avatarProcessingInProgress) {
            requestAvatarStatus();
        }
        GlobalNotification::showFailure(this, QStringLiteral("头像还在上传或处理，请稍后创建"));
        return;
    }

    if (createRequested) {
        createRequested(request);
    }
}
