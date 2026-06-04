# NetherLink 前端对接说明

本文档面向前端工程师和后续接手的 AI。前端应把后端视为黑盒函数：只依赖本文描述的传入格式、返回格式、错误格式、网络协议和重连/重试规则，不假设数据库结构或后端内部实现。

> AI_ACTION_REQUIRED: 若你已经按本文某条“待确认/待补充/待实现”指令完成了代码或文档更新，请同步删除对应指令块。不要保留已经完成的指令，避免下一个 AI 重复执行。

> AI_ACTION_REQUIRED: 本文档是第一版前端契约说明，尚未覆盖每一个字段的业务枚举来源和 UI 文案映射。后续若新增接口或修改响应字段，请先更新本文，再改前端调用层。完成后删除本指令。

## 0. 当前 Qt 前端对接范围

本文记录的是后端完整 API 契约，不等同于当前 Qt 前端功能清单。前端对接 API 时必须先匹配已有 UI、模型和 repository 能力；如果接口能力超出当前前端已经实现的交互，不在对接阶段新增 UI 或业务流程。

本地 repository 中的静态样例数据已删除，不能作为 API 对接 fallback。repository 可以继续作为业务入口、本地缓存、临时状态和事件游标承载层，但真实列表、详情、消息、通知、动态和 AI 会话数据应来自后端。当前 AI 的模拟流式输出不是静态数据；它代表已有的流式 UI 交互，后续需要用后端 SSE 正常实现。

当前优先对接：

- 登录、注册、登出、token refresh、实时连接和通用错误/重试。
- 当前用户资料、头像、偏好中已有编辑入口的字段。
- 聊天会话、文本消息、已有图片消息、撤回/引用/已读/未读等现有聊天交互。
- 好友、好友通知、群组、群通知中已经存在的列表、详情、申请、同意/拒绝、成员展示和基础管理入口。
- 动态信息流、详情、图片展示、点赞、关注、评论、回复等当前动态模块已有能力。
- AI 对话仅限当前文本输入、文本流式回复、停止生成、标题等已有对话能力；必须保留流式体验，用后端 SSE 替换当前模拟输出源。

以下后端能力已可作为契约参考，但当前 Qt 前端对接时不要实现：

- 帖子上传或播放视频、直播、音频、普通文件；不要新增动态发布器或媒体管理器。
- 聊天发送视频、音频、普通文件、多附件管理；除已有图片消息外不要扩展附件类型。
- AI 对话上传文件、绑定 AI 文件、文件解析状态、`aiFileIds` 非空消息、AI 文件删除/保留策略。
- 预签名单文件 PUT、多段上传、上传会话查询/取消；当前只保留基础代理上传作为已有图片/头像入口的支撑。
- 账号 session 管理页、批量用户接口、复杂群管理、群主转让、群成员角色编辑、用户公开 ID 冷却提示等没有现成 UI 的管理功能。
- 新增实时事件 handler 时，只处理已有模块能消费的事件；未知或超出 UI 能力的事件记录日志并安全忽略。

## 1. 基础约定

### 1.1 Base URL

开发环境默认：

```text
http://localhost:8080
```

所有业务 HTTP API 使用前缀：

```text
/api/v1
```

健康检查不带此前缀：

```text
GET /healthz
GET /readyz
```

生产环境建议通过 HTTPS 访问后端或网关。当前后端代码没有内置 CORS 中间件，跨域前端需要由网关、反向代理或后端新增 CORS 支持。

### 1.2 HTTP 黑盒调用格式

前端可以将所有 JSON API 抽象为：

```ts
type BackendCall = {
  method: "GET" | "POST" | "PATCH" | "DELETE";
  path: string;
  query?: Record<string, string | number | boolean | undefined>;
  headers?: Record<string, string>;
  body?: unknown;
};
```

JSON 请求：

```http
Content-Type: application/json
Accept: application/json
Authorization: Bearer <accessToken>
X-Request-Id: <optional-client-request-id>
Idempotency-Key: <optional-operation-key>
If-Match: <optional-etag>
If-None-Match: <optional-etag>
```

成功响应通常直接返回业务对象，不再套 `data`。错误响应固定为：

```json
{
  "code": "VALIDATION_ERROR",
  "message": "Invalid request",
  "requestId": "req_xxx",
  "details": {}
}
```

通用错误处理：

| HTTP | code | 前端处理 |
| --- | --- | --- |
| 400 | `VALIDATION_ERROR`, `INVALID_JSON`, `INVALID_FIELD` | 表单字段或请求构造错误，展示可修复提示 |
| 401 | `TOKEN_MISSING`, `TOKEN_EXPIRED`, `TOKEN_INVALID` | 尝试刷新 token；刷新失败则退出登录 |
| 403 | `FORBIDDEN` | 无权限，回退列表或展示无权限页 |
| 404 | `NOT_FOUND` | 资源不存在或不可见 |
| 409 | `CONFLICT`, `VERSION_CONFLICT`, `OPERATION_IN_PROGRESS` 等 | 按业务处理；版本冲突需重新拉取资源 |
| 413 | `REQUEST_TOO_LARGE`, `FILE_TOO_LARGE` | 提示文件或 JSON 请求过大 |
| 415 | `FILE_TYPE_NOT_ALLOWED` | 文件类型不允许 |
| 422 | `FILE_SCAN_REJECTED`, `AI_FILE_PARSE_FAILED` | 内容扫描或 AI 文件解析失败 |
| 429 | `RATE_LIMITED` | 登录/注册尝试过多，等待后再试 |
| 503 | `SERVICE_NOT_READY` | 后端依赖未就绪，可重试 |

### 1.3 JSON 限制和本地校验

- JSON body 最大 1 MiB。
- 后端拒绝未知 JSON 字段，前端不要传未定义字段。
- 时间字段均按 RFC3339 字符串处理，例如 `2026-06-03T12:34:56Z`。
- UUID 字段必须是标准 UUID。
- 分页参数：`limit` 默认 20，最大 100；`offset` 默认 0。
- `clientOperationId`、`clientMessageId`、`Idempotency-Key` 最大 128 字符，只使用字母、数字、`-`、`_`、`.`、`:`。

### 1.4 鉴权

登录或注册返回：

```json
{
  "user": { "userUuid": "uuid", "userId": "public_id", "nickName": "Ming", "version": 1, "etag": "\"user-uuid-v1\"" },
  "preferences": { "themeColor": null, "fontMode": null, "version": 1, "etag": "\"preference-uuid-v1\"" },
  "accessToken": "jwt-or-signed-token",
  "refreshToken": "opaque-refresh-token",
  "expiresIn": 900
}
```

前端存储建议：

- `accessToken` 存内存优先，页面刷新可从安全存储恢复。
- `refreshToken` 必须比 access token 更谨慎，避免写入可被第三方脚本读取的位置。
- 每次 API 调用都带 `Authorization: Bearer <accessToken>`。
- 遇到 `TOKEN_EXPIRED` 时，只允许一个刷新请求在途，其他请求排队等待刷新结果。
- 刷新成功后替换两种 token，并重放原请求。
- 刷新失败、`TOKEN_INVALID` 或 session 被撤销时清空本地会话。

刷新接口：

```http
POST /api/v1/auth/refresh
Content-Type: application/json
```

```json
{ "refreshToken": "string", "deviceId": "web-xxx" }
```

返回：

```json
{ "accessToken": "string", "refreshToken": "string", "expiresIn": 900 }
```

### 1.5 幂等写入

可重复提交的写接口支持以下二选一：

- 请求头 `Idempotency-Key: <key>`
- JSON body 或 query 中的 `clientOperationId`
- 聊天/AI 消息使用 `clientMessageId`

前端规则：

1. 用户触发一次“创建/删除/发送”动作时生成一个稳定 key。
2. 网络失败、超时或 503 重试时复用同一个 key。
3. 不同业务动作不能复用同一个 key。
4. 若返回 `OPERATION_IN_PROGRESS`，前端应短轮询或稍后重试同一请求，不要生成新 key。
5. 已完成的重复请求会直接回放首次成功的响应。

### 1.6 版本控制和缓存

详情资源返回 `ETag` 头，响应体通常也有 `etag` 和 `version`：

- 支持 `If-None-Match` 的 GET：`/me`、`/me/preferences`、`/users/{userUuid}`、`/users/by-id/{userId}`、`/groups/{groupId}`、`/conversations/{conversationId}`、`/files/{fileId}`、`/posts/{postId}`。
- 可变更资源写入可带 `If-Match: <etag>` 或 `expectedVersion`。
- 删除、头像上传等使用 query：`?expectedVersion=3`。
- 版本不匹配返回 `409 VERSION_CONFLICT`，前端应重新拉取当前详情并提示用户合并或覆盖。

## 2. 网络层规范

网络层必须和业务 API 分开实现。建议前端封装三个客户端：

```text
httpClient       负责 JSON、multipart、认证刷新、幂等重试
realtimeClient   负责 WebSocket 心跳、重连、事件序号恢复
uploadClient     负责代理上传、预签名 PUT、多段上传
```

### 2.1 HTTP 重试

默认不要重试非幂等写入，除非请求有 `Idempotency-Key`、`clientOperationId` 或 `clientMessageId`。

建议重试条件：

- 网络断开、DNS、连接超时、读超时。
- HTTP `503 SERVICE_NOT_READY`。
- HTTP `409 OPERATION_IN_PROGRESS`，只对同一幂等 key 重试。
- 对 `GET` 可重试 `502/503/504`。

建议退避：

```text
第 1 次: 300ms + jitter
第 2 次: 800ms + jitter
第 3 次: 2000ms + jitter
之后交给 UI 显示重试按钮
```

不要自动重试：

- `400/401/403/404/413/415/422`。
- `409 VERSION_CONFLICT`。
- 文件上传 complete 返回 `CONTENT_HASH_MISMATCH`、`UPLOAD_PART_ETAG_MISMATCH`。

### 2.2 WebSocket 连接

连接地址：

```text
ws://localhost:8080/api/v1/ws
wss://<host>/api/v1/ws
```

认证方式优先级：

```http
Authorization: Bearer <accessToken>
```

浏览器 WebSocket 不能设置任意 header 时，使用 query：

```text
/api/v1/ws?accessToken=<accessToken>
```

恢复参数：

```text
/api/v1/ws?accessToken=<token>&lastEventSeq=123
/api/v1/ws?accessToken=<token>&lastEventId=evt_123
```

服务端推送事件统一格式：

```json
{
  "eventId": "evt_123",
  "eventSeq": 123,
  "type": "chat.message.created",
  "emittedAt": "2026-06-03T12:34:56Z",
  "payload": {}
}
```

控制事件如 `realtime.ready`、`realtime.pong`、`event.replay`、`chat.message.sent`、`client.error` 的 `eventSeq` 为 0，不参与恢复。

前端必须持久化最近处理成功的 `eventSeq`：

- 只保存 `eventSeq > 0` 的最大值。
- 事件处理完成后再保存，避免崩溃后跳过未处理事件。
- 重连时带上 `lastEventSeq`。
- 收到 `sync.required` 表示事件 backlog 不足，前端必须重新拉取会话、通知、好友、群组等关键列表做全量同步。

当前 Qt 前端实现状态：

- `RealtimeClient` 已按上述规则连接、心跳、重连和携带恢复游标。
- `NetworkService` 会把实时连接状态转发给应用壳；连接失败时主窗口提示正在重试，恢复 ready 后提示已恢复。
- token 刷新失败会清空内存会话，停止实时连接，并回到登录窗；refresh token 仍不落 SQLite。
- `sync.required` 已触发 `RemoteDataBootstrapper::syncAll()`。
- `MessageRepository` 已接入 `chat.message.created`、`chat.message.sent`、`chat.conversation.updated`、`chat.conversation.sync.updated`、`chat.read.updated` 的同步缓存 handler，会把消息写入 `chat_messages`、把会话状态写入 `conversations`，并从这两个 domain 重建会话列表。
- `FriendNotificationRepository` 已接入 `friend.request.*`，会更新 `friend_notifications` 和 `friend_requests` 未读 scope。
- `GroupNotificationRepository` 已接入 `group.notification.created` 与 `group.join_request.*`，会更新 `group_notifications` domain 和同名未读 scope。
- 当前业务 handler 都是同步本地缓存写入；`RealtimeEventDispatcher` 在 `AppEventBus::publish()` 返回后推进游标。后续若 handler 需要等待异步补拉，再升级为 ack 模式。

### 2.3 WebSocket 心跳

后端支持应用层 ping：

```json
{ "type": "ping", "payload": {} }
```

返回：

```json
{
  "eventSeq": 0,
  "type": "realtime.pong",
  "payload": { "receivedAt": "2026-06-03T12:34:56Z" }
}
```

建议策略：

- WebSocket open 后每 25 秒发送一次 `ping`。
- 60 秒未收到任何消息或 `realtime.pong`，主动关闭并重连。
- 页面进入后台可以降低心跳频率到 45 到 60 秒，但恢复前台后立即 ping。

### 2.4 WebSocket 重连

建议状态机：

```text
idle -> connecting -> ready -> stale -> reconnecting -> ready
                         \-> closed
```

重连策略：

- 第一次立即重连。
- 后续使用指数退避：1s、2s、5s、10s、30s，带随机 jitter。
- `TOKEN_EXPIRED` 或握手 401：先刷新 access token，再重连。
- 用户退出登录时关闭连接，不再重连。
- 重连成功后，如果服务端没有自动补发，发送 `resume`：

```json
{
  "type": "resume",
  "payload": { "lastEventSeq": 123 }
}
```

成功返回：

```json
{
  "eventSeq": 0,
  "type": "event.replay",
  "payload": {
    "lastEventSeq": 123,
    "events": []
  }
}
```

### 2.5 SSE 流式 AI

当前 Qt 前端对接范围：AI 只按文本对话接入。发送消息时 `aiFileIds` 必须为空数组或省略，不接入 AI 文件上传、绑定、解析和引用流程。下列 `aiFileIds` 示例仅表示后端契约，不是当前前端实现要求。

AI REST 消息接口支持 SSE：

```http
POST /api/v1/ai/conversations/{conversationId}/messages?stream=true
Accept: text/event-stream
Content-Type: application/json
Authorization: Bearer <accessToken>
```

请求：

```json
{
  "message": "帮我总结这份文档",
  "clientMessageId": "msg_20260603_001",
  "aiFileIds": ["uuid"]
}
```

SSE event 名称：

- `ai.stream.started`
- `ai.stream.chunk`
- `ai.stream.done`
- `ai.stream.cancelled`
- `client.error`

每条 SSE 的 `data` 是 JSON。前端逐步拼接 `ai.stream.chunk.delta`，收到 `ai.stream.done` 后用服务端返回的 `assistantMessage` 替换本地临时消息。

断开重试：

- 使用同一个 `clientMessageId` 重新请求。
- 若服务端仍在处理，返回 `client.error`，code 为 `OPERATION_IN_PROGRESS`。
- 若已完成，服务端会重放 `ai.stream.done`，不会再次调用模型。

## 3. 通用数据模型

### 3.1 User

```ts
type User = {
  userUuid: string;
  userId: string;
  email?: string;
  nickName: string;
  gender: "unspecified" | "male" | "female" | "custom";
  genderCustom: string | null;
  avatarUrl: string | null;
  avatarVersion: number;
  avatarEtag: string | null;
  avatarContentHash: string | null;
  status: "active";
  signature: string | null;
  region: string | null;
  lastSeenAt: string | null;
  version: number;
  etag: string;
  updatedAt: string;
};
```

校验：

- `password`: 8 到 256 字符。
- `userId`: `[A-Za-z0-9_]{4,32}`。
- `nickName`: 1 到 64 字符。
- `signature`: 最多 256 字符。
- `region`: 最多 64 字符。
- `genderCustom`: 最多 64 字符，`gender=custom` 时必填。

### 3.2 Preferences

```ts
type Preferences = {
  themeColor: string | null;
  fontMode: string | null;
  inputEffects: unknown;
  settings: unknown;
  version: number;
  etag: string;
  updatedAt: string;
};
```

当前 Qt 前端已建立 `CurrentUserPreferences` 业务模型和本地缓存，保存 `themeColor`、`fontMode`、`inputEffects`、`settings`、`version`、`etag`。设置页控件到这些字段的完整枚举映射尚未完成，因此本节仍保留底部偏好 schema 待补充指令。

### 3.3 File

```ts
type FileResource = {
  fileId: string;
  fileName: string;
  mimeType: string;
  sizeBytes: number;
  url: string;
  visibility: "private" | "protected" | "public" | "authenticated";
  processingStatus: "uploaded" | "processing" | "ready" | "failed";
  version: number;
  etag: string | null;
  contentHash: string | null;
  createdAt: string;
  updatedAt: string;
  variants: FileVariant[];
};
```

### 3.4 分页

所有列表接口默认支持：

```text
?limit=20&offset=0
```

列表返回通常包含 `total`，聊天消息返回游标字段。

## 4. Auth 和账号 API

### 4.1 注册

```http
POST /api/v1/auth/register
```

```json
{
  "email": "user@example.com",
  "password": "12345678",
  "displayName": "Ming",
  "userId": "ming_001",
  "gender": "unspecified",
  "deviceId": "web-chrome-uuid",
  "deviceName": "Chrome on macOS"
}
```

返回 `201 authResponse`。

### 4.2 登录

```http
POST /api/v1/auth/login
```

```json
{
  "account": "user@example.com or userId",
  "password": "12345678",
  "deviceId": "web-chrome-uuid",
  "deviceName": "Chrome on macOS"
}
```

返回 `200 authResponse`，包含 `preferences`。

### 4.3 登出和 session

```http
POST /api/v1/auth/logout
```

```json
{ "refreshToken": "string" }
```

返回：

```json
{ "ok": true }
```

```http
GET /api/v1/auth/sessions
POST /api/v1/auth/sessions/{sessionId}/revoke
```

当前 Qt 前端接入状态：

- 登录、注册、登出已通过 `shared/network/AuthApiClient` 和 `NetworkService` 接入上述接口。
- 登录/注册成功后会调用 `AuthSession::setTokens`；`NetworkService` 会触发 `RemoteDataBootstrapper::syncAll()` 并启动实时连接。
- 登出会先请求 `/auth/logout`，随后停止实时连接、清理 `AuthSession` 和当前用户。
- 注册窗口当前没有性别输入，固定传 `gender=unspecified`；`userId` 由邮箱本地部分派生。
- 本地账号静态数据已删除；`LoginAccountRepository` 仅保存最近登录账号缓存。网络不可达或 `502/503/504/SERVICE_NOT_READY` 时展示网络错误或重试入口，不回退到本地账号仓库。

sessions 返回：

```json
{
  "sessions": [
    {
      "sessionId": "uuid",
      "deviceId": "web",
      "deviceName": "Chrome",
      "expiresAt": "2026-07-03T00:00:00Z",
      "lastSeenAt": "2026-06-03T00:00:00Z",
      "createdAt": "2026-06-03T00:00:00Z",
      "current": true
    }
  ]
}
```

### 4.4 当前用户

```http
GET /api/v1/me
PATCH /api/v1/me
```

PATCH 请求：

```json
{
  "nickName": "New name",
  "signature": "Hi",
  "region": "Shanghai",
  "status": "active",
  "gender": "custom",
  "genderCustom": "custom text",
  "expectedVersion": 2
}
```

返回 `User`。

当前 Qt 前端接入状态：

- 已新增 `app/state/CurrentUserRemoteDataSource` 封装 `GET /me` 与 `PATCH /me`。
- 登录/注册响应和 `/me` 响应会写入 `CurrentUserProfileRepository`，并保留 `userUuid`、`version`、`etag`。
- `CurrentUser::setUserInfo()` 会主动拉取 `/me`；资料编辑保存会调用 `PATCH /me`。
- PATCH 使用模型内 `etag` 生成 `If-Match`，使用模型内 `version` 生成 `expectedVersion`。
- 当前资料编辑 UI 只发送已有的昵称、签名、地区字段。头像上传仍属于后续文件上传阶段，不通过 `/me` PATCH 传本地头像路径；左侧头像状态属于本地呈现状态，等待 presence 契约明确后再接后端。
- `VERSION_CONFLICT` 等保存失败会显示全局失败通知，不写入静态 fallback。

### 4.5 公开 ID

```http
GET /api/v1/me/user-id/check?userId=ming_001
PATCH /api/v1/me/user-id
```

检查返回：

```json
{ "available": true, "reason": "" }
```

修改请求：

```json
{
  "userId": "new_public_id",
  "clientOperationId": "op_xxx",
  "expectedVersion": 2
}
```

公开 ID 修改有 30 天冷却期。

### 4.6 偏好

```http
GET /api/v1/me/preferences
PATCH /api/v1/me/preferences
```

PATCH：

```json
{
  "themeColor": "#3366ff",
  "fontMode": "system",
  "inputEffects": {},
  "settings": {},
  "expectedVersion": 1
}
```

返回 `Preferences`。

当前 Qt 前端接入状态：

- 已新增 `CurrentUserPreferences` 与 `CurrentUserPreferencesRepository`，缓存 `themeColor`、`fontMode`、`inputEffects`、`settings`、`version`、`etag`。
- 已新增 `CurrentUserRemoteDataSource::fetchPreferences()` 与 `updatePreferences()` 封装 `GET/PATCH /me/preferences`。
- 登录/注册响应中的 `preferences` 会写入本地偏好缓存；`CurrentUser::setUserInfo()` 和 `RemoteDataBootstrapper::syncAll()` 也会拉取 `/me/preferences`。
- PATCH 使用模型内 `etag` 生成 `If-Match`，使用模型内 `version` 生成 `expectedVersion`。
- 设置页控件到远程偏好的完整读写映射尚未完成。

## 5. 文件和上传 API

当前 Qt 前端只允许把文件 API 用于已有图片类入口的支撑，例如头像图片和聊天图片。不要因为后端支持更多 `targetType` 就新增帖子视频、聊天文件、聊天音频、AI 文件上传或多附件管理。预签名单文件上传和多段上传当前不接入。

### 5.1 targetType 白名单

| targetType | 最大大小 | 类型 |
| --- | ---: | --- |
| `avatar`, `group_avatar` | 8 MiB | jpg/jpeg/png/gif |
| `chat_image`, `post_image` | 32 MiB | jpg/jpeg/png/gif |
| `chat_video` | 512 MiB | mp4/mov/webm/m4v |
| `post_video`, `post_live`, `post_media` | 1024 MiB | 图片或视频，按 targetType 限制 |
| `chat_audio` | 64 MiB | aac/flac/m4a/mp3/ogg/opus/wav/weba |
| `chat_file` | 128 MiB | pdf/doc/docx/xls/xlsx/ppt/pptx/txt/md/csv/tsv/json/xml/log/zip |
| `ai_upload` | 4 MiB | text/pdf/docx/pptx/xlsx/csv/html/md/json/xml 等 |

别名：

- `user_avatar` 等同 `avatar`
- `chat_attachment` 等同 `chat_file`

### 5.2 代理上传

小文件可直接走 multipart：

```http
POST /api/v1/files
Content-Type: multipart/form-data
```

字段：

```text
file=<binary>
targetType=chat_image
contentHash=sha256:<optional>
```

返回 `201 FileResource`。

头像专用：

```http
POST /api/v1/me/avatar?expectedVersion=2
Content-Type: multipart/form-data
```

返回：

```json
{
  "avatarUrl": "https://...",
  "avatarVersion": 3,
  "avatarEtag": "\"file-avatar-v3\"",
  "avatarContentHash": "sha256:...",
  "fileId": "uuid"
}
```

### 5.3 预签名单文件上传

当前 Qt 前端暂不接入本节。若已有图片入口的代理上传无法满足性能或大小需求，再单独立项实现。

创建上传会话：

```http
POST /api/v1/files/upload-session
```

```json
{
  "targetType": "chat_image",
  "fileName": "a.png",
  "mimeType": "image/png",
  "sizeBytes": 12345,
  "contentHash": "sha256:...",
  "uploadMode": "single",
  "clientOperationId": "op_upload_001"
}
```

返回：

```json
{
  "uploadSessionId": "uuid",
  "fileId": "uuid",
  "uploadMode": "single",
  "uploadUrl": "https://s3-presigned-url",
  "headers": { "Content-Type": "image/png" },
  "expiresAt": "2026-06-03T13:00:00Z"
}
```

前端随后对 `uploadUrl` 发 `PUT`，必须携带后端返回的 `headers`。PUT 成功后取响应 `ETag`，回调：

```http
POST /api/v1/files/upload-session/{uploadSessionId}/complete
```

```json
{
  "etag": "\"etag-from-storage\"",
  "contentHash": "sha256:..."
}
```

返回：

```json
{
  "fileId": "uuid",
  "url": "https://...",
  "etag": "\"etag\"",
  "contentHash": "sha256:...",
  "version": 1,
  "processingStatus": "processing"
}
```

### 5.4 预签名多段上传

当前 Qt 前端暂不接入本节；不要为帖子视频、直播媒体、聊天大文件新增多段上传流程。

创建时使用：

```json
{
  "targetType": "post_video",
  "fileName": "video.mp4",
  "mimeType": "video/mp4",
  "sizeBytes": 987654321,
  "uploadMode": "multipart",
  "clientOperationId": "op_upload_video_001"
}
```

返回包含：

```json
{
  "uploadMode": "multipart",
  "partSizeBytes": 8388608,
  "totalParts": 118
}
```

申请分片 URL：

```http
POST /api/v1/files/upload-session/{uploadSessionId}/parts
```

```json
{ "partNumbers": [1, 2, 3] }
```

返回：

```json
{
  "parts": [
    { "partNumber": 1, "uploadUrl": "https://...", "headers": {}, "expiresAt": "..." }
  ]
}
```

每个分片 PUT 成功后记录 `{partNumber, etag, sizeBytes}`，最后 complete：

```json
{
  "parts": [
    { "partNumber": 1, "etag": "\"etag1\"", "sizeBytes": 8388608 }
  ],
  "contentHash": "sha256:..."
}
```

查询和取消：

```http
GET /api/v1/files/upload-session/{uploadSessionId}
POST /api/v1/files/upload-session/{uploadSessionId}/abort
```

### 5.5 文件详情和下载

```http
GET /api/v1/files/{fileId}
GET /api/v1/files/{fileId}/download-url?variant=thumbnail_512
DELETE /api/v1/files/{fileId}?expectedVersion=1
```

下载 URL 返回：

```json
{
  "url": "https://signed-url",
  "expiresAt": "2026-06-03T13:00:00Z",
  "headers": {}
}
```

## 6. 用户、好友、群组 API

当前 Qt 前端只对接已有好友、群组 UI 能覆盖的接口。没有现成入口的批量用户、复杂群管理、群主转让、成员角色编辑、公开 ID 管理等功能不要在 API 对接阶段新增。

### 6.1 用户

```http
GET /api/v1/users?keyword=m&limit=20&offset=0
GET /api/v1/users/{userUuid}
GET /api/v1/users/by-id/{userId}
POST /api/v1/users/batch
```

批量请求：

```json
{ "userUuids": ["uuid"] }
```

详情返回：

```json
{
  "user": {},
  "relation": {
    "isFriend": true,
    "remark": "M",
    "friendGroupId": "uuid",
    "isDnd": false,
    "version": 1
  }
}
```

### 6.2 好友

```http
GET /api/v1/friend-groups?keyword=x
POST /api/v1/friend-groups
GET /api/v1/friends?keyword=x&groupId=<uuid>&limit=20&offset=0
PATCH /api/v1/friends/{friendUserUuid}
DELETE /api/v1/friends/{friendUserUuid}?clientOperationId=op_x
```

创建好友分组：

```json
{ "name": "同事", "sortOrder": 0, "clientOperationId": "op_x" }
```

更新好友：

```json
{
  "remark": "备注",
  "groupId": "uuid-or-null",
  "isDnd": false,
  "clientOperationId": "op_x"
}
```

好友请求：

```http
POST /api/v1/friend-requests
GET /api/v1/friend-requests?status=pending&limit=20&offset=0
POST /api/v1/friend-requests/{requestId}/accept
POST /api/v1/friend-requests/{requestId}/reject
```

创建：

```json
{
  "toUserUuid": "uuid",
  "message": "我是...",
  "sourceType": "search",
  "sourceGroupId": null,
  "sourceFriendUuid": null,
  "clientOperationId": "op_x"
}
```

同意：

```json
{ "remark": "Ming", "groupId": "uuid", "clientOperationId": "op_x" }
```

拒绝可以只带：

```json
{ "clientOperationId": "op_x" }
```

### 6.3 群组

```http
GET /api/v1/groups?keyword=x&categoryId=<uuid>&limit=20&offset=0
POST /api/v1/groups
GET /api/v1/groups/{groupId}
PATCH /api/v1/groups/{groupId}
DELETE /api/v1/groups/{groupId}?expectedVersion=1&clientOperationId=op_x
GET /api/v1/groups/{groupId}/members?keyword=x&limit=20&offset=0
POST /api/v1/groups/{groupId}/members
DELETE /api/v1/groups/{groupId}/members/{userUuid}?clientOperationId=op_x
PATCH /api/v1/groups/{groupId}/members/{userUuid}
POST /api/v1/groups/{groupId}/transfer-owner
PATCH /api/v1/groups/{groupId}/my-settings
```

创建：

```json
{
  "name": "项目群",
  "memberIds": ["uuid"],
  "avatarFileId": "uuid",
  "clientOperationId": "op_x"
}
```

更新：

```json
{
  "name": "新群名",
  "introduction": "简介",
  "announcement": "公告",
  "avatarFileId": "uuid-or-null",
  "expectedVersion": 1,
  "clientOperationId": "op_x"
}
```

成员角色：

```text
owner | admin | member
```

入群请求：

```http
POST /api/v1/group-join-requests
GET /api/v1/group-notifications?limit=20&offset=0
POST /api/v1/group-join-requests/{requestId}/accept
POST /api/v1/group-join-requests/{requestId}/reject
```

创建：

```json
{ "groupId": "uuid", "message": "申请加入", "clientOperationId": "op_x" }
```

## 7. 聊天 API

当前 Qt 前端聊天对接以现有文本消息和图片消息为边界。后端支持的 `file`、`audio`、`video` 消息类型仅作为契约保留；前端不要新增发送文件、语音、视频、多附件选择和相关播放器。

### 7.1 会话

```http
GET /api/v1/conversations?keyword=x&limit=20&offset=0
POST /api/v1/conversations/direct
POST /api/v1/conversations/group
GET /api/v1/conversations/{conversationId}
PATCH /api/v1/conversations/{conversationId}/settings
POST /api/v1/conversations/{conversationId}/read
POST /api/v1/conversations/{conversationId}/unread
DELETE /api/v1/conversations/{conversationId}/messages?expectedVersion=1
```

打开单聊：

```json
{ "peerUserUuid": "uuid" }
```

打开群聊：

```json
{ "groupId": "uuid" }
```

会话返回：

```ts
type Conversation = {
  conversationId: string;
  type: "direct" | "group";
  summary: {
    title: string;
    avatarUrl: string | null;
    peerUser?: User;
    group?: unknown;
  };
  syncState: {
    isPinned: boolean;
    isDnd: boolean;
    lastReadMessageId: string | null;
    lastReadAt: string | null;
    unreadCount: number;
    hiddenAt: string | null;
    updatedAt: string;
  };
  lastMessage?: Message;
  lastMessageAt?: string;
  version: number;
  etag: string;
  createdAt: string;
  updatedAt: string;
};
```

更新设置：

```json
{
  "isPinned": true,
  "isDnd": false,
  "hidden": false,
  "expectedVersion": 1
}
```

### 7.2 消息

```http
GET /api/v1/conversations/{conversationId}/messages?beforeMessageSeq=100&limit=20
GET /api/v1/conversations/{conversationId}/messages?afterMessageSeq=100&limit=20
POST /api/v1/conversations/{conversationId}/messages
POST /api/v1/conversations/{conversationId}/messages/{messageId}/recall
```

发送消息：

```json
{
  "clientMessageId": "msg_001",
  "clientSentAt": "2026-06-03T12:34:56Z",
  "type": "text",
  "content": { "text": "hello", "json": null },
  "attachments": [],
  "referencedMessageId": null
}
```

消息类型：

```text
text | image | file | audio | video
```

校验：

- `clientMessageId` 必填，最大 128 字符。
- 文本最多 4000 字符。
- 附件最多 20 个。
- `image` 消息附件必须有 `width` 和 `height`。
- 非 `text` 消息至少有一个附件。

附件：

```json
{
  "fileId": "uuid",
  "displayName": "a.png",
  "durationMs": 1000,
  "width": 800,
  "height": 600
}
```

消息返回：

```ts
type Message = {
  messageId: string;
  conversationId: string;
  senderUuid: string;
  messageSeq: number;
  type: "text" | "image" | "file" | "audio" | "video";
  content: { text?: string; json?: unknown };
  attachments: Array<{
    attachmentId: string;
    fileId: string;
    displayName?: string;
    mimeType: string;
    sizeBytes: number;
    url?: string;
    etag?: string;
    contentHash?: string;
    version: number;
    durationMs?: number;
    width?: number;
    height?: number;
  }>;
  referencedMessageId?: string;
  clientMessageId: string;
  clientSentAt?: string;
  serverReceivedAt: string;
  createdAt: string;
  editedAt?: string;
  recalledAt?: string;
};
```

### 7.3 WebSocket 命令

所有命令格式：

```json
{ "type": "chat.message.send", "payload": {} }
```

支持：

- `ping`
- `resume`
- `chat.message.send`
- `chat.message.recall`
- `chat.read`
- `presence.update`
- `ai.message.send`
- `ai.stream.cancel`

`chat.message.send` payload 等于 REST 发送消息字段，加上 `conversationId`。

成功发送后：

- 持久事件 `chat.message.created` 推给会话相关用户。
- 发送者额外收到控制 ACK `chat.message.sent`：

```json
{
  "type": "chat.message.sent",
  "payload": {
    "clientMessageId": "msg_001",
    "message": {}
  }
}
```

## 8. 通知 API

```http
GET /api/v1/notifications?type=x&limit=20&offset=0
POST /api/v1/notifications/read-all
POST /api/v1/notifications/{notificationId}/read
```

列表返回：

```json
{
  "notifications": [
    {
      "notificationId": "uuid",
      "type": "friend.request.created",
      "sourceId": "uuid",
      "payload": {},
      "readAt": null,
      "createdAt": "...",
      "updatedAt": "..."
    }
  ],
  "total": 1,
  "unreadCount": 1
}
```

全部已读请求可为空 body，或：

```json
{ "type": "friend.request.created" }
```

返回：

```json
{ "ok": true, "updated": 3, "unreadCount": 0 }
```

## 9. 帖子 API

当前 Qt 前端动态对接以现有信息流、详情、图片展示、点赞、关注、评论和回复为边界。后端支持的帖子创建、视频、直播、音频、文件媒体和复杂媒体字段暂不作为前端实现目标；如果没有现成发布 UI，不要新增发布流程。

### 9.1 帖子

```http
GET /api/v1/posts?followOnly=false&limit=20&offset=0
POST /api/v1/posts
GET /api/v1/posts/{postId}
PATCH /api/v1/posts/{postId}
DELETE /api/v1/posts/{postId}?expectedVersion=1&clientOperationId=op_x
POST /api/v1/posts/{postId}/like
POST /api/v1/users/{authorUuid}/follow
```

创建：

```json
{
  "title": "标题",
  "content": "正文",
  "contentCreatedAt": "2026-06-03T12:34:56Z",
  "visibility": "public",
  "media": [
    {
      "fileId": "uuid",
      "mediaType": "image",
      "sortOrder": 0,
      "width": 800,
      "height": 600,
      "durationMs": null,
      "coverFileId": null,
      "playback": {},
      "liveStatus": null,
      "metadata": {}
    }
  ],
  "clientOperationId": "op_x"
}
```

限制：

- `title` 最多 120 字符，可空。
- `content` 必填，最多 10000 字符。
- `visibility`: `public` 或 `private`，空值默认 `public`。
- `media` 最多 20 个。
- `mediaType`: `image`、`video`、`live`、`audio`、`file`。

点赞：

```json
{ "liked": true }
```

返回：

```json
{ "likeCount": 10, "isLiked": true }
```

关注作者：

```json
{ "followed": true }
```

返回：

```json
{ "isFollowedAuthor": true }
```

### 9.2 评论和回复

```http
GET /api/v1/posts/{postId}/comments?limit=20&offset=0
POST /api/v1/posts/{postId}/comments
POST /api/v1/comments/{commentId}/like
POST /api/v1/comments/{commentId}/replies
POST /api/v1/replies/{replyId}/like
DELETE /api/v1/comments/{commentId}?clientOperationId=op_x
DELETE /api/v1/replies/{replyId}?clientOperationId=op_x
```

评论：

```json
{ "content": "评论内容", "clientOperationId": "op_x" }
```

回复：

```json
{
  "content": "回复内容",
  "targetUserUuid": "uuid",
  "targetReplyId": "uuid",
  "clientOperationId": "op_x"
}
```

评论和回复内容最多 2000 字符。

## 10. AI API

当前 Qt 前端 AI 对接以文本会话为边界。可以接入已有输入框、流式回复、取消、标题生成等能力；不要实现文件上传、AI 文件绑定、文件解析轮询、引用文件回答和 AI 文件删除。

### 10.1 会话

```http
GET /api/v1/ai/conversations?limit=20&offset=0
POST /api/v1/ai/conversations
DELETE /api/v1/ai/conversations/{conversationId}?clientOperationId=op_x
GET /api/v1/ai/conversations/{conversationId}/messages
GET /api/v1/ai/conversations/{conversationId}/context
POST /api/v1/ai/title
```

创建：

```json
{
  "title": "可选标题",
  "firstMessage": "可选首条用户消息",
  "clientOperationId": "op_x"
}
```

生成标题：

```json
{ "firstUserMessage": "第一条消息" }
```

返回：

```json
{ "title": "简短标题" }
```

### 10.2 非流式消息

当前文本对接中 `aiFileIds` 固定为空数组或省略。

```http
POST /api/v1/ai/conversations/{conversationId}/messages
```

```json
{
  "message": "你好",
  "clientMessageId": "ai_msg_001",
  "aiFileIds": []
}
```

返回：

```json
{
  "userMessage": {},
  "assistantMessageId": "uuid",
  "streamId": "",
  "usedFiles": []
}
```

### 10.3 AI 文件

当前 Qt 前端暂不接入本节。保留契约仅供后续明确需要“AI 对话上传文件”功能时参考。

流程：

1. 用文件 API 上传 `targetType=ai_upload` 文件。
2. 等 `GET /api/v1/files/{fileId}` 的 `processingStatus=ready`。
3. 绑定到 AI 会话。
4. 轮询 AI 文件详情，直到 `parseStatus=ready`。
5. 发送 AI 消息时带 `aiFileIds`。

绑定：

```http
POST /api/v1/ai/conversations/{conversationId}/files
```

```json
{
  "fileId": "uuid",
  "purpose": "reference",
  "retentionPolicy": "conversation",
  "clientOperationId": "op_x"
}
```

枚举：

- `purpose`: `reference`、`attachment`，空值默认 `reference`。
- `retentionPolicy`: `conversation`、`temporary`、`source_deleted_after_parse`、`none`，空值默认 `conversation`。
- `temporary` 默认 24 小时后过期。
- `source_deleted_after_parse` 会在解析成功且没有其他业务引用时删除源文件对象，但保留抽取文本和 chunks。

查询：

```http
GET /api/v1/ai/conversations/{conversationId}/files
GET /api/v1/ai/files/{aiFileId}
DELETE /api/v1/ai/files/{aiFileId}?clientOperationId=op_x
```

详情返回：

```json
{
  "aiFile": {
    "aiFileId": "uuid",
    "conversationId": "uuid",
    "fileId": "uuid",
    "purpose": "reference",
    "parseStatus": "ready",
    "extractedTextFileId": "uuid",
    "tokenCount": 123,
    "retentionPolicy": "conversation",
    "expiresAt": null,
    "createdAt": "...",
    "updatedAt": "..."
  },
  "parseStatus": "ready",
  "tokenCount": 123,
  "previewText": "..."
}
```

### 10.4 反馈

```http
POST /api/v1/ai/messages/{messageId}/feedback
```

```json
{
  "feedback": "liked",
  "clientOperationId": "op_x"
}
```

`feedback`: `liked`、`disliked`、`none`。

## 11. 实时事件列表

前端应通过事件 `type` 分发，未知事件安全忽略并记录日志。

当前服务端会推送：

- `realtime.ready`
- `realtime.pong`
- `event.replay`
- `sync.required`
- `chat.message.created`
- `chat.message.recalled`
- `chat.conversation.sync.updated`
- `chat.conversation.updated`
- `chat.read.updated`
- `profile.updated`
- `preference.updated`
- `friend.group.created`
- `friend.updated`
- `friend.deleted`
- `friend.request.*`
- `group.created`
- `group.updated`
- `group.deleted`
- `group.members.added`
- `group.member.removed`
- `group.member.updated`
- `group.owner.transferred`
- `group.my_settings.updated`
- `group.join_request.*`
- `group.notification.created`
- `notification.read.updated`
- `post.created`
- `post.updated`
- `post.deleted`
- `post.like.updated`
- `post.comment.created`
- `post.comment.deleted`
- `post.reply.created`
- `post.reply.deleted`
- `post.comment.like.updated`
- `post.reply.like.updated`
- `post.author.follow.updated`
- `presence.updated`
- `ai.stream.started`
- `ai.stream.chunk`
- `ai.stream.done`
- `ai.stream.cancelled`
- `client.error`

前端推荐处理：

| 事件 | 处理 |
| --- | --- |
| `chat.message.created` | 写入消息列表，更新会话 lastMessage |
| `chat.conversation.updated` | 更新会话列表项 |
| `chat.conversation.sync.updated` | 更新未读数、置顶、免打扰、隐藏状态 |
| `notification.read.updated` | 更新通知未读数 |
| `profile.updated` | 刷新用户缓存和头像缓存 key |
| `preference.updated` | 更新本地偏好 |
| `presence.updated` | 更新好友在线状态 |
| `sync.required` | 全量重新拉取关键列表 |
| `client.error` | 根据 `payload.requestType` 和 `payload.code` 处理失败命令 |

当前 Qt 前端已落地 `chat.message.created`、`chat.message.sent`、`chat.conversation.updated`、`chat.conversation.sync.updated`、`chat.read.updated`、`friend.request.*`、`group.notification.created`、`group.join_request.*` 的同步缓存处理。尚未接入的事件继续安全忽略或由已有 `profile.updated`、`group.updated` 等局部 handler 消费。

## 12. 前端实现建议

### 12.1 HTTP client 伪代码

```ts
async function request<T>(call: BackendCall): Promise<T> {
  const token = authStore.accessToken;
  const res = await fetch(buildUrl(call.path, call.query), {
    method: call.method,
    headers: {
      Accept: "application/json",
      ...(call.body ? { "Content-Type": "application/json" } : {}),
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...call.headers,
    },
    body: call.body ? JSON.stringify(call.body) : undefined,
  });

  if (res.status === 304) return undefined as T;
  const payload = await res.json().catch(() => ({}));
  if (!res.ok) throw new BackendError(res.status, payload);
  return payload as T;
}
```

### 12.2 本地临时状态

- 发送聊天消息时先创建 pending 消息，key 用 `clientMessageId`。
- 收到 REST 响应或 WS `chat.message.sent` 后替换 pending。
- 收到 `chat.message.created` 时按 `messageId` 和 `clientMessageId` 去重。
- AI 流式回复先创建临时 assistant 消息，chunk 拼接，done 后替换为服务端消息。

### 12.3 缓存 key

- 用户头像缓存 key 使用 `avatarVersion` 或 `avatarContentHash`。
- 文件预览缓存 key 使用 `fileId + version + contentHash`。
- 会话缓存 key 使用 `conversationId + version`。
- 帖子缓存 key 使用 `postId + version`。

## 13. 待补充指令

> AI_ACTION_REQUIRED: 若前端开始实现，请先生成 TypeScript 类型文件，至少覆盖 `User`、`Preferences`、`FileResource`、`Conversation`、`Message`、`Post`、`Notification`、`AIConversation`、`AIMessage`、`AIUploadedFile`、`BackendError`。完成后删除本指令。

> AI_ACTION_REQUIRED: 若后端新增 CORS、Cookie 鉴权或 CSRF 保护，请更新“基础约定”和“鉴权”章节。完成后删除本指令。

> AI_ACTION_REQUIRED: 若确认所有事件 payload 的完整类型，请在“实时事件列表”章节把 `payload` 从描述改为精确 TypeScript discriminated union。完成后删除本指令。

> AI_ACTION_REQUIRED: 若前端 UI 已确定偏好字段 `themeColor`、`fontMode`、`inputEffects`、`settings` 的枚举和 schema，请补充到 `Preferences` 章节。完成后删除本指令。

> AI_ACTION_REQUIRED: 若上线环境不使用 `localhost:8080`，请补充环境变量和部署网关 base URL 规则。完成后删除本指令。
