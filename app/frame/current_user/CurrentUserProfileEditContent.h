#pragma once

#include "app/state/CurrentUserProfile.h"

#include <QWidget>

#include <functional>

class CurrentUserProfileEditContent final : public QWidget
{
public:
    explicit CurrentUserProfileEditContent(const CurrentUserProfile& profile,
                                           QWidget* parent = nullptr);
    ~CurrentUserProfileEditContent() override;

    std::function<void(const CurrentUserProfile&)> saveRequested;
    std::function<void()> cancelRequested;

private:
    class Private;
    Private* d = nullptr;
};
