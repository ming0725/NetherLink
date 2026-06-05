# 前端网络基础设施搭建记录

## 整个需求

根据 `frontend-api-guide.md` 中的后端契约，为当前 Qt 前端先搭建网络基础设施，先明确网络边界，再逐步把登录、聊天、好友、群组、动态、AI 等已有业务流程接到后端。

本阶段目标是让项目具备清晰的网络接入边界：

- 网络层和业务 repository 分离，业务层不直接依赖 `QNetworkAccessManager`、`QWebSocket` 等 Qt 底层网络对象。
- HTTP JSON 请求统一处理 base URL、`/api/v1` 前缀、query、headers、JSON body、鉴权 header、错误响应和重试规则。
- 认证状态集中管理，支持 access token、refresh token、设备信息、token 刷新和会话清理。
- 实时事件通过 WebSocket 统一连接、心跳、断线重连、事件恢复游标和事件分发。
- 上传、SSE 流式响应预留独立客户端，后续业务可以逐步接入。
- 事件分发要做到解耦：网络层只发布标准事件，业务模块自行订阅对应 event type。
- repository 继续作为业务入口、本地缓存、临时状态和事件游标承载层，但 API 对接不能依赖静态样例数据 fallback；`resources/data` 已删除。
- AI 文本回复通过后端 SSE 输出，保留现有流式追加、停止生成和完成态 UI。

## 已实现需求

### 构建依赖

- `CMakeLists.txt` 已加入 `Qt::Network` 和 `Qt::WebSockets`。
- 新增源码目录：`shared/network`。
- 已通过 Qt 6.11 Debug 构建验证。

### 通用网络类型

已新增 `shared/network/NetworkTypes.h/.cpp`：

- `BackendEnvironment`
  - 默认后端：`http://localhost:8080`
  - 默认 API 前缀：`/api/v1`
  - 统一生成 HTTP API URL 和 WebSocket URL。
- `NetworkRequest`
  - 支持 `GET`、`POST`、`PATCH`、`DELETE`。
  - 支持 query、headers、JSON body、raw body、content type、认证开关、超时、重试次数。
  - 可识别 `Idempotency-Key`、`clientOperationId`、`clientMessageId`。
- `NetworkResponse`
  - 保留 HTTP status、headers、raw body、JSON body、ETag、requestId。
- `NetworkError`
  - 对齐后端错误结构：`code`、`message`、`requestId`、`details`。
  - 提供 token 过期、鉴权失败、可重试服务错误判断。
- `RealtimeEvent`
  - 对齐后端事件结构：`eventId`、`eventSeq`、`type`、`emittedAt`、`payload`。
  - 区分控制事件和需要全量同步的 `sync.required`。

### 认证会话

已新增 `shared/network/AuthSession.h/.cpp`：

- 集中保存 access token、refresh token、过期时间、deviceId、deviceName。
- 提供 `setTokens`、`updateAccessToken`、`clear`。
- 通过 `tokensChanged`、`sessionCleared` 通知外部。

当前仅提供内存态会话；是否落盘 refresh token 需要后续结合平台安全存储策略决定。

### 认证 API 接入

已新增 `shared/network/AuthApiClient.h/.cpp`，并通过 `NetworkService` 对 UI 暴露认证入口：

- `NetworkService::login(account, password)`
  - 请求 `POST /api/v1/auth/login`。
  - 自动携带 `deviceId`、`deviceName`。
  - 成功后解析 `authResponse`，调用 `AuthSession::setTokens`。
  - `NetworkService` 收到登录成功后调用 `RemoteDataBootstrapper::syncAll()` 并启动实时连接。
- `NetworkService::registerAccount(email, password, displayName, userId)`
  - 请求 `POST /api/v1/auth/register`。
  - 当前 Qt 注册窗口没有性别输入，固定传 `gender=unspecified`。
  - `userId` 由邮箱本地部分派生。
  - 成功后调用 `AuthSession::setTokens`，触发远程数据 bootstrap，同步启动实时连接，并把账号写入本地最近账号缓存。
- `NetworkService::logout()`
  - 请求 `POST /api/v1/auth/logout`，body 带当前 `refreshToken`。
  - 回调后停止 `RealtimeClient` 并清理 `AuthSession`。

登录窗口和注册窗口已改为网络优先：

- 登录先走 `/auth/login`，成功后更新 `CurrentUserProfileRepository`、`CurrentUser` 和最近账号缓存。
- 登录响应中的 `user.version/user.etag` 和 `preferences.version/preferences.etag` 会写入当前用户资料与偏好缓存。
- 注册先走 `/auth/register`，成功后缓存账号、当前用户资料、偏好并回填登录窗口。
- `LoginAccountRepository` 仅作为最近登录账号缓存；当开发后端未启动、网络错误或后端返回 `502/503/504/SERVICE_NOT_READY` 时，展示网络错误或重试入口，不回退到本地账号仓库。
- `401`、`409` 等明确业务错误按后端错误处理，不回退到本地 fallback。

### 当前用户与偏好 API

已新增 `app/state/CurrentUserRemoteDataSource.h/.cpp`：

- `fetchProfile()` 请求 `GET /api/v1/me`，成功后写入 `CurrentUserProfileRepository`。
- `updateProfile(profile)` 请求 `PATCH /api/v1/me`，body 发送当前 UI 已有的昵称、签名和地区字段；若模型内有 `etag`，请求带 `If-Match`；若模型内有 `version`，body 带 `expectedVersion`。
- `fetchPreferences()` 请求 `GET /api/v1/me/preferences`，成功后写入 `CurrentUserPreferencesRepository`。
- `updatePreferences(preferences)` 请求 `PATCH /api/v1/me/preferences`，同样使用 `If-Match` 与 `expectedVersion` 做版本条件。
- `CurrentUserProfile` 已保留 `userUuid`、`version`、`etag`；`CurrentUserPreferences` 已保留 `themeColor`、`fontMode`、`inputEffects`、`settings`、`version`、`etag`。
- `CurrentUser::setUserInfo()` 会在本地身份建立后主动拉取 `/me` 和 `/me/preferences`。
- 资料编辑入口调用 `CurrentUser::saveProfile()` 后由远程 PATCH 成功结果更新本地 repository；`VERSION_CONFLICT` 等失败会通过现有全局通知提示。

当前头像专用上传已接入：

- 资料编辑仍先把裁剪后的头像保存为本地 PNG 以便立即预览。
- `CurrentUser::saveProfile()` 发现头像路径变成本地文件时，会调用 `CurrentUserRemoteDataSource::uploadAvatar()`。
- `uploadAvatar()` 使用 `UploadClient::uploadAvatar()` 发起 `POST /api/v1/me/avatar?expectedVersion=<version>`。
- 上传成功后按返回的 `avatarUrl`、`avatarVersion`、`avatarEtag`、`avatarContentHash`、`fileId/avatarFileId` 更新 `CurrentUserProfileRepository`，并触发头像缓存失效。
- 资料 PATCH 仍只发送已有的昵称、签名、地区字段，不把本地头像文件路径写入 `/me`。
- 左侧头像状态仍是本地呈现状态，不映射到后端当前仅表示账号可用性的 `User.status`。

### 远程数据 Bootstrap

已新增 `shared/network/RemoteDataBootstrapper.h/.cpp`：

- 触发时机：
  - 登录成功后调用 `syncAll()`。
  - 注册成功后调用 `syncAll()`。
  - 收到 `AppEventBus::fullSyncRequired` 后再次调用 `syncAll()`。
- 通过现有 `HttpClient` 拉取当前账号可见的首屏/分页快照，并写入 `LocalDataStore`。
- 当前 bootstrap 覆盖或追加的 domain：
  - `current_profiles`：`GET /me`
  - `current_preferences`：`GET /me/preferences`
  - `users`：`GET /users`、`GET /friends`
  - `groups`：`GET /groups`
  - `friend_notifications`：`GET /friend-requests`
  - `group_notifications`：`GET /group-notifications`
  - `notifications`：`GET /notifications`
  - `posts`：`GET /posts`
  - `ai_chat_entries`：`GET /ai/conversations`
  - `conversations`：`GET /conversations`
- 支持 `limit=100&offset=...` 分页续拉；响应字段可为契约数组名、`items`、`data` 或 `results`。
- 会对常见后端字段做轻量归一化，例如 `userId/nickName/avatarUrl` 到当前 repository 可读的 `id/nick/avatarPath`，并保留当前用户 `version/etag`。
- 当前用户资料和偏好已具备独立 remote data source；其他业务写流程、列表刷新信号和更细粒度 remote data source 尚未完成。

### HTTP Client

已新增 `shared/network/HttpClient.h/.cpp`：

- 统一使用 `QNetworkAccessManager` 发起 JSON/原始 body 请求。
- 自动拼接 base URL 和 `/api/v1`。
- 自动添加 `Accept: application/json`、`Content-Type: application/json` 和 `Authorization: Bearer <token>`。
- 对成功响应发出 `requestSucceeded`。
- 对错误响应解析后端标准错误体并发出 `requestFailed`。
- 支持 token 过期后的单刷新入口：
  - 401 且 code 为 `TOKEN_EXPIRED` 时，当前请求进入刷新队列。
  - 只允许一个 refresh 请求在途。
  - refresh 成功后重放队列请求。
  - refresh 失败后清空会话并失败队列请求。
- 支持重试规则：
  - `GET` 或带幂等标识的写请求才自动重试。
  - `502/503/504`、`SERVICE_NOT_READY`、网络错误可重试。
  - `OPERATION_IN_PROGRESS` 仅对带幂等标识的请求重试。
  - `VERSION_CONFLICT` 不自动重试。
  - 默认退避约为 300ms、800ms、2000ms，并带 jitter。

### 上传 Client

已新增 `shared/network/UploadClient.h/.cpp`：

- 支持 multipart 代理上传到 `/api/v1/files`。
- 支持字段：`file`、`targetType`、可选 `contentHash`。
- 自动带 `Authorization`。
- 暴露上传进度、成功、失败信号。
- 支持头像专用 multipart 上传到 `/api/v1/me/avatar`，可携带 `expectedVersion` query。

预签名 PUT、多段上传尚未实现，只保留基础代理上传入口。

### SSE Client

已新增 `shared/network/SseClient.h/.cpp`：

- 支持发起 `Accept: text/event-stream` 的 POST 请求。
- 按 SSE block 解析 `event:` 和 `data:`。
- 将 `data` 解析为 JSON object 后通过信号发出。
- 支持取消流，取消时会断开当前 reply 信号后 abort，避免旧 reply 回调污染新请求。

### AI SSE 对话

已用 `SseClient` 替换 `AiChatStreamClient` 原本地模拟流式输出：

- 发送用户文本后请求 `POST /api/v1/ai/conversations/{conversationId}/messages?stream=true`。
- 请求 body 固定包含 `message`、稳定 `clientMessageId` 和空 `aiFileIds`，不接入 AI 文件上传、绑定和解析轮询。
- `ai.stream.chunk` 的 `delta` 会追加到现有临时 AI 消息，保持输入栏停止态、气泡增量刷新和自动滚动体验。
- `ai.stream.done` 会读取服务端 `assistantMessage`，用服务端 `messageId/text/time` 替换本地临时消息并同步更新 `AiChatRepository`、`AiChatMessageListModel`；若包含 `title`，同步更新会话标题。
- `ai.stream.cancelled` 会按同一终态路径处理；payload 携带 `assistantMessage` 时替换本地临时 AI 消息，未生成 token 时允许没有 `assistantMessage`。
- `client.error` 或 HTTP/SSE 失败会结束流式状态并显示全局失败提示，不回退到本地模拟回复。
- 用户停止生成时优先通过 WebSocket 发送 `ai.stream.cancel`，并等待 SSE/WebSocket 返回 `ai.stream.cancelled`；尚未拿到 `streamId` 时断开 SSE，触发服务端保存 partial 后同步取消终态。

### WebSocket Realtime Client

已新增 `shared/network/RealtimeClient.h/.cpp`：

- 使用 `QWebSocket` 连接：
  - 默认 `ws://localhost:8080/api/v1/ws`
  - HTTPS 环境自动转换为 `wss`
  - URL query 带 `accessToken`、`lastEventSeq`、`lastEventId`
  - 同时设置 `Authorization: Bearer <token>`
- 状态机：
  - `Idle`
  - `Connecting`
  - `Ready`
  - `Stale`
  - `Reconnecting`
  - `Closed`
- 心跳：
  - ready 后每 25 秒发送 `{ "type": "ping", "payload": {} }`
  - 60 秒无消息进入 stale 并重连。
- 重连：
  - 使用 0ms、1s、2s、5s、10s、30s 阶梯退避，并带 jitter。
  - 用户主动关闭后不再重连。
- 恢复：
  - 连接时从 `EventCursorStore` 读取最近成功处理的事件游标。
  - ready 后发送 `resume`。
- 收到 WebSocket 文本消息后解析为 `RealtimeEvent`，并交给 `RealtimeEventDispatcher` 分发。
- 收到 `client.error` 且 code 为 `TOKEN_EXPIRED` 时触发 HTTP token refresh。

### 事件分发与游标存储

已新增：

- `shared/network/AppEventBus.h/.cpp`
- `shared/network/RealtimeEventDispatcher.h/.cpp`
- `shared/network/EventCursorStore.h/.cpp`

实现内容：

- `AppEventBus` 是全局事件总线。
  - 发出原始 `RealtimeEvent`。
  - 发出按 type 拆分的 `typedEventReceived(type, payload, event)`。
  - 对 `sync.required` 发出 `fullSyncRequired`。
- `RealtimeEventDispatcher` 负责把网络事件发布到事件总线。
- `EventCursorStore` 使用现有 `LocalDataStore` 持久化全局事件游标：
  - domain：`network_event_cursor`
  - key：`global`
  - 只保存 `eventSeq > 0` 的最大值。

### 聊天和通知实时事件

已在现有 repository 层接入首批业务实时事件 handler：

- `MessageRepository`
  - 监听 `chat.message.created`、`chat.message.sent` 和 `chat.message.recalled`。
  - 将后端 `Message` payload 转成现有 `TextMessage`、`ImageMessage` 或撤回占位消息。
  - 按 `messageId` 去重后写入内存消息列表，并同步持久化到 `LocalDataStore` 的 `chat_messages` domain。
  - 监听 `chat.conversation.updated`、`chat.conversation.sync.updated`、`chat.read.updated`，把会话状态写入 `conversations` domain。
  - 启动、切换账号或本地缓存变化时，会从 `conversations` 和 `chat_messages` 重建会话列表、未读数、置顶、免打扰和首批消息。
- `FriendNotificationRepository`
  - 监听 `friend.request.*`。
  - 将请求创建、同意、拒绝事件写入 `friend_notifications` domain。
  - 同步更新 `UnreadStateRepository` 中 `friend_requests` scope 的未读状态。
- `GroupNotificationRepository`
  - 监听 `group.notification.created` 和 `group.join_request.*`。
  - 将群通知创建、同意、拒绝事件写入 `group_notifications` domain。
  - 同步更新 `UnreadStateRepository` 中 `group_notifications` scope 的未读状态。

当前 handler 均为同步缓存更新；`RealtimeEventDispatcher` 在 `AppEventBus::publish()` 返回后推进 `EventCursorStore` 游标，因此这些已接入的同步 handler 完成后才会保存 `eventSeq`。若后续接入需要等待 HTTP 补拉或异步写入的 handler，再把 dispatcher 升级为 handler ack 模式。

### 聊天发送与图片上传

已新增 `features/chat/data/ChatRemoteDataSource.h/.cpp`：

- 文本消息通过 `POST /api/v1/conversations/{conversationId}/messages` 发送。
- 聊天图片先调用 `UploadClient::uploadFile(path, "chat_image")` 上传到 `/files`，成功后用返回的 `fileId` 作为 `image` 消息附件发送。
- 每条本端发送消息都生成稳定 `clientMessageId`，失败重试由 `HttpClient` 按幂等消息规则处理。
- `ChatArea` 继续先创建本地乐观消息并保留现有 UI 体验；发送失败用现有全局通知提示。
- `ChatMessage`、`MessageRepository` 和 `ChatListModel` 已保留并按 `clientMessageId` 去重，避免 REST 成功或 WebSocket `chat.message.sent/chat.message.created` 回包重复追加同一条本端消息。
- 当前只接入已有文本消息和单张图片消息入口，不新增聊天文件、语音、视频或多附件 UI。
- 消息撤回通过 `POST /api/v1/conversations/{conversationId}/messages/{messageId}/recall` 发送。
- REST 成功响应中的 `message` / `replacementMessage` 和 WebSocket `chat.message.recalled` 会走同一缓存更新路径：`MessageRepository` 按 `messageId/clientMessageId` 替换本地消息、写入 `chat_messages`，当前打开的 `ChatArea` 收到 `messageUpdated` 后替换对应行；失败只提示“消息撤回失败”，不做本地确认。

### 会话设置、已读和清空

已新增 `features/chat/data/ConversationRemoteDataSource.h/.cpp`：

- 置顶、免打扰和移除会话通过 `PATCH /api/v1/conversations/{conversationId}/settings` 发送。
  - 置顶 body 包含 `isPinned`。
  - 免打扰 body 包含 `isDnd`。
  - 删除/移除会话使用后端隐藏语义，body 包含 `hidden=true`。
- 标记已读通过 `POST /api/v1/conversations/{conversationId}/read` 发送。
- 标记未读通过 `POST /api/v1/conversations/{conversationId}/unread` 发送。
- 清空聊天记录通过 `DELETE /api/v1/conversations/{conversationId}/messages` 发送。
- `MessageRepository` 监听远程成功信号后再更新本地会话状态、未读数、清空消息或移除会话；失败通过会话列表提示，不把纯本地操作当作后端确认。
- 当前 `ConversationMeta` / `ConversationSummary` 尚未承载 `version/etag`，因此设置和清空暂不发送 `expectedVersion` / `If-Match`。

### 好友/群组申请远程写入

已新增 `features/friend/data/FriendRemoteDataSource.h/.cpp`：

- 好友申请同意通过 `POST /api/v1/friend-requests/{requestId}/accept` 发送，body 包含 `remark`、`groupId` 和稳定 `clientOperationId`。
- 好友申请拒绝通过 `POST /api/v1/friend-requests/{requestId}/reject` 发送，body 包含稳定 `clientOperationId`。
- 好友搜索发起申请通过 `POST /api/v1/friend-requests` 发送，body 包含 `toUserUuid`、`message`、`sourceType=search`、空 `sourceGroupId/sourceFriendUuid` 和稳定 `clientOperationId`。
- 群搜索发起入群申请通过 `POST /api/v1/group-join-requests` 发送，body 包含 `groupId`、`message` 和稳定 `clientOperationId`。
- 群入群申请同意通过 `POST /api/v1/group-join-requests/{requestId}/accept` 发送，body 包含稳定 `clientOperationId`；前端选择的群备注和本地分组仍只用于现有本地 UI 状态。
- 群入群申请拒绝通过 `POST /api/v1/group-join-requests/{requestId}/reject` 发送，body 包含稳定 `clientOperationId`。
- 好友备注、分组和免打扰状态通过 `PATCH /api/v1/friends/{friendUserUuid}` 发送，body 包含 `remark`、`groupId`、`isDnd` 和稳定 `clientOperationId`；本地默认分组 `default` 映射为后端 `null`。
- 删除好友通过 `DELETE /api/v1/friends/{friendUserUuid}?clientOperationId=<op>` 发送，同时携带 `Idempotency-Key`。
- `FriendSessionController` 保持 UI 层现有调用入口不变，远程请求成功后复用 `FriendNotificationRepository` / `GroupNotificationRepository` 原有本地缓存更新逻辑。
- 好友资料更新成功后再写入 `UserRepository`；好友详情页、好友列表菜单和聊天资料页的备注/分组编辑共用同一远程结果。
- 删除好友成功后再移除本地会话和好友缓存；好友页、好友列表菜单和聊天资料页的删除入口共用同一远程结果。
- 已新增 `features/chat/data/GroupRemoteDataSource.h/.cpp`：
  - 创建群聊通过 `POST /api/v1/groups` 发送，body 包含 `name`、`memberIds` 和稳定 `clientOperationId`。
  - 群全局资料编辑通过 `PATCH /api/v1/groups/{groupId}` 发送，body 包含 `name`、`introduction`、`announcement` 和稳定 `clientOperationId`。
  - 当前用户群备注、分组和免打扰设置通过 `PATCH /api/v1/groups/{groupId}/my-settings` 发送，body 包含 `remark`、`listGroupId`、`listGroupName`、`isDnd` 和稳定 `clientOperationId`。
  - 退群通过 `DELETE /api/v1/groups/{groupId}/members/{currentUserUuid}?clientOperationId=<op>` 发送，并携带同值 `Idempotency-Key`；`currentUserUuid` 优先来自当前用户资料中的 `userUuid`。
  - 创建群成功后写入服务端返回的群资料并打开会话；群资料、我的群设置和退群远程成功后再写入 `GroupRepository`、移除本地会话和群缓存；好友页、群列表菜单和聊天资料页入口共用同一远程结果。
- 搜索窗口发起申请会等待远程返回；成功只记录本进程 pending 状态并提示“申请已发送”，不直接把对方写成好友或把当前用户写入群成员。失败提示发送失败并允许重试。
- 请求失败时通过 `FriendApplication` / `ChatArea` / 搜索窗口展示“好友申请处理失败”“入群申请处理失败”“好友申请发送失败”“入群申请发送失败”“创建群聊失败”“好友资料保存失败”“删除好友失败”“群资料保存失败”“群设置保存失败”或“退出群聊失败”，不回退到静态样例数据。
- 当前只覆盖已有通知页的同意/拒绝按钮、好友搜索/群搜索发起申请、好友资料编辑和删除好友入口，已有创建群、群资料编辑、我的群设置和退群入口，会话置顶/免打扰/隐藏/已读/未读/清空入口，以及消息撤回入口；群成员管理、转让群主和消息历史分页尚未接入远程。

### 网络状态 UI 汇总

`NetworkService` 已把底层网络状态汇总为 UI 可消费信号：

- `RealtimeClient::connectionError` 会转发为 `NetworkService::realtimeConnectionError`。
- `RealtimeClient::stateChanged` 会转发为 `NetworkService::realtimeStateChanged`。
- `HttpClient::authRefreshFailed` 会停止实时连接，并通过 `NetworkService::sessionExpired` 通知应用壳层。

主窗口当前处理：

- 登录后实时连接失败或进入 stale/reconnecting 时，显示节流后的“实时连接失败，正在重试”全局通知；底层继续按既有退避策略自动重连。
- 实时连接恢复到 ready，且此前出现过连接失败时，显示“实时连接已恢复”。
- token 刷新失败时，当前用户状态会被清理，主窗口提示“登录状态已过期”，随后回到登录窗。

## 下一步

建议按以下顺序接入业务，避免一次性重写导致边界混乱：

总体约束：

- 只接入已有 UI、模型和交互能承载的 API；不要因为后端已有接口就新增前端功能。
- 删除静态样例数据依赖。repository 可保留为业务入口和缓存层，但真实业务数据以远程接口为准；`resources/data` 不再存在。
- 网络不可用时显示错误、重试或空状态，不回退到静态本地数据。

1. 业务 API 封装
   - 每个 feature 增加独立 remote data source。当前已覆盖 `ChatRemoteDataSource`、`FriendRemoteDataSource` 的好友/群申请同意拒绝入口，以及 `GroupRemoteDataSource` 的群资料、我的群设置和退群入口。
   - repository 保留统一业务入口，但不再以静态样例数据作为 fallback；本地只保留缓存、临时 pending 状态、游标和必要的 UI 状态。
   - UI 层只观察 repository/model，不直接调用网络 client。

2. 配置和安全存储
   - 将 base URL、代理、超时接入设置页。
   - access token 保持内存优先。
   - refresh token 是否落盘需结合 macOS Keychain、Windows Credential Manager 或 Qt 平台能力再实现。
