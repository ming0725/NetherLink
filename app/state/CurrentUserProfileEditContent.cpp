#include "app/state/CurrentUserProfileEditContent.h"

#include "app/state/CurrentUserPopupStyle.h"
#include "app/state/ProfileAvatarPreview.h"
#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/avatar/AvatarCropPopupContent.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"

#include <QDir>
#include <QHBoxLayout>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

using namespace CurrentUserPopupStyle;

namespace {

QString saveAvatarImageToAppData(const QImage& image, const QString& userId)
{
    if (image.isNull()) {
        return {};
    }

    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath() + QStringLiteral("/NetherLink");
    }

    const QString avatarDirPath = root + QStringLiteral("/avatars");
    QDir avatarDir;
    if (!avatarDir.mkpath(avatarDirPath)) {
        return {};
    }

    const QString fileName = QStringLiteral("%1_%2.png")
            .arg(userId.isEmpty() ? QStringLiteral("user") : userId,
                 QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString filePath = QDir(avatarDirPath).filePath(fileName);
    return image.save(filePath, "PNG") ? filePath : QString();
}

QWidget* createInputRow(QWidget* parent, const QString& title, InlineEditableText* edit)
{
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);

    auto* titleLabel = makeLabel(title, ThemeColor::SecondaryText, 13, row);
    titleLabel->setFixedWidth(kProfileEditLabelWidth);
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    edit->setParent(row);
    rowLayout->addWidget(titleLabel);
    rowLayout->addWidget(edit, 1);
    return row;
}

void configureEdit(InlineEditableText* edit, const QString& placeholder, const QString& text)
{
    edit->setMinimumHeight(kProfileEditInputHeight);
    edit->setPlaceholderText(placeholder);
    edit->setText(text);
    applyEditStyle(edit);
}

} // namespace

class CurrentUserProfileEditContent::Private
{
public:
    CurrentUserProfile profile;
    ProfileAvatarPreview* avatar = nullptr;
    InlineEditableText* nameEdit = nullptr;
    InlineEditableText* regionEdit = nullptr;
    InlineEditableText* signatureEdit = nullptr;
    StatefulPushButton* cancelButton = nullptr;
    StatefulPushButton* saveButton = nullptr;
    QPointer<InWindowPopupOverlay> avatarCropPopup;
};

CurrentUserProfileEditContent::CurrentUserProfileEditContent(const CurrentUserProfile& profile,
                                                             QWidget* parent)
    : QWidget(parent)
    , d(new Private)
{
    d->profile = profile;
    setMinimumSize(420, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(12);

    auto* title = makeLabel(QStringLiteral("编辑资料"), ThemeColor::PrimaryText, 20, this);
    title->setFont(AppFonts::applicationPixelSizedFont(20, true));
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    layout->addWidget(title);

    d->avatar = new ProfileAvatarPreview(this);
    d->avatar->setAvatarSource(d->profile.avatarPath);
    d->avatar->avatarImageSelected = [this](const QImage& image) {
        if (image.isNull() || d->avatarCropPopup) {
            return;
        }
        auto* content = new AvatarCropPopupContent(image);
        InWindowPopupOverlay::Options options;
        options.maximumPopupSize = QSize(480, 560);
        options.dismissOnOutsideClick = true;
        options.dismissOnEscape = true;
        d->avatarCropPopup = InWindowPopupOverlay::showPopup(this, content, options);
        if (!d->avatarCropPopup) {
            return;
        }
        content->accepted = [this](const QImage& croppedImage) {
            const QString savedPath = saveAvatarImageToAppData(croppedImage, d->profile.userId);
            if (!savedPath.isEmpty()) {
                d->profile.avatarPath = savedPath;
                d->avatar->setAvatarSource(savedPath);
            }
            if (d->avatarCropPopup) {
                d->avatarCropPopup->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
            }
        };
        content->cancelRequested = [this]() {
            if (d->avatarCropPopup) {
                d->avatarCropPopup->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
            }
        };
        connect(d->avatarCropPopup, &InWindowPopupOverlay::dismissed, this, [this]() {
            d->avatarCropPopup = nullptr;
        });
    };
    layout->addWidget(d->avatar, 0, Qt::AlignHCenter);

    auto* idLabel = makeLabel(QStringLiteral("ID %1").arg(d->profile.userId),
                              ThemeColor::TertiaryText,
                              12,
                              this);
    idLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    layout->addWidget(idLabel);
    layout->addSpacing(2);

    d->nameEdit = new InlineEditableText(this);
    d->regionEdit = new InlineEditableText(this);
    d->signatureEdit = new InlineEditableText(this);
    configureEdit(d->nameEdit, QStringLiteral("昵称"), d->profile.nickName);
    configureEdit(d->regionEdit, QStringLiteral("地区"), d->profile.region);
    configureEdit(d->signatureEdit, QStringLiteral("个性签名"), d->profile.signature);
    layout->addWidget(createInputRow(this, QStringLiteral("昵称"), d->nameEdit));
    layout->addWidget(createInputRow(this, QStringLiteral("地区"), d->regionEdit));
    layout->addWidget(createInputRow(this, QStringLiteral("签名"), d->signatureEdit));
    layout->addStretch(1);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    d->cancelButton = new StatefulPushButton(QStringLiteral("取消"), this);
    d->saveButton = new StatefulPushButton(QStringLiteral("保存"), this);
    d->cancelButton->setFixedSize(86, 32);
    d->saveButton->setFixedSize(86, 32);
    applyDefaultButtonStyle(d->cancelButton);
    d->saveButton->setPrimaryStyle();
    buttonLayout->addWidget(d->cancelButton);
    buttonLayout->addWidget(d->saveButton);
    layout->addLayout(buttonLayout);

    connect(d->cancelButton, &QPushButton::clicked, this, [this]() {
        if (cancelRequested) {
            cancelRequested();
        }
    });
    connect(d->saveButton, &QPushButton::clicked, this, [this]() {
        CurrentUserProfile next = d->profile;
        next.nickName = d->nameEdit->text().trimmed();
        if (next.nickName.isEmpty()) {
            next.nickName = next.userId;
        }
        next.avatarPath = d->profile.avatarPath;
        next.region = d->regionEdit->text().trimmed();
        next.signature = d->signatureEdit->text().trimmed();
        if (saveRequested) {
            saveRequested(next);
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        const QList<PaintedLabel*> labels = findChildren<PaintedLabel*>();
        for (PaintedLabel* label : labels) {
            const QVariant roleValue = label->property("themeTextRole");
            if (roleValue.isValid()) {
                label->setTextColor(ThemeManager::instance().color(
                        static_cast<ThemeColor>(roleValue.toInt())));
            }
        }
        applyEditStyle(d->nameEdit);
        applyEditStyle(d->regionEdit);
        applyEditStyle(d->signatureEdit);
        applyDefaultButtonStyle(d->cancelButton);
        d->saveButton->setPrimaryStyle();
        update();
    });
}

CurrentUserProfileEditContent::~CurrentUserProfileEditContent()
{
    delete d;
}
