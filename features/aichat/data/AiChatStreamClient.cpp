#include "AiChatStreamClient.h"

#include "shared/theme/ThemeManager.h"

#include <QColor>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>

namespace {

bool promptRequestsDarkMode(const QString& normalizedPrompt)
{
    return normalizedPrompt.contains(QStringLiteral("深色模式")) ||
           normalizedPrompt.contains(QStringLiteral("暗色模式")) ||
           normalizedPrompt.contains(QStringLiteral("dark mode"));
}

bool promptRequestsLightMode(const QString& normalizedPrompt)
{
    return normalizedPrompt.contains(QStringLiteral("浅色模式")) ||
           normalizedPrompt.contains(QStringLiteral("亮色模式")) ||
           normalizedPrompt.contains(QStringLiteral("light mode"));
}

bool promptRequestsThemeColor(const QString& normalizedPrompt)
{
    return normalizedPrompt.contains(QStringLiteral("主题色")) ||
           normalizedPrompt.contains(QStringLiteral("主题颜色")) ||
           normalizedPrompt.contains(QStringLiteral("theme color")) ||
           normalizedPrompt.contains(QStringLiteral("accent color"));
}

QString settingFunctionCallMarkdown(const QString& label,
                                    const QString& action,
                                    const QString& value,
                                    const QString& previousValue,
                                    const QString& previousLabel)
{
    return QStringLiteral("```setting\n"
                          "type: button\n"
                          "label: %1\n"
                          "action: %2\n"
                          "value: %3\n"
                          "previous: %4\n"
                          "previous-label: %5\n"
                          "```")
            .arg(label, action, value, previousValue, previousLabel);
}

QString modeValue(ThemeManager::Mode mode)
{
    switch (mode) {
    case ThemeManager::Mode::Light:
        return QStringLiteral("light");
    case ThemeManager::Mode::Dark:
        return QStringLiteral("dark");
    case ThemeManager::Mode::FollowSystem:
        return QStringLiteral("follow-system");
    }
    return QStringLiteral("follow-system");
}

QString modeLabel(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("light")) {
        return QStringLiteral("外观模式：浅色模式");
    }
    if (normalized == QStringLiteral("dark")) {
        return QStringLiteral("外观模式：深色模式");
    }
    return QStringLiteral("外观模式：跟随系统");
}

ThemeManager::Mode modeFromValue(const QString& value, ThemeManager::Mode fallback)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("light")) {
        return ThemeManager::Mode::Light;
    }
    if (normalized == QStringLiteral("dark")) {
        return ThemeManager::Mode::Dark;
    }
    if (normalized == QStringLiteral("follow-system") || normalized == QStringLiteral("system")) {
        return ThemeManager::Mode::FollowSystem;
    }
    return fallback;
}

QString colorValue(const QColor& color)
{
    return color.toRgb().name(QColor::HexRgb).toUpper();
}

QString themeColorLabel(const QString& value)
{
    return QStringLiteral("主题颜色：%1").arg(value.trimmed().toUpper());
}

QColor colorFromPrompt(const QString& prompt)
{
    static const QRegularExpression hexExpression(QStringLiteral(R"(#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b)"));
    const QRegularExpressionMatch hexMatch = hexExpression.match(prompt);
    if (hexMatch.hasMatch()) {
        return QColor(hexMatch.captured(0));
    }

    const QString normalized = prompt.simplified().toLower();
    const QVector<QPair<QStringList, QColor>> namedColors = {
        {{QStringLiteral("蓝色"), QStringLiteral("蓝"), QStringLiteral("blue")}, QColor(QStringLiteral("#0099FF"))},
        {{QStringLiteral("红色"), QStringLiteral("红"), QStringLiteral("red")}, QColor(QStringLiteral("#E53935"))},
        {{QStringLiteral("绿色"), QStringLiteral("绿"), QStringLiteral("green")}, QColor(QStringLiteral("#2E7D32"))},
        {{QStringLiteral("紫色"), QStringLiteral("紫"), QStringLiteral("purple")}, QColor(QStringLiteral("#8E44AD"))},
        {{QStringLiteral("粉色"), QStringLiteral("粉"), QStringLiteral("pink")}, QColor(QStringLiteral("#E91E63"))},
        {{QStringLiteral("橙色"), QStringLiteral("橙"), QStringLiteral("orange")}, QColor(QStringLiteral("#FF9800"))},
        {{QStringLiteral("黄色"), QStringLiteral("黄"), QStringLiteral("yellow")}, QColor(QStringLiteral("#FBC02D"))},
        {{QStringLiteral("青色"), QStringLiteral("青"), QStringLiteral("cyan")}, QColor(QStringLiteral("#00ACC1"))}
    };
    for (const auto& entry : namedColors) {
        for (const QString& keyword : entry.first) {
            if (normalized.contains(keyword)) {
                return entry.second;
            }
        }
    }

    return {};
}

QColor randomThemeColor()
{
    static const QVector<QColor> colors = {
        QColor(QStringLiteral("#0099FF")),
        QColor(QStringLiteral("#3B82F6")),
        QColor(QStringLiteral("#10B981")),
        QColor(QStringLiteral("#14B8A6")),
        QColor(QStringLiteral("#8B5CF6")),
        QColor(QStringLiteral("#EC4899")),
        QColor(QStringLiteral("#F97316")),
        QColor(QStringLiteral("#EF4444")),
        QColor(QStringLiteral("#06B6D4")),
        QColor(QStringLiteral("#84CC16"))
    };
    return colors.at(QRandomGenerator::global()->bounded(colors.size()));
}

} // namespace

AiChatStreamClient::AiChatStreamClient(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &AiChatStreamClient::emitNextChunk);
}

bool AiChatStreamClient::isRunning() const
{
    return m_running;
}

void AiChatStreamClient::start(const QString& prompt)
{
    cancel();
    m_segments = responseForPrompt(prompt);
    m_segmentIndex = 0;
    m_offset = 0;
    m_running = !m_segments.isEmpty();
    if (m_running) {
        scheduleNextChunk(160, 320);
    }
}

void AiChatStreamClient::cancel()
{
    m_timer->stop();
    m_segments.clear();
    m_segmentIndex = 0;
    m_offset = 0;
    m_running = false;
}

void AiChatStreamClient::emitNextChunk()
{
    if (!m_running) {
        return;
    }

    if (m_segmentIndex >= m_segments.size()) {
        cancel();
        emit finished();
        return;
    }

    const StreamSegment segment = m_segments.at(m_segmentIndex);
    QString chunk;
    if (segment.kind == StreamSegment::Kind::SettingCall) {
        chunk = applySettingCall(segment);
        ++m_segmentIndex;
        m_offset = 0;
    } else {
        const int chunkSize = QRandomGenerator::global()->bounded(2, 8);
        const int nextOffset = qMin(m_offset + chunkSize, segment.text.size());
        chunk = segment.text.mid(m_offset, nextOffset - m_offset);
        m_offset = nextOffset;
        if (m_offset >= segment.text.size()) {
            ++m_segmentIndex;
            m_offset = 0;
        }
    }
    emit chunkReceived(chunk);

    if (m_segmentIndex >= m_segments.size()) {
        cancel();
        emit finished();
        return;
    }

    scheduleNextChunk();
}

QVector<AiChatStreamClient::StreamSegment> AiChatStreamClient::responseForPrompt(const QString& prompt) const
{
    const QString promptPreview = prompt.simplified().left(80);
    const QString normalizedPrompt = prompt.simplified().toLower();
    if (promptRequestsDarkMode(normalizedPrompt)) {
        return {
            textSegment(QStringLiteral("已理解，我会通过 function calling 将外观模式切换为深色模式。\n\n")),
            textSegment(QStringLiteral("下面是检测到的设置变更，请在设置 block 中查看本次调用内容。\n\n")),
            settingSegment(QStringLiteral("settings.appearance.mode"),
                           QStringLiteral("dark"),
                           modeLabel(QStringLiteral("dark"))),
            textSegment(QStringLiteral("\n\n已设置好深色模式。"))
        };
    }
    if (promptRequestsLightMode(normalizedPrompt)) {
        return {
            textSegment(QStringLiteral("已理解，我会通过 function calling 将外观模式切换为浅色模式。\n\n")),
            textSegment(QStringLiteral("下面是检测到的设置变更，请在设置 block 中查看本次调用内容。\n\n")),
            settingSegment(QStringLiteral("settings.appearance.mode"),
                           QStringLiteral("light"),
                           modeLabel(QStringLiteral("light"))),
            textSegment(QStringLiteral("\n\n已设置好浅色模式。"))
        };
    }
    if (promptRequestsThemeColor(normalizedPrompt)) {
        const QColor requestedColor = colorFromPrompt(prompt);
        const QColor nextColor = requestedColor.isValid() ? requestedColor : randomThemeColor();
        const QString value = colorValue(nextColor);
        return {
            textSegment(QStringLiteral("已理解，我会通过 function calling 更新主题颜色。\n\n")),
            textSegment(QStringLiteral("下面是检测到的设置变更，请在设置 block 中查看本次调用内容。\n\n")),
            settingSegment(QStringLiteral("settings.appearance.theme_color"),
                           value,
                           themeColorLabel(value)),
            textSegment(QStringLiteral("\n\n已设置好主题颜色：%1。").arg(value))
        };
    }

    const QStringList replies = {
        QStringLiteral("### 本地流式接口\n\n这次回复来自一个异步 stream client，不会提前把完整内容交给界面层。\n\n你刚才输入的是：**%1**。\n\n#### 处理链路\n\n1. 控制器提交用户消息，并立刻锁定输入状态。\n2. stream client 分批发出 chunk，界面只追加当前可见文本。\n3. 同一个 `messageId` 持续更新，避免列表闪烁或重复插入。\n\n```cpp\nvoid appendChunk(QString& visibleText, const QString& chunk) {\n    if (chunk.isEmpty()) {\n        return;\n    }\n    visibleText += chunk;\n    repository.updateAiChatMessageText(conversationId, messageId, visibleText);\n}\n```\n\n示例公式：\n\n$$\nT_{visible}(k)=\\sum_{i=1}^{k}\\Delta t_i\n$$\n\n这个公式用来描述第 `k` 个分片到达前用户已经等待的可见时间；如果每个分片都很小，界面会更像真实 AI 输出。")
                .arg(promptPreview),
        QStringLiteral("### 增量回复测试\n\n流式请求已经建立，下面的文本会以网络分片形式陆续返回。\n\n> 输入摘要：%1\n\n#### 验证重点\n\n| 场景 | 期望行为 | 观察点 |\n| :--- | :---: | ---: |\n| 正在发送 | 禁止重复提交 | 输入按钮切换为停止态 |\n| 用户取消 | 保留已显示内容 | 后续 chunk 不再追加 |\n| 请求完成 | 解锁输入 | 会话列表时间刷新 |\n\n```text\nrequest started\n  -> chunk received\n  -> append to message bubble\n  -> repaint visible rows\n  -> request finished\n```\n\n为了估算气泡高度，可以把每个 block 的高度写成：\n\n$$\nH=\\sum_{b \\in blocks}(h_b + m_b)\n$$\n\n其中 `h_b` 是 block 自身高度，`m_b` 是它和下一个 block 之间的间距。这个样例故意拉长，方便检查流式追加时滚动位置是否稳定。")
                .arg(promptPreview),
        QStringLiteral("### 可替换网络接口说明\n\n收到。这里走的是可替换网络接口的流式输出，当前只是本地模拟，但控制器和真实接口的边界已经分开。\n\n压力样例：**%1**。\n\n```typescript\ntype StreamEvent =\n  | { type: \"chunk\"; delta: string }\n  | { type: \"done\" }\n  | { type: \"error\"; reason: string };\n\nfunction reduceMessage(previous: string, event: StreamEvent): string {\n  if (event.type !== \"chunk\") return previous;\n  return previous + event.delta;\n}\n```\n\n如果换成真实接口，只需要把网络层输出映射成同样的事件即可。界面侧仍然只关心一个递推关系：\n\n$$\nM_{t+1}=M_t+\\Delta_t\n$$\n\n这里 `M_t` 表示当前气泡里已经显示的 Markdown 文本，`\\Delta_t` 表示本次收到的新片段。")
                .arg(promptPreview),
        QStringLiteral("### 输入体验验证\n\nAI 正在回复时，右下角图标会切换为离线图标，点击后立即停止传输。\n\n关于你的输入：**%1**。\n\n#### 状态机\n\n```mermaid\nstateDiagram-v2\n    Idle --> Streaming: submitUserMessage\n    Streaming --> Idle: finished\n    Streaming --> Canceled: cancel\n    Canceled --> Idle: reset\n```\n\n当前实现里可以把按钮状态理解为一个布尔变量：\n\n$$\nbuttonIcon =\n\\begin{cases}\nstop, & streaming = true \\\\\nsend, & streaming = false\n\\end{cases}\n$$\n\n空闲时图标恢复为书写图标，可以再次发送新消息。这个回复同时包含标题、列表、代码块和公式，用来验证真实对话里的 AI 消息不只是普通文本。")
                .arg(promptPreview)
    };

    return {textSegment(replies.at(QRandomGenerator::global()->bounded(replies.size())))};
}

AiChatStreamClient::StreamSegment AiChatStreamClient::textSegment(const QString& text)
{
    StreamSegment segment;
    segment.kind = StreamSegment::Kind::Text;
    segment.text = text;
    return segment;
}

AiChatStreamClient::StreamSegment AiChatStreamClient::settingSegment(const QString& action,
                                                                     const QString& value,
                                                                     const QString& label)
{
    StreamSegment segment;
    segment.kind = StreamSegment::Kind::SettingCall;
    segment.action = action;
    segment.value = value;
    segment.label = label;
    return segment;
}

QString AiChatStreamClient::applySettingCall(const StreamSegment& segment)
{
    if (segment.action == QStringLiteral("settings.appearance.mode")) {
        const QString previousValue = modeValue(ThemeManager::instance().configuredMode());
        ThemeManager::instance().setMode(modeFromValue(segment.value, ThemeManager::Mode::FollowSystem));
        return settingFunctionCallMarkdown(segment.label,
                                           segment.action,
                                           segment.value,
                                           previousValue,
                                           modeLabel(previousValue));
    }

    if (segment.action == QStringLiteral("settings.appearance.theme_color")) {
        const QString previousValue = colorValue(ThemeManager::instance().themeColor());
        ThemeManager::instance().setThemeColor(QColor(segment.value));
        return settingFunctionCallMarkdown(segment.label,
                                           segment.action,
                                           segment.value,
                                           previousValue,
                                           themeColorLabel(previousValue));
    }

    return settingFunctionCallMarkdown(segment.label,
                                       segment.action,
                                       segment.value,
                                       QString(),
                                       QString());
}

void AiChatStreamClient::scheduleNextChunk(int minDelayMs, int maxDelayMs)
{
    const int lower = qMax(1, minDelayMs);
    const int upper = qMax(lower + 1, maxDelayMs);
    m_timer->start(QRandomGenerator::global()->bounded(lower, upper));
}
