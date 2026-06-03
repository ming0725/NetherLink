# NetherLink Local Data Storage

## 改动说明

当前本地存储只作为服务端数据快照、最近账号缓存、临时交互状态和事件游标使用。`resources/data` 静态业务种子已经删除，登录态数据必须来自服务端响应。

- 新增 `shared/data/LocalDataStore.h/.cpp`，统一管理本地数据目录、SQLite 连接、基础 schema、JSON 记录读写。
- `CMakeLists.txt` 增加 `Qt::Sql` 依赖。
- `UserRepository`、`GroupRepository`、`CurrentUserProfileRepository`、`LoginAccountRepository`、`UnreadStateRepository` 启动时只从 SQLite 读取，不再从 qrc 种子导入。
- 帖子、评论、好友通知、群通知、AI 会话列表均读取 SQLite 快照；空库时返回空列表，由 `RemoteDataBootstrapper` 登录后拉取服务端数据填充。
- 点赞状态、评论数增量、评论点赞状态、未读状态、最近登录账号仍写入 SQLite，作为临时或用户交互状态。
- AI 聊天本地新建/编辑能力仍会写入 SQLite；后续服务端 SSE 接入后再细化消息同步和冲突处理。

## 本地结构

当前 `LocalDataStore` 使用 Qt 的 `QStandardPaths::AppDataLocation`：

```text
<AppDataLocation>/
  netherlink-local.sqlite
```

如果系统没有返回应用数据目录，则回退到：

```text
<temp>/NetherLink/netherlink-local.sqlite
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
users
groups
current_profiles
login_accounts
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
- 每账号单独数据库时，每个账号使用独立 SQLCipher key。
- key 由系统安全存储保存，不应写入普通配置文件或代码。

## 多账号建议

当前实现先落在单个本地库，便于把 C++ 死数据迁出。后续多账号应改成：

```text
AppData/NetherLink/
  global.sqlite
  anonymous/
    settings.sqlite
  accounts/
    <account_hash>/
      data.sqlite
      cache/
        avatars/
        images/
        attachments/
```

其中：

- `global.sqlite`：只保存最近登录账号、匿名窗口状态、schema version。
- `anonymous/settings.sqlite`：登录前主题默认值、登录窗大小位置。
- `accounts/<account_hash>/data.sqlite`：该账号设置、缓存快照、同步队列。
- `account_hash = sha256(user_uuid + app_salt)`，不要直接使用邮箱或可修改的公开用户 ID 做目录名。

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

头像/图片缓存：

- 用户资料必须带 `avatar_url`、`avatar_version`、`avatar_etag`、`avatar_content_hash`。
- 用户资料、群资料、帖子作者资料等接口必须直接带 `avatar_url`；本地缓存只保存 URL 对应的下载结果。
- 服务端数据库内部固定使用 `avatar_file_id` 关联 `files.id`；`avatar_file_id` 不进入客户端用户模型。
- 本地缓存键使用 `avatar:<user_uuid>:<avatar_version>`，不要使用可修改的公开 `user_id`。
- 资料刷新发现版本变化时下载新头像，更新 `file_cache`，通知 UI 刷新。
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
