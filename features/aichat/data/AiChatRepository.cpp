#include "AiChatRepository.h"

#include <QRandomGenerator>
#include <QStringList>
#include <QtMath>

#include <algorithm>

namespace {

QString normalizedMessageText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

QString normalizedAiMessageText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString markdownCapabilitySample()
{
    return QString::fromUtf8(R"MARKDOWN(# Markdown 能力演示
这是一段用于 AI 聊天气泡的 Markdown 样例，目标是同时覆盖排版、代码、表格、列表、引用、链接、行内公式和块级公式。它不依赖 `Qt::setMarkdown()`，而是由本项目的渲染器拆成 block 后用 `QTextLayout` 与 `QPainter` 绘制。

示例说明：这一屏先展示标题、段落和行内样式。你可以用它检查字号层级、段落间距、加粗粗细、斜体倾斜角度、删除线位置和行内代码背景是否稳定。

## 1. 段落和行内元素
段落支持 **加粗文本会明显变粗**、*斜体文本会保持可读性*、`inlineCode()`、~~删除线~~、[外部链接](https://www.qt.io) 和行内公式 $\Delta t = t_{end} - t_{start}$。如果一段文字很长，它会在气泡宽度内自然换行，并且不会挤压到头像或气泡边缘。

示例说明：下面这条公式放在段落之后，用来检查公式块的上下留白是否和普通文本相邻时仍然自然。

$$
S_{paragraph}
=
\sum_{i=1}^{n}
w_i x_i
\quad
\text{where}
\quad
\sum_{i=1}^{n} w_i = 1
$$

### 2. 引用和分隔线
标题从 H1 到 H6 使用独立字号，避免不同平台自带 Markdown 样式不一致。引用块会保留语义，但不会把 `>` 符号直接画进正文，而是使用左侧引用线强调内容层级。

> 设计备注：引用适合放结论、限制条件或用户原文摘录。渲染时需要保证多行引用的左侧线条高度、文字基线和上下间距都一致。

---

示例说明：分隔线之后再插入公式，能观察数学 block 是否不会和水平线粘在一起。

$$
\operatorname{score}(q, d)
=
\frac{q \cdot d}{\lVert q \rVert \lVert d \rVert}
$$

#### 3. 列表排版
无序列表用于展示并列能力点：

- 第一项：列表符号和正文之间保留固定缩进。
- 第二项：当文字很长时，会在列表文字区域自动换行，而不是压到项目符号下面；这条内容故意写得更长，用来观察换行后的第二行是否和第一行正文左侧对齐。
  - 二级无序列表：缩进后仍支持 **加粗**、`行内代码` 和链接。
  - 二级无序列表：也可以继续承载较长说明，便于检查嵌套列表的视觉密度。
- 第三项：列表项之间的间距应该比普通段落更紧凑。

有序列表用于展示流程：

1. 收集用户输入，保留原始换行和 Markdown 标记。
2. 解析为 block 列表，再为每个 block 计算尺寸。
   1. 二级步骤：代码块、表格和数学公式会走专用测量逻辑。
   2. 二级步骤：普通段落则交给文本布局处理换行。
3. 绘制气泡背景、文本内容和可选的代码复制按钮。

示例说明：列表后接一个矩阵公式，用来验证列表 block 与公式 block 之间不会显得拥挤。

$$
\begin{bmatrix}
1 & 0 & 0 \\
0 & \cos\theta & -\sin\theta \\
0 & \sin\theta & \cos\theta
\end{bmatrix}
\begin{bmatrix}
x \\
y \\
z
\end{bmatrix}
=
\begin{bmatrix}
x' \\
y' \\
z'
\end{bmatrix}
$$

#### 4. 表格
表格适合展示对比信息。下面的列分别使用左对齐、居中和右对齐，单元格内部也可以包含行内样式。

| 功能 | 当前状态 | 说明 |
| :--- | :---: | ---: |
| 标题层级 | **支持** | H1 到 H6 独立字号 |
| 表格渲染 | 支持 | 自动绘制边框、表头和单元格背景 |
| 代码高亮 | `支持` | 支持复制按钮和等宽字体 |
| 数学公式 | 支持 | 行内公式与块级公式分开布局 |
| 链接命中 | 支持 | [GitHub](https://github.com) 可点击 |

示例说明：表格后插入一个较短的损失函数，检查表格底部和公式顶部的距离。

$$
\mathcal{L}(\theta)
=
-\frac{1}{m}
\sum_{i=1}^{m}
\left[
y_i \log \hat{y}_i
+
(1-y_i)\log(1-\hat{y}_i)
\right]
$$

##### 5. 代码块
代码块使用独立背景、等宽字体和语言标签。下面的 C++ 示例故意包含模板、容器、lambda、异常处理和日志宏，用来测试高亮覆盖面。

```cpp
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define LOG(x) std::cout << "[LOG] " << x << '\n'

template <typename T>
class Box {
public:
    explicit Box(T value)
        : value_(std::make_unique<T>(std::move(value))) {}

    const T& get() const noexcept {
        return *value_;
    }

    void set(T value) {
        *value_ = std::move(value);
    }

private:
    std::unique_ptr<T> value_;
};

int main() {
    std::vector<int> nums {1, 2, 3, 4, 5};
    std::transform(nums.begin(), nums.end(), nums.begin(), [](int x) {
        return x * x;
    });

    Box<std::string> message("Hello, C++ syntax highlighting!");
    for (const int n : nums) {
        LOG((n % 2 == 0 ? "even square: " : "odd square: ") << n);
    }

    try {
        throw std::runtime_error(message.get());
    } catch (const std::exception& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }
}
```

示例说明：代码块之后放一个递推公式，用来检查代码块底部留白、复制按钮区域和后续公式之间的视觉节奏。

$$
dp[i][j]
=
\min
\begin{cases}
dp[i-1][j] + c_{delete} \\
dp[i][j-1] + c_{insert} \\
dp[i-1][j-1] + c_{replace}
\end{cases}
$$

###### 6. 更长的数学内容
长公式不再连续堆在同一个区域，而是作为本节的压力样例出现。它用于验证复杂分式、积分、上下标、希腊字母、集合关系、极限、矩阵和盒选公式的绘制质量。

示例说明：第一条是多项式长行，主要观察横向排版是否仍在气泡内处理得合理。

$$
f(x)
=
a_1x^{100}+a_2x^{99}+a_3x^{98}+a_4x^{97}+a_5x^{96}
+a_6x^{95}+a_7x^{94}+a_8x^{93}+a_9x^{92}+a_{10}x^{91}
$$

示例说明：第二条是积分和级数混合，主要观察分式高度、根号范围和上下限。

$$
\int_{-\infty}^{+\infty}
\frac{
\left(
\sum_{n=1}^{\infty}
\frac{(-1)^n x^{2n+1}}{(2n+1)!}
\right)^2
}{
\sqrt{
1+
\left(
\frac{\partial^2 u}{\partial x^2}
\right)^2
}
}
\cdot e^{-x^2}\,dx
=
\frac{
\Gamma\left(\frac12\right)\zeta(2)
}{
\displaystyle
\lim_{n\to\infty}
\left(1+\frac1n\right)^n
}
$$

示例说明：最后一条集中覆盖符号族，但它被放在文档末尾，避免和前面的公式样例挤在一起。

$$
\begin{aligned}
&
\forall x \in \mathbb{R},
\quad
\exists y \in \mathbb{C},
\quad
\neg (x \approx y)
\iff
\sum_{n=0}^{\infty}\frac{(-1)^n}{n!}
=
e^{-1}
\\
&
\alpha,\beta,\gamma,\Gamma,\Delta,\epsilon,\varepsilon,\theta,\lambda,
\mu,\pi,\Pi,\sigma,\Sigma,\phi,\varphi,\omega,\Omega
\\
&
\left\{
\begin{matrix}
a+b=c \\
x^2+y^2=z^2
\end{matrix}
\right.
\qquad
\begin{bmatrix}
1 & 2 \\
3 & 4
\end{bmatrix}
\\
&
\nabla^2u
=
\frac{\partial^2 u}{\partial x^2}
+
\frac{\partial^2 u}{\partial y^2},
\qquad
\vec{F}=m\vec{a},
\qquad
\overrightarrow{AB}
\\
&
A \subset B \subseteq C,
\qquad
a \le b \ge c \neq d,
\qquad
\lim_{x\to0}\frac{\sin x}{x}=1
\\
&
\boxed{E = mc^2}
\qquad
\text{Hello\ LaTeX}
\end{aligned}
$$)MARKDOWN");
}

} // namespace

AiChatRepository::AiChatRepository(QObject* parent)
    : QObject(parent)
{
    const QStringList sampleTitles = {
        QStringLiteral("深度研究助理"),
        QStringLiteral("界面重构草案"),
        QStringLiteral("插件调试记录"),
        QStringLiteral("多代理协作实验"),
        QStringLiteral("日报自动整理"),
        QStringLiteral("提示词优化备份"),
        QStringLiteral("模型切换对照"),
        QStringLiteral("前端交互验证")
    };

    const QDateTime now = QDateTime::currentDateTime();
    m_entries.reserve(40);
    for (int i = 0; i < 40; ++i) {
        const int offsetDays = QRandomGenerator::global()->bounded(0, 18);
        const int offsetMinutes = QRandomGenerator::global()->bounded(0, 24 * 60);
        AiChatListEntry entry {
                QStringLiteral("ai-chat-%1").arg(m_nextConversationId++),
                QStringLiteral("%1 %2")
                        .arg(sampleTitles.at(i % sampleTitles.size()))
                        .arg(i + 1),
                now.addDays(-offsetDays).addSecs(-offsetMinutes * 60)
        };
        m_entries.push_back(entry);
    }

    QVector<int> latestIndexes;
    latestIndexes.reserve(m_entries.size());
    for (int index = 0; index < m_entries.size(); ++index) {
        latestIndexes.push_back(index);
    }
    std::sort(latestIndexes.begin(), latestIndexes.end(), [this](int lhs, int rhs) {
        return m_entries.at(lhs).time > m_entries.at(rhs).time;
    });

    const int candidateCount = qMin(8, latestIndexes.size());
    const int unreadDotCount = qMin(4, candidateCount);
    QSet<int> selectedIndexes;
    while (selectedIndexes.size() < unreadDotCount) {
        selectedIndexes.insert(latestIndexes.at(QRandomGenerator::global()->bounded(candidateCount)));
    }
    for (int index : selectedIndexes) {
        m_entries[index].hasUnreadDot = true;
    }
}

AiChatRepository& AiChatRepository::instance()
{
    static AiChatRepository repo;
    return repo;
}

QVector<AiChatListEntry> AiChatRepository::requestAiChatList(const AiChatListRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    if (query.limit <= 0 || query.offset < 0 || query.offset >= m_entries.size()) {
        return {};
    }

    QVector<AiChatListEntry> sortedEntries = m_entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const AiChatListEntry& lhs, const AiChatListEntry& rhs) {
        return lhs.time > rhs.time;
    });

    const int offset = qBound(0, query.offset, sortedEntries.size());
    const int limit = qMax(0, query.limit);
    return sortedEntries.mid(offset, limit);
}

QVector<AiChatMessage> AiChatRepository::requestAiChatMessages(const QString& conversationId) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_seededMessageConversationIds.contains(conversationId)) {
        for (int index = 0; index < m_entries.size(); ++index) {
            if (m_entries.at(index).conversationId == conversationId) {
                appendInitialMessages(m_entries.at(index), index);
                m_seededMessageConversationIds.insert(conversationId);
                break;
            }
        }
    }
    return m_messages.value(conversationId);
}

AiChatContextUsage AiChatRepository::requestAiChatContextUsage(const AiChatContextUsageRequest& request) const
{
    QMutexLocker locker(&m_mutex);
    if (request.conversationId.isEmpty()) {
        return {};
    }

    if (!m_seededMessageConversationIds.contains(request.conversationId)) {
        for (int index = 0; index < m_entries.size(); ++index) {
            if (m_entries.at(index).conversationId == request.conversationId) {
                appendInitialMessages(m_entries.at(index), index);
                m_seededMessageConversationIds.insert(request.conversationId);
                break;
            }
        }
    }

    const AiChatContextUsage usage = buildContextUsageLocked(request.conversationId);
    if (usage.available) {
        m_contextUsages.insert(request.conversationId, usage);
    } else {
        m_contextUsages.remove(request.conversationId);
    }
    return usage;
}

QString AiChatRepository::createAiChatConversation(const QString& title, const QDateTime& time)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty() || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    const QString conversationId = QStringLiteral("ai-chat-%1").arg(m_nextConversationId++);
    const AiChatListEntry entry {conversationId, trimmedTitle, time, false};
    m_entries.push_back(entry);
    m_messages.insert(conversationId, {});
    m_seededMessageConversationIds.insert(conversationId);
    return conversationId;
}

AiChatMessage AiChatRepository::addAiChatMessage(const QString& conversationId,
                                                 const QString& text,
                                                 bool isFromUser,
                                                 const QDateTime& time)
{
    const QString messageText = isFromUser ? normalizedMessageText(text) : normalizedAiMessageText(text);
    if (conversationId.isEmpty() || (isFromUser && messageText.isEmpty()) || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    auto entryIt = std::find_if(m_entries.begin(), m_entries.end(), [&conversationId](const AiChatListEntry& entry) {
        return entry.conversationId == conversationId;
    });
    if (entryIt == m_entries.end()) {
        return {};
    }

    AiChatMessage message {
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            conversationId,
            messageText,
            isFromUser,
            time
    };
    m_messages[conversationId].push_back(message);
    m_contextUsages.remove(conversationId);
    entryIt->time = time;
    return message;
}

bool AiChatRepository::updateAiChatMessageText(const QString& conversationId,
                                               const QString& messageId,
                                               const QString& text,
                                               const QDateTime& time)
{
    const QString messageText = normalizedAiMessageText(text);
    if (conversationId.isEmpty() || messageId.isEmpty() || !time.isValid()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    auto messagesIt = m_messages.find(conversationId);
    if (messagesIt == m_messages.end()) {
        return false;
    }

    for (AiChatMessage& message : messagesIt.value()) {
        if (message.messageId == messageId) {
            message.text = messageText;
            message.time = time;
            m_contextUsages.remove(conversationId);
            for (AiChatListEntry& entry : m_entries) {
                if (entry.conversationId == conversationId) {
                    entry.time = time;
                    break;
                }
            }
            return true;
        }
    }

    return false;
}

bool AiChatRepository::setConversationUnreadDot(const QString& conversationId, bool unread)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        for (AiChatListEntry& entry : m_entries) {
            if (entry.conversationId != conversationId) {
                continue;
            }

            if (entry.hasUnreadDot == unread) {
                return false;
            }

            entry.hasUnreadDot = unread;
            changed = true;
            break;
        }
    }

    if (changed) {
        emit unreadDotStateChanged();
    }
    return changed;
}

int AiChatRepository::unreadDotCount() const
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const AiChatListEntry& entry : m_entries) {
        if (entry.hasUnreadDot) {
            ++count;
        }
    }
    return count;
}

bool AiChatRepository::removeAiChatMessage(const QString& conversationId, const QString& messageId)
{
    if (conversationId.isEmpty() || messageId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    auto messagesIt = m_messages.find(conversationId);
    if (messagesIt == m_messages.end()) {
        return false;
    }

    QVector<AiChatMessage>& messages = messagesIt.value();
    for (int row = 0; row < messages.size(); ++row) {
        if (messages.at(row).messageId != messageId) {
            continue;
        }

        messages.removeAt(row);
        m_contextUsages.remove(conversationId);
        for (AiChatListEntry& entry : m_entries) {
            if (entry.conversationId == conversationId) {
                entry.time = messages.isEmpty()
                        ? QDateTime::currentDateTime()
                        : messages.constLast().time;
                break;
            }
        }
        return true;
    }

    return false;
}

bool AiChatRepository::renameAiChatConversation(const QString& conversationId, const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (conversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    for (AiChatListEntry& entry : m_entries) {
        if (entry.conversationId == conversationId) {
            entry.title = trimmedTitle;
            return true;
        }
    }
    return false;
}

bool AiChatRepository::removeAiChatConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    bool hadUnreadDot = false;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&conversationId](const AiChatListEntry& entry) {
            return entry.conversationId == conversationId;
        });
        if (it == m_entries.end()) {
            return false;
        }

        hadUnreadDot = it->hasUnreadDot;
        m_entries.erase(it);
        m_messages.remove(conversationId);
        m_seededMessageConversationIds.remove(conversationId);
        m_contextUsages.remove(conversationId);
    }

    if (hadUnreadDot) {
        emit unreadDotStateChanged();
    }
    return true;
}

AiChatContextUsage AiChatRepository::buildContextUsageLocked(const QString& conversationId) const
{
    const QVector<AiChatMessage> messages = m_messages.value(conversationId);
    if (messages.isEmpty()) {
        return {conversationId, 0, 128000, false};
    }

    int characterCount = 0;
    int userMessageCount = 0;
    int assistantMessageCount = 0;
    for (const AiChatMessage& message : messages) {
        characterCount += message.text.size();
        if (message.isFromUser) {
            ++userMessageCount;
        } else {
            ++assistantMessageCount;
        }
    }

    const int maxTokens = 128000;
    const int structuralTokens = 900 + messages.size() * 96 + userMessageCount * 34 + assistantMessageCount * 56;
    const int simulatedContentTokens = qCeil(static_cast<qreal>(characterCount) / 2.8);
    const int usedTokens = qBound(1, structuralTokens + simulatedContentTokens, maxTokens);
    return {conversationId, usedTokens, maxTokens, true};
}

void AiChatRepository::appendInitialMessages(const AiChatListEntry& entry, int sampleIndex) const
{
    const QStringList prompts = {
        QStringLiteral("帮我把这段需求拆成可执行的实现步骤。"),
        QStringLiteral("分析一下这个界面交互有没有明显问题。"),
        QStringLiteral("给出一个更稳妥的重构方案。"),
        QStringLiteral("整理一下今天的调试结论。")
    };
    const QString sampleReply = markdownCapabilitySample();
    const QStringList replies = {sampleReply, sampleReply, sampleReply, sampleReply};

    QVector<AiChatMessage>& messages = m_messages[entry.conversationId];
    const QDateTime firstTime = entry.time.addSecs(-180);
    messages.push_back({
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            entry.conversationId,
            prompts.at(sampleIndex % prompts.size()),
            true,
            firstTime
    });
    messages.push_back({
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            entry.conversationId,
            replies.at(sampleIndex % replies.size()),
            false,
            entry.time
    });
}
