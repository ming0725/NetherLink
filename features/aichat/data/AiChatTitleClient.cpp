#include "features/aichat/data/AiChatTitleClient.h"

namespace {

constexpr int kMaxTitleLength = 24;

} // namespace

AiChatTitleClient::AiChatTitleClient(QObject* parent)
    : QObject(parent)
{
}

QString AiChatTitleClient::generateTitle(const QString& firstUserMessage) const
{
    return fallbackTitle(firstUserMessage);
}

QString AiChatTitleClient::fallbackTitle(const QString& firstUserMessage)
{
    QString title = firstUserMessage.trimmed();
    const int lineBreak = title.indexOf(QLatin1Char('\n'));
    if (lineBreak >= 0) {
        title = title.left(lineBreak).trimmed();
    }
    title = title.simplified();
    if (title.size() > kMaxTitleLength) {
        title = title.left(kMaxTitleLength).trimmed() + QStringLiteral("...");
    }
    return title.isEmpty() ? QStringLiteral("新对话") : title;
}
