#include "app/frame/current_user/CurrentUserStatusPopupContent.h"

#include "app/frame/current_user/CurrentUserPopupStyle.h"
#include "app/frame/current_user/StatusChoiceCard.h"
#include "shared/services/AppFonts.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/types/User.h"
#include "shared/ui/PaintedLabel.h"

#include <QHBoxLayout>
#include <QVector>
#include <QVBoxLayout>

#include <tuple>

using namespace CurrentUserPopupStyle;

namespace {

AudioService::SoundEffect soundEffectForChoiceIndex(int index)
{
    switch (index) {
    case 0:
        return AudioService::SoundEffect::LevelUp;
    case 1:
        return AudioService::SoundEffect::Mining;
    case 2:
        return AudioService::SoundEffect::Flying;
    case 3:
        return AudioService::SoundEffect::Potion;
    default:
        return AudioService::SoundEffect::LevelUp;
    }
}

} // namespace

class CurrentUserStatusPopupContent::Private
{
public:
    QVector<StatusChoiceCard*> cards;
    int selectedIndex = 0;
};

CurrentUserStatusPopupContent::CurrentUserStatusPopupContent(int selectedIndex, QWidget* parent)
    : QWidget(parent)
    , d(new Private)
{
    setMinimumSize(660, 280);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(34, 28, 34, 34);
    layout->setSpacing(26);

    auto* title = new PaintedLabel(QStringLiteral("选择在线状态"), this);
    title->setFont(AppFonts::applicationPixelSizedFont(20, true));
    title->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    layout->addWidget(title);

    auto* choicesLayout = new QHBoxLayout;
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    choicesLayout->setSpacing(18);
    layout->addLayout(choicesLayout, 1);

    const QVector<std::tuple<QString, QString, int>> choices = {
            {QStringLiteral("在线"), statusIconPath(Online), 0},
            {QStringLiteral("挖矿中"), statusIconPath(Mining), 1},
            {QStringLiteral("飞行模式"), statusIconPath(Flying), 2},
            {QStringLiteral("隐身"), QStringLiteral(":/resources/icon/invisible.png"), 3}
    };
    for (const auto& choice : choices) {
        auto* card = new StatusChoiceCard(std::get<0>(choice),
                                          std::get<1>(choice),
                                          std::get<2>(choice),
                                          this);
        card->clicked = [this](int index) {
            if (index < 0 || index >= kStatusIconChoiceCount) {
                return;
            }
            AudioService::instance().play(soundEffectForChoiceIndex(index));
            d->selectedIndex = index;
            for (StatusChoiceCard* card : d->cards) {
                card->setSelected(card->choiceIndex() == index);
            }
            if (selectionChanged) {
                selectionChanged(index);
            }
        };
        d->cards.push_back(card);
        choicesLayout->addWidget(card, 1);
    }

    d->selectedIndex = qBound(0, selectedIndex, kStatusIconChoiceCount - 1);
    for (StatusChoiceCard* card : d->cards) {
        card->setSelected(card->choiceIndex() == d->selectedIndex);
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this, title]() {
        title->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
        for (StatusChoiceCard* card : d->cards) {
            card->setSelected(card->choiceIndex() == d->selectedIndex);
        }
        update();
    });
}

CurrentUserStatusPopupContent::~CurrentUserStatusPopupContent()
{
    delete d;
}
