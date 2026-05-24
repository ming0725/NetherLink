#include "shared/ui/popup/dialogs/InWindowPopupTextInputContent.h"

#include "shared/ui/InlineEditableText.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/popup/dialogs/InWindowPopupDialogStyle.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace InWindowPopup {

TextInputContent::TextInputContent(const QString& title,
                                   const QString& label,
                                   QLineEdit::EchoMode echo,
                                   const QString& text,
                                   QWidget* parent)
    : QWidget(parent)
    , m_edit(new InlineEditableText(this))
{
    setMinimumWidth(380);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(DialogStyle::kPopupPadding,
                               DialogStyle::kPopupPadding,
                               DialogStyle::kPopupPadding,
                               DialogStyle::kPopupPadding);
    layout->setSpacing(14);
    layout->addWidget(DialogStyle::createTitleLabel(title, this));
    layout->addWidget(DialogStyle::createBodyLabel(label, this));

    m_edit->setTrimTextOnCommit(false);
    m_edit->setEchoMode(echo);
    m_edit->setText(text);
    m_edit->setMinimumHeight(DialogStyle::kInputHeight);
    DialogStyle::applyTextInputStyle(m_edit);
    layout->addWidget(m_edit);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto* cancelButton = DialogStyle::createButton(QStringLiteral("取消"), this);
    DialogStyle::applyDefaultButtonStyle(cancelButton);
    auto* okButton = DialogStyle::createButton(QStringLiteral("确认"), this);
    okButton->setPrimaryStyle();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, this, [this]() {
        if (accepted) {
            accepted();
        }
    });
    connect(m_edit, &InlineEditableText::returnPressed, this, [this]() {
        if (accepted) {
            accepted();
        }
    });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        if (rejected) {
            rejected();
        }
    });
}

QString TextInputContent::text() const
{
    return m_edit ? m_edit->text() : QString();
}

void TextInputContent::startEditing()
{
    if (m_edit) {
        m_edit->startEditing();
    }
}

} // namespace InWindowPopup
