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
- 当前 AI 的模拟流式输出不是静态数据，后续应保留流式 UI 体验并用后端 SSE 替换输出源。

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

当前头像专用上传尚未接入；资料 PATCH 不把本地头像文件路径当作后端头像更新。左侧头像状态仍是本地呈现状态，不映射到后端当前仅表示账号可用性的 `User.status`。

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

头像专用上传、预签名 PUT、多段上传尚未实现，只保留了基础代理上传入口。

### SSE Client

已新增 `shared/network/SseClient.h/.cpp`：

- 支持发起 `Accept: text/event-stream` 的 POST 请求。
- 按 SSE block 解析 `event:` 和 `data:`。
- 将 `data` 解析为 JSON object 后通过信号发出。
- 支持取消流。

当前尚未接入 AI 消息业务，也未实现基于 `clientMessageId` 的业务级重放。

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

## 下一步

建议按以下顺序接入业务，避免一次性重写导致边界混乱：

总体约束：

- 只接入已有 UI、模型和交互能承载的 API；不要因为后端已有接口就新增前端功能。
- 删除静态样例数据依赖。repository 可保留为业务入口和缓存层，但真实业务数据以远程接口为准；`resources/data` 不再存在。
- 网络不可用时显示错误、重试或空状态，不回退到静态本地数据。
- AI 模拟流式输出要正常改造成后端 SSE 流式输出，保留分片追加、停止生成和完成替换等现有交互。

1. 实时连接全量同步
   - `AppEventBus::fullSyncRequired` 已挂到 `RemoteDataBootstrapper::syncAll()`。
   - 针对登录后 `RealtimeClient` 连接失败、token 刷新失败补 UI 提示或重试入口。

2. 聊天和通知事件
   - 为 `chat.message.created`、好友通知、群通知等 event type 建立业务 handler。
   - handler 完成内存/本地状态更新后，再依赖 `EventCursorStore` 推进游标。
   - 如后续需要严格异步 handler 完成语义，可把 dispatcher 改为 handler ack 模式。

3. 文件上传
   - 只在已有入口使用 `UploadClient::uploadFile`，例如头像和聊天图片。
   - 动态模块当前以远程列表、详情和图片展示为主；没有发布器时不要新增动态图片/视频上传。
   - 不接入预签名 PUT、多段上传、聊天文件、聊天音频、聊天视频、帖子视频、直播媒体和 AI 文件上传。

4. AI SSE
   - 用 `SseClient` 替换当前本地模拟流式输出，保留现有流式追加、停止生成和完成态 UI。
   - 发送请求时强制生成并复用 `clientMessageId`。
   - 对 `ai.stream.chunk` 拼接 delta，对 `ai.stream.done` 用服务端消息替换临时消息。
   - `aiFileIds` 固定为空数组或省略，不实现 AI 文件上传、绑定、解析轮询和引用文件回答。

5. 业务 API 封装
   - 每个 feature 增加独立 remote data source，例如 `ChatRemoteDataSource`、`FriendRemoteDataSource`。
   - repository 保留统一业务入口，但不再以静态样例数据作为 fallback；本地只保留缓存、临时 pending 状态、游标和必要的 UI 状态。
   - UI 层只观察 repository/model，不直接调用网络 client。

6. 配置和安全存储
   - 将 base URL、代理、超时接入设置页。
   - access token 保持内存优先。
   - refresh token 是否落盘需结合 macOS Keychain、Windows Credential Manager 或 Qt 平台能力再实现。
