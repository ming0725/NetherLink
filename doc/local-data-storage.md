# NetherLink Local Data Storage

## 改动说明

当前本地存储只作为服务端数据快照、最近账号缓存、临时交互状态和事件游标使用。`resources/data` 静态业务种子已经删除，登录态数据必须来自服务端响应。除登录界面的历史账号列表外，所有账号态数据必须写入当前账号目录，不能再落到单个全局业务库。

- `shared/data/LocalDataStore.h/.cpp` 统一管理本地数据目录、SQLite 连接、基础 schema、JSON 记录读写，并按当前账号路由到独立账号库。
- `CMakeLists.txt` 增加 `Qt::Sql` 依赖。
- `LoginAccountRepository` 只读写全局 `global.sqlite` 的 `login_accounts` domain。
- `UserRepository`、`GroupRepository`、`CurrentUserProfileRepository`、`UnreadStateRepository` 等账号态仓库只读写 `accounts/<account_hash>/data.sqlite`，并在账号切换或同步落库后重载内存快照。
- 帖子、评论、好友通知、群通知、AI 会话列表均读取 SQLite 快照；空库时返回空列表，由 `RemoteDataBootstrapper` 登录后拉取服务端数据填充。
- 点赞状态、评论数增量、评论点赞状态、未读状态等用户交互状态写入账号库；最近登录账号写入全局库。
- AI 聊天本地新建/编辑能力仍会写入 SQLite；文本回复已经接入后端 SSE，服务端完成态会覆盖本地临时消息并同步会话标题。
- 好友申请和群入群申请的同意/拒绝按钮已经先调用远程 API，成功后再更新 `friend_notifications`、`group_notifications`、`users`、`groups`、`conversations` 等本地快照；失败只显示错误提示，不把静态样例或纯本地结果当作后端确认。
- 认证 token 仍只保存在 `AuthSession` 内存态；token 刷新失败后由 `NetworkService::sessionExpired` 通知应用壳清理当前用户并回到登录窗，不写入 SQLite。
- 聊天列表不再生成演示历史消息或演示未读数；消息和未读状态必须来自服务端会话状态或用户真实本地操作。

## 本地结构

当前 `LocalDataStore` 使用 Qt 的 `QStandardPaths::AppDataLocation`：

```text
<AppDataLocation>/
  global.sqlite
  accounts/
    <account_hash>/
      data.sqlite
      cache/
        images/
          <sha256(source-with-avatar-version-fragment)>.img
```

如果系统没有返回应用数据目录，则回退到：

```text
<temp>/NetherLink/
  global.sqlite
  accounts/<account_hash>/data.sqlite
```

SQLite 内部先使用通用 JSON 记录表：

```sql
local_records(
  domain TEXT NOT NULL,
  key TEXT NOT NULL,
  value_json TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  PRIMARY KEY(domain, key)
)
```

当前 domain 约定：

```text
login_accounts
users
groups
current_profiles
unread_state
post_like_states
post_comment_count_deltas
comment_like_states
reply_like_states
friend_notifications
group_notifications
ai_chat_entries
ai_chat_messages
posts
post_comments
notifications
conversations
network_event_cursor
```

`login_accounts` 是唯一全局 domain，只保存登录界面的历史账号登录信息。其它 domain 都是账号态 domain，路由到当前登录账号的 `accounts/<account_hash>/data.sqlite`。

`network_event_cursor` 只记录当前账号已成功处理的实时事件游标，不保存 access token、refresh token 或 WebSocket 连接状态。

已删除的静态资源目录：

```text
resources/data/
```

不要恢复该目录作为开发 fallback。测试 fixture 应放在测试目录，不进入运行时 qrc 资源。

## SQLCipher

`LocalDataStore` 会读取环境变量 `NETHERLINK_SQLCIPHER_KEY`。如果该变量存在，会在打开数据库后执行：

```sql
PRAGMA key = '<key>';
PRAGMA cipher_version;
```

如果当前 Qt SQLite driver 链接的是 SQLCipher，`cipher_version` 会返回版本，`LocalDataStore::isEncrypted()` 会为 true。若 Qt 仍是普通 SQLite driver，则数据库会正常作为未加密 SQLite 使用。

后续更稳妥的生产方案：

- token、refresh token、私钥不要进入 SQLite，应使用 Keychain/Credential Manager。
- 每个账号应使用独立 SQLCipher key。
- key 由系统安全存储保存，不应写入普通配置文件或代码。

## 多账号隔离

当前实现已经按账号拆分本地目录：

```text
AppData/NetherLink/
  global.sqlite
  accounts/
    <account_hash>/
      data.sqlite
      cache/
        images/
        attachments/
```

其中：

- `global.sqlite`：只保存最近登录账号等登录界面历史信息；不得写入好友、群、消息、通知、帖子、AI 会话、偏好、事件游标等账号态数据。
- `accounts/<account_hash>/data.sqlite`：该账号设置、缓存快照、同步队列。
- `account_hash = sha256(user_uuid + app_salt)`，不要直接使用邮箱或可修改的公开用户 ID 做目录名。
- 用户远程图片缓存默认位于当前账号目录下的 `cache/images/`，避免不同账号共享头像或媒体缓存状态。
- 登录、注册成功后，网络层必须先调用 `LocalDataStore::setActiveAccount()`，再保存当前用户资料、偏好并启动 `RemoteDataBootstrapper::syncAll()`。

## 后续网络部分

网络层不要直接改 UI。建议在现有 Repository 下面拆三层：

```text
Repository
  LocalDataSource(SQLite)
  RemoteDataSource(HTTP/WebSocket)
  SyncCoordinator
```

读流程：

1. Repository 先读 SQLite，立即返回本地快照或空列表。
2. 登录/注册成功、`sync.required` 或页面刷新时，后台调用 `RemoteDataBootstrapper`/RemoteDataSource。
3. 服务端返回 `version`、`etag` 或 `updated_at`。
4. 无变化时只更新 `fetched_at`；有变化时覆盖 SQLite，并发出 model 刷新信号。
5. 不存在资源 JSON 种子 fallback；不要把演示数据和真实账号数据混合合并。

写流程：

1. UI 调 Repository 写入。
2. Repository 先写本地乐观状态。
3. 同步操作进入 `sync_outbox`。
4. SyncCoordinator 发送到服务端。
5. 服务端确认后写入最终版本；失败则回滚或标记冲突。

当前例外：

- 聊天文本/图片发送由 `ChatRemoteDataSource` 生成 `clientMessageId` 并发送远程请求，UI 保持本地乐观消息；REST/WS 回包按 `messageId/clientMessageId` 去重覆盖。
- 好友申请和群入群申请同意/拒绝由 `FriendRemoteDataSource` 先发送远程请求，成功后复用现有 repository 更新本地通知、好友/群成员和会话提示消息；暂未引入通用 `sync_outbox` 表。

头像/图片缓存：

- 用户资料必须带 `avatar_url`、`avatar_version`、`avatar_etag`、`avatar_content_hash`。
- 用户资料、群资料、帖子作者资料等接口必须直接带 `avatar_url`；本地缓存只保存 URL 对应的下载结果。
- 服务端数据库内部固定使用 `avatar_file_id` 关联 `files.id`；`avatar_file_id` 不进入客户端用户模型。
- 前端展示用的 `avatarPath` 会把远程 URL 和 `avatar_version` / `avatar_content_hash` / `avatar_etag` 合成为带 `#nl-cache=...` fragment 的 source；真正 HTTP 请求会去掉 fragment，只把它作为本地缓存 key。
- 本地远程图片缓存位于 `accounts/<account_hash>/cache/images/`，文件名使用 `sha256(source-with-avatar-version-fragment)`；SQLite 继续保存用户、群资料 JSON 和头像元数据，不保存二进制图片。
- 资料刷新或 `profile.updated` / `group.updated` 实时事件发现头像版本变化时，Repository 写入新资料，旧 source 通过 `ImageService::invalidateSource()` 清掉内存和磁盘缓存，UI 通过已有 model/repaint 信号刷新。
- 本地缓存键逻辑应等价于 `avatar:<user_uuid>:<avatar_version>`；当前 C++ 模型仍沿用既有 `user.id` 字段和 `avatarPath` 字段承载 UI 数据。
- 旧头像按账号缓存目录清理。

帖子：

- 帖子流和详情建议保持打开即请求，不做长期 SQLite 内容缓存。
- 不把帖子图片存成简单 URL list；服务端返回统一 `media[]`，每项包含 `media_type`、`url`、`thumbnail_url`、`width`、`height`、`duration_ms`、`version`、`etag`、`content_hash`，后续视频和实况也走同一结构。
- 只缓存媒体文件和用户本地交互的待确认状态。
- 帖子、评论、回复列表接口必须返回当前登录用户视角的 `viewer` 状态，例如 `viewer.isLiked`、`viewer.isAuthor`、`viewer.isFollowedAuthor`。
- 点赞、评论等写操作进入同步队列，由服务端确认后刷新该帖子；服务端返回值覆盖本地乐观状态。

消息：

- 本地 `messages` 只缓存当前设备最近访问过的分页，不承诺完整历史。
- 本地消息缓存可按数量、时间或磁盘空间清理；清理不影响服务端历史。
- 未读数、最后已读消息、最后消息预览以后端会话状态为准，本地只缓存最近一次服务端状态。
- 发送消息先写本地乐观记录时，必须保存 `client_message_id`；服务端 ack 后用服务端 `message_id`、`message_seq`、`server_received_at` 覆盖。
- 不使用用户本机时间作为最终消息时间，不用本地插入顺序作为最终消息顺序。
