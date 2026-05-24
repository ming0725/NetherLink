#pragma once

#include <QWidget>

#include <functional>

class CurrentUserStatusPopupContent final : public QWidget
{
public:
    explicit CurrentUserStatusPopupContent(int selectedIndex, QWidget* parent = nullptr);
    ~CurrentUserStatusPopupContent() override;

    std::function<void(int)> selectionChanged;

private:
    class Private;
    Private* d = nullptr;
};
