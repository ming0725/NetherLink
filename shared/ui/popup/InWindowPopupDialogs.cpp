#include "InWindowPopupDialogs.h"

#include "InWindowPopupOverlay.h"
#include "shared/ui/popup/dialogs/InWindowPopupMessageContent.h"
#include "shared/ui/popup/dialogs/InWindowPopupTextInputContent.h"

#include <QEventLoop>
#include <QPointer>
#include <QTimer>

InWindowPopup::Button InWindowPopup::question(QWidget* parent,
                                             const QString& title,
                                             const QString& text,
                                             Button defaultButton)
{
    Button result = Button::No;
    auto* content = new MessageContent(title, text, defaultButton);

    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(460, 260);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content, options);
    if (!overlay) {
        return result;
    }

    content->buttonActivated = [&result, overlay](Button button) {
        result = button;
        if (overlay) {
            overlay->closePopup(button == Button::Yes
                                ? InWindowPopupOverlay::DismissReason::Accepted
                                : InWindowPopupOverlay::DismissReason::Rejected);
        }
    };

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}

QString InWindowPopup::getText(QWidget* parent,
                               const QString& title,
                               const QString& label,
                               QLineEdit::EchoMode echo,
                               const QString& text,
                               bool* accepted)
{
    if (accepted) {
        *accepted = false;
    }

    auto* content = new TextInputContent(title, label, echo, text);
    QString result = text;
    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content);
    if (!overlay) {
        return QString();
    }

    content->accepted = [accepted, content, overlay, &result]() {
        result = content->text();
        if (accepted) {
            *accepted = true;
        }
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    content->rejected = [overlay]() {
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    };

    QTimer::singleShot(0, content, [content]() {
        content->startEditing();
    });

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}
