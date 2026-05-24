#pragma once

#include <QLineEdit>
#include <QString>
#include <QWidget>

#include <functional>

class InlineEditableText;

namespace InWindowPopup {

class TextInputContent final : public QWidget
{
public:
    TextInputContent(const QString& title,
                     const QString& label,
                     QLineEdit::EchoMode echo,
                     const QString& text,
                     QWidget* parent = nullptr);

    QString text() const;
    void startEditing();

    std::function<void()> accepted;
    std::function<void()> rejected;

private:
    InlineEditableText* m_edit = nullptr;
};

} // namespace InWindowPopup
