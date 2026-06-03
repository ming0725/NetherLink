# NetherLink 后端需求总表

本文只定义后端需要提供和保证的能力：数据模型、接口、鉴权、实时事件、响应结构和错误码。调用方内部架构和运行时保存策略不属于本文范围，不能作为后端开发约束。

## 1. 总体原则

| 项 | 要求 |
| --- | --- |
| 数据权威 | 用户资料、好友关系、群资料、群权限、通知、会话状态、消息、偏好配置以后端持久化数据为准。 |
| 身份模型 | 用户必须同时具备不可变内部 UUID 和可修改公开 ID。内部 UUID 用作数据库主键和业务外键；公开 ID 只用于展示、搜索和用户输入。 |
| 接口边界 | API 返回业务所需字段，不暴露内部关联字段、对象存储路径、密码散列、refresh token hash 等实现细节。 |
| 分页同步 | 列表、消息、通知等接口必须支持分页或游标补偿，不要求任何调用方一次性拉取全量历史。 |
| 版本校验 | 资料、文件、群、会话等可变资源返回 `version`、`etag`、`updatedAt`，GET 接口支持 `If-None-Match` 和 304。 |
| 幂等写入 | 写接口支持 `Idempotency-Key` 或业务级 `clientOperationId`，避免重试导致重复创建、重复发送或重复扣减。 |
| 实时推送 | 会话、消息、通知、在线状态、资料变更通过 WebSocket 增量推送；REST 用于首屏、分页、重试和补偿拉取。 |

## 2. 后端数据库表总表

建议使用 PostgreSQL。所有表默认包含 `created_at`、`updated_at`；需要软删的业务表增加 `deleted_at`。`users.id` 固定使用不可变 UUID；其它业务表主键也建议统一使用 UUID，并在 API 中按字符串返回。

用户身份固定拆成两层：

- `users.id`：不可变内部 UUID，作为数据库主键和所有外键引用目标。
- `users.public_id`：用户可读公开 ID，字符串类型，全局唯一，允许修改。它只用于展示、搜索、复制和用户主动输入，不作为关系表外键。

因此所有数据库字段名里的 `user_id`、`sender_id`、`owner_id`、`author_id` 都表示 `users.id` 这个不可变 UUID；公开可读 ID 统一命名为 `public_id`，API 字段名为 `userId`。用户资料 API 必须同时返回 `userUuid` 和 `userId`。

头像、群头像和媒体文件在后端内部使用 `files.id` 做稳定关联。API 的公开资料对象直接返回 `avatarUrl`、`avatarVersion`、`avatarEtag`、`avatarContentHash`；`avatar_file_id` 只作为后端内部字段和上传结果字段，不作为公开资料字段。

| 表名 | 关键字段 | 用途 |
| --- | --- | --- |
| `users` | `id`, `public_id`, `email`, `password_hash`, `nick`, `gender`, `avatar_file_id`, `status`, `signature`, `region`, `last_seen_at`, `token_version`, `version` | 登录主体和公开资料。 |
| `user_id_changes` | `id`, `user_id`, `old_public_id`, `new_public_id`, `changed_at`, `client_operation_id` | 记录公开 ID 修改历史，支持审计、风控和幂等重试。 |
| `user_preferences` | `user_id`, `theme_color`, `font_mode`, `input_effects`, `settings_json`, `version` | 用户偏好配置。 |
| `user_sessions` | `id`, `user_id`, `refresh_token_hash`, `device_id`, `device_name`, `expires_at`, `revoked_at`, `last_seen_at` | refresh token 会话管理。 |
| `files` | `id`, `owner_user_id`, `bucket`, `object_key`, `mime_type`, `size_bytes`, `etag`, `content_hash`, `url`, `version` | 头像、群头像、帖子图片、聊天附件等文件元数据。 |
| `friend_groups` | `id`, `user_id`, `name`, `sort_order` | 好友分组。 |
| `friendships` | `id`, `user_id`, `friend_user_id`, `remark`, `friend_group_id`, `is_dnd`, `status`, `version` | 好友关系和用户对好友的私有设置。 |
| `friend_requests` | `id`, `from_user_id`, `to_user_id`, `message`, `source_type`, `source_group_id`, `source_friend_id`, `status`, `handled_at` | 好友申请。 |
| `groups` | `id`, `name`, `owner_id`, `avatar_file_id`, `introduction`, `announcement`, `version` | 群基础资料。 |
| `group_members` | `group_id`, `user_id`, `role`, `nickname`, `is_dnd`, `joined_at`, `version` | 群成员、权限和群昵称。 |
| `group_user_settings` | `group_id`, `user_id`, `remark`, `list_group_id`, `list_group_name`, `is_dnd` | 用户对群的私有备注、分类和免打扰设置。 |
| `group_join_requests` | `id`, `group_id`, `actor_user_id`, `operator_user_id`, `message`, `status`, `handled_at` | 入群申请。 |
| `group_events` | `id`, `group_id`, `type`, `actor_user_id`, `operator_user_id`, `payload_json`, `created_at` | 群系统事件，例如退群、设管理员、转让群主。 |
| `conversations` | `id`, `type`, `peer_user_id`, `group_id`, `last_message_id`, `last_message_at`, `version` | 私聊和群聊会话。 |
| `conversation_participants` | `conversation_id`, `user_id`, `is_pinned`, `is_dnd`, `last_read_message_id`, `last_read_at`, `unread_count`, `hidden_at` | 用户维度会话状态。 |
| `messages` | `id`, `conversation_id`, `sender_id`, `message_seq`, `type`, `content_text`, `content_json`, `referenced_message_id`, `client_message_id`, `client_sent_at`, `server_received_at`, `created_at`, `edited_at`, `recalled_at` | 聊天消息。`message_seq` 和正式时间由服务端分配。 |
| `message_attachments` | `id`, `message_id`, `file_id`, `display_name`, `duration_ms`, `width`, `height` | 图片、文件、语音消息附件。 |
| `notifications` | `id`, `user_id`, `type`, `source_id`, `payload_json`, `read_at`, `created_at` | 统一通知盒。 |
| `posts` | `id`, `author_id`, `title`, `content`, `cover_media_id`, `like_count`, `comment_count`, `created_at`, `content_created_at`, `visibility`, `version` | 帖子详情和计数。 |
| `post_media` | `id`, `post_id`, `file_id`, `media_type`, `sort_order`, `width`, `height`, `duration_ms`, `cover_file_id`, `playback_json`, `live_status`, `metadata_json` | 帖子媒体，统一承载图片、视频、实况等资源。 |
| `post_likes` | `post_id`, `user_id`, `created_at` | 帖子点赞。 |
| `post_comments` | `id`, `post_id`, `author_id`, `content`, `like_count`, `reply_count`, `created_at`, `deleted_at` | 一级评论。 |
| `post_comment_replies` | `id`, `comment_id`, `post_id`, `author_id`, `target_user_id`, `target_reply_id`, `content`, `like_count`, `created_at`, `deleted_at` | 评论回复。 |
| `post_comment_likes` | `comment_id`, `user_id`, `created_at` | 评论点赞。 |
| `post_reply_likes` | `reply_id`, `user_id`, `created_at` | 回复点赞。 |
| `follows` | `follower_user_id`, `target_user_id`, `created_at` | 帖子作者关注关系。 |
| `ai_conversations` | `id`, `user_id`, `title`, `last_message_at`, `has_unread_dot`, `model`, `context_used_tokens`, `context_max_tokens` | AI 聊天列表和上下文用量。 |
| `ai_messages` | `id`, `conversation_id`, `role`, `text`, `token_count`, `created_at`, `feedback` | AI 会话消息。 |
| `operation_receipts` | `id`, `user_id`, `client_operation_id`, `operation_type`, `server_result_json`, `created_at` | 幂等操作回执。 |

## 3. REST 接口总表

统一约定：

- Base URL：`/api/v1`
- 鉴权：除注册、登录、刷新 token 外，全部要求 `Authorization: Bearer <access_token>`。
- 分页：普通列表使用 `limit` + `offset`；消息历史支持 `beforeMessageId` / `afterMessageId` / `aroundMessageId` 或 `beforeMessageSeq` / `afterMessageSeq` 游标。
- 缓存校验：资料类 GET 支持 `If-None-Match`，返回 `ETag`；响应体包含 `version` 和 `updatedAt`。
- 幂等：写接口支持 `Idempotency-Key` 或请求体 `clientOperationId`。

### 3.1 Auth / Account

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| POST | `/auth/register` | `email`, `password`, `displayName`, `userId?`, `gender=unspecified` | `user`, `accessToken`, `refreshToken`, `expiresIn` | 未传 `userId` 时后端生成唯一公开 ID。 |
| POST | `/auth/login` | `account`, `password`, `deviceId`, `deviceName` | `user`, `preferences`, `accessToken`, `refreshToken`, `expiresIn` | 账号可为邮箱或公开 ID。 |
| POST | `/auth/refresh` | `refreshToken`, `deviceId` | `accessToken`, `refreshToken`, `expiresIn` | refresh token 轮换。 |
| POST | `/auth/logout` | `refreshToken` | `ok` | 撤销当前设备 refresh token。 |
| GET | `/auth/sessions` | - | `sessions[]` | 查询当前账号会话。 |
| POST | `/auth/sessions/{sessionId}/revoke` | - | `ok` | 踢出指定设备。 |

### 3.2 Current User / Preferences

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| GET | `/me` | - | 当前用户资料，含 `userUuid`, `userId`, `gender`, `genderCustom`, `avatarUrl`, `avatarVersion`, `avatarEtag`, `avatarContentHash` | 返回当前登录用户资料。 |
| PATCH | `/me` | `nickName`, `signature`, `region`, `status`, `gender`, `genderCustom` | 更新后的用户资料 | 保存资料编辑；不在这里修改公开 ID。 |
| GET | `/me/user-id/check` | `userId` | `available`, `reason?` | 检查公开 ID 唯一性和格式。 |
| PATCH | `/me/user-id` | `userId`, `clientOperationId` | 更新后的用户资料，含 `userUuid` 和新 `userId` | 修改公开 ID；后端校验唯一性、格式、冷却时间并写入 `user_id_changes`。 |
| POST | `/me/avatar` | multipart image | `avatarUrl`, `avatarVersion`, `avatarEtag`, `avatarContentHash`, `fileId` | 上传当前用户头像。 |
| GET | `/me/preferences` | - | `themeColor`, `fontMode`, `inputEffects`, `settings`, `version` | 查询用户偏好。 |
| PATCH | `/me/preferences` | 偏好增量字段 | 更新后的偏好 | 更新用户偏好。 |

### 3.3 User / Friend

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| GET | `/users` | `keyword`, `limit`, `offset` | `users[]`, `total` | 按昵称、公开 ID 搜索；响应同时返回 `userUuid` 和 `userId`。 |
| GET | `/users/{userUuid}` | - | 用户详情，含好友关系字段和头像版本字段 | 用不可变 UUID 取详情，支持 ETag。 |
| GET | `/users/by-id/{userId}` | - | 用户详情 | 用公开 ID 查用户。 |
| POST | `/users/batch` | `userUuids[]` | `users[]` | 批量查询用户详情。 |
| GET | `/friends` | `keyword`, `groupId`, `limit`, `offset` | `friends[]`, `total` | 当前用户好友列表。 |
| GET | `/friend-groups` | `keyword` | `groups[]` | 当前用户好友分组。 |
| POST | `/friend-groups` | `name`, `sortOrder` | `group` | 新增好友分组。 |
| PATCH | `/friends/{friendUserUuid}` | `remark`, `groupId`, `isDnd` | 更新后的好友关系 | 保存备注、分组、免打扰。 |
| DELETE | `/friends/{friendUserUuid}` | - | `ok` | 删除好友。 |
| POST | `/friend-requests` | `toUserUuid`, `message`, `sourceType`, `sourceGroupId`, `sourceFriendUuid` | `request` | 发起好友申请。 |
| GET | `/friend-requests` | `status`, `limit`, `offset` | `requests[]`, `unreadCount` | 好友申请列表。 |
| POST | `/friend-requests/{requestId}/accept` | `remark`, `groupId` | `friendship` | 接受好友申请。 |
| POST | `/friend-requests/{requestId}/reject` | - | `request` | 拒绝好友申请。 |

### 3.4 Group

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| GET | `/groups` | `keyword`, `categoryId`, `limit`, `offset` | `groups[]`, `total` | 当前用户群列表、分类列表、搜索。 |
| POST | `/groups` | `name`, `memberIds[]`, `avatarFileId?` | `group`，含头像版本字段 | 创建群聊。 |
| GET | `/groups/{groupId}` | - | 群详情，含当前用户角色、设置、头像版本字段 | 查询群详情。 |
| PATCH | `/groups/{groupId}` | `name`, `introduction`, `announcement`, `avatarFileId?` | `group` | 群主/管理员编辑群资料。 |
| DELETE | `/groups/{groupId}` | - | `ok` | 解散群或退出群，按权限裁决。 |
| GET | `/groups/{groupId}/members` | `keyword`, `limit`, `offset` | `members[]`, `total` | 群成员分页。 |
| POST | `/groups/{groupId}/members` | `userUuids[]` | `members[]` | 邀请/添加成员。 |
| DELETE | `/groups/{groupId}/members/{userUuid}` | - | `ok` | 移除成员或退群。 |
| PATCH | `/groups/{groupId}/members/{userUuid}` | `role`, `nickname`, `isDnd` | `member` | 设置管理员、修改群昵称、免打扰。 |
| POST | `/groups/{groupId}/transfer-owner` | `userUuid` | `group` | 转让群主。 |
| PATCH | `/groups/{groupId}/my-settings` | `remark`, `listGroupId`, `listGroupName`, `isDnd` | `settings` | 当前用户群备注、分类、免打扰。 |
| POST | `/group-join-requests` | `groupId`, `message` | `request` | 申请入群。 |
| GET | `/group-notifications` | `limit`, `offset` | `notifications[]`, `unreadCount` | 群通知列表。 |
| POST | `/group-join-requests/{requestId}/accept` | `remark`, `categoryId` | `group`, `member` | 接受入群申请。 |
| POST | `/group-join-requests/{requestId}/reject` | - | `request` | 拒绝入群申请。 |

### 3.5 Chat

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| GET | `/conversations` | `keyword`, `limit`, `offset` | `conversations[]`，每项含 `summary`、`syncState`、`lastMessage` | 查询当前用户会话列表和未读状态。 |
| POST | `/conversations/direct` | `peerUserUuid` | `conversation` | 打开或创建私聊。 |
| POST | `/conversations/group` | `groupId` | `conversation` | 打开群会话。 |
| GET | `/conversations/{conversationId}` | - | `ConversationMeta` | 查询会话元数据。 |
| PATCH | `/conversations/{conversationId}/settings` | `isPinned`, `isDnd`, `hidden` | `ConversationSyncState` | 置顶、免打扰、移除会话。 |
| POST | `/conversations/{conversationId}/read` | `lastReadMessageId`, `lastReadAt?` | `ConversationSyncState`，`unreadCount=0` | 标记已读；服务端按 `lastReadMessageId` 对齐。 |
| POST | `/conversations/{conversationId}/unread` | `lastReadMessageId?`, `unreadCount?` | `ConversationSyncState` | 手动标未读。 |
| GET | `/conversations/{conversationId}/messages` | `beforeMessageId?`, `afterMessageId?`, `aroundMessageId?`, `beforeMessageSeq?`, `afterMessageSeq?`, `limit` | `messages[]`, `hasMoreBefore`, `hasMoreAfter`, `syncCursor` | 历史消息分页、断线补偿和定位引用消息。 |
| POST | `/conversations/{conversationId}/messages` | `clientMessageId`, `clientSentAt?`, `type`, `content`, `attachments`, `referencedMessageId` | `message` | 发送消息；服务端落库后分配 `messageSeq` 和服务端时间。 |
| POST | `/conversations/{conversationId}/messages/{messageId}/recall` | - | `message` | 撤回消息。 |
| DELETE | `/conversations/{conversationId}/messages` | - | `ok` | 清空当前用户视角的会话记录或隐藏会话历史。 |

#### 3.5.1 Chat 一致性

- 未读状态以后端 `conversation_participants` 为权威，按 `user_id + conversation_id` 持久化。任一设备已读、手动标未读或收到新消息导致未读变化后，服务端更新 `last_read_message_id`、`last_read_at`、`unread_count`，并通过 WebSocket 推送给该用户其它在线设备。
- 消息完整历史以后端为准。服务端必须保证同一会话内 `message_seq` 单调递增，并支持通过 `messageSeq` / `messageId` 补齐缺口。
- WebSocket 正常在线时推送 `chat.message.created`；断线恢复优先用 `lastEventId` 补事件，超出事件保留窗口时返回 `sync.required`，调用方再用 REST 分页补偿。
- 消息正式时间以后端接收、校验、幂等去重并成功落库后的时间为准。`clientSentAt` 只用于诊断和排查，不参与最终排序和未读计算。

### 3.6 Post / Comment

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| GET | `/posts` | `offset`, `limit`, `followOnly` | `posts[]`, `total`；每个帖子含 `mediaPreview[]` 和 `viewer` | 帖子流。 |
| POST | `/posts` | `title`, `content`, `media[]`, `visibility` | `post` | 发布帖子；`media[]` 项包含 `fileId`, `mediaType`, `sortOrder`, `coverFileId?`, `metadata?`。 |
| GET | `/posts/{postId}` | - | `PostDetailData`，含完整 `media[]` 和 `viewer` | 帖子详情。 |
| PATCH | `/posts/{postId}` | `title`, `content`, `media[]`, `visibility` | `post` | 作者编辑；媒体按 `media[]` 全量顺序更新或按后端约定增量更新。 |
| DELETE | `/posts/{postId}` | - | `ok` | 作者删除。 |
| POST | `/posts/{postId}/like` | `liked` | `likeCount`, `isLiked` | 点赞或取消点赞。 |
| POST | `/users/{authorUuid}/follow` | `followed` | `isFollowedAuthor` | 关注或取消关注作者。 |
| GET | `/posts/{postId}/comments` | `offset`, `limit` | `comments[]`, `total`, `hasMore`；每条评论和回复含 `viewer` | 帖子评论列表。 |
| POST | `/posts/{postId}/comments` | `content` | `comment`, `postCommentCount` | 新增一级评论。 |
| POST | `/comments/{commentId}/like` | `liked` | `likeCount`, `isLiked` | 评论点赞或取消点赞。 |
| POST | `/comments/{commentId}/replies` | `content`, `targetUserUuid?`, `targetReplyId?` | `reply` | 新增回复。 |
| POST | `/replies/{replyId}/like` | `liked` | `likeCount`, `isLiked` | 回复点赞或取消点赞。 |
| DELETE | `/comments/{commentId}` | - | `ok` | 删除评论。 |
| DELETE | `/replies/{replyId}` | - | `ok` | 删除回复。 |

### 3.7 AI Chat

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| GET | `/ai/conversations` | `offset`, `limit` | `entries[]` | 查询 AI 会话列表。 |
| POST | `/ai/conversations` | `title?`, `firstMessage?` | `conversation` | 新建 AI 会话。 |
| GET | `/ai/conversations/{conversationId}/messages` | - | `messages[]` | 查询 AI 会话消息。 |
| POST | `/ai/conversations/{conversationId}/messages` | `message`, `clientMessageId` | `userMessage`, `assistantMessageId`, `streamId` | 触发 AI 回复，正文可用 SSE 或 WS 流式返回。 |
| GET | `/ai/conversations/{conversationId}/context` | - | `usedTokens`, `maxTokens`, `available` | 查询上下文用量。 |
| POST | `/ai/messages/{messageId}/feedback` | `feedback=liked/disliked/none` | `message` | AI 消息反馈。 |
| POST | `/ai/title` | `firstUserMessage` | `title` | 根据首条用户消息生成标题。 |

### 3.8 File / Notification / Operation Receipt

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| POST | `/files` | multipart file, `bucket` | `fileId`, `url`, `etag`, `contentHash`, `version` | 上传头像、帖子图、聊天附件等文件。 |
| GET | `/files/{fileId}` | - | 文件元数据或 302 到对象存储 | 查询文件或跳转下载。 |
| GET | `/notifications` | `type`, `limit`, `offset` | `notifications[]`, `unreadCount` | 统一通知入口。 |
| POST | `/notifications/read-all` | `type?` | `ok`, `unreadCount=0` | 按类型或全部标记已读。 |
| POST | `/notifications/{notificationId}/read` | - | `notification` | 单条通知已读。 |
| POST | `/operations/receipt` | `clientOperationId`, `operationType`, `payload` | `status`, `serverResult` | 幂等操作回执查询或补偿。 |

## 4. Token 验证机制

| 项 | 设计 |
| --- | --- |
| Access Token | JWT，短有效期建议 15 分钟；只放 `sub=userUuid`、`sid=sessionId`、`deviceId`、`iat`、`exp`、`scope`、`tokenVersion`。 |
| Refresh Token | 高熵随机字符串，服务端只存 hash，建议 30 天有效；每次刷新后轮换，旧 refresh token 立即失效。 |
| 服务端验证 | 校验签名、`exp`、`sid` 是否存在且未撤销、用户是否禁用、`tokenVersion` 是否匹配。 |
| 权限校验 | 资料修改只允许本人；好友关系按 `friendships.user_id`；群管理操作按 `group_members.role`；消息发送必须是会话参与者。 |
| WebSocket 鉴权 | 建连时使用 `Authorization: Bearer <access_token>` 或 `?token=`；服务端验证后绑定 `userUuid/sessionId/deviceId`。 |
| 续期流程 | REST 返回 401 且错误码 `TOKEN_EXPIRED`；调用 `/auth/refresh` 成功后获得新 token，失败则必须重新登录。 |
| 撤销策略 | 修改密码、退出登录、踢设备时撤销对应 `user_sessions`；必要时递增 `users.token_version` 让旧 token 全失效。 |
| 防重放 | 写接口使用 `Idempotency-Key` / `clientOperationId`；消息发送额外使用 `clientMessageId` 去重。 |

错误码建议：

| HTTP | code | 场景 |
| --- | --- | --- |
| 401 | `TOKEN_MISSING` | 未携带 token。 |
| 401 | `TOKEN_INVALID` | 签名错误、格式错误、session 不存在。 |
| 401 | `TOKEN_EXPIRED` | access token 过期，可尝试 refresh。 |
| 401 | `REFRESH_TOKEN_INVALID` | refresh token 失效，需要重新登录。 |
| 403 | `PERMISSION_DENIED` | 用户无权操作资源。 |
| 409 | `VERSION_CONFLICT` | 资料版本冲突，需要重新拉取。 |

## 5. WebSocket 需求

### 5.1 连接

| 项 | 要求 |
| --- | --- |
| URL | `wss://<host>/ws` |
| 鉴权 | 握手携带 access token；失败返回 401 并关闭。 |
| 心跳 | 连接方每 25 秒 `ping`，服务端 10 秒内 `pong`；连续 2 次失败应断开重连。 |
| 恢复 | 连接方保存 `lastEventId`，重连后发送 `resume`；服务端补发最近事件，超出保留窗口则返回 `sync.required`。 |
| 顺序 | 每个用户连接收到的事件带全局递增 `eventId` 和服务端 `createdAt`；同一 conversation 内消息按服务端分配的 `messageSeq` 排序。 |

### 5.2 服务端接收事件

| event | payload | 返回/广播 | 说明 |
| --- | --- | --- | --- |
| `ping` | `timestamp` | `pong` | 心跳。 |
| `resume` | `lastEventId` | `event.replay` 或 `sync.required` | 断线恢复。 |
| `chat.message.send` | `conversationId`, `clientMessageId`, `clientSentAt?`, `type`, `content`, `attachments`, `referencedMessageId` | `chat.message.ack`，并向参与者广播 `chat.message.created` | 实时发送消息；ack 返回服务端消息 ID、`messageSeq` 和服务端时间。 |
| `chat.message.recall` | `conversationId`, `messageId` | `chat.message.recalled` | 撤回。 |
| `chat.read` | `conversationId`, `lastReadMessageId`, `lastReadAt` | `chat.read.updated` | 已读同步。 |
| `presence.update` | `status` | `presence.updated` | 在线状态。 |
| `ai.message.send` | `conversationId`, `clientMessageId`, `message` | `ai.stream.chunk`, `ai.stream.done` | AI 流式回复。 |
| `ai.stream.cancel` | `streamId` | `ai.stream.cancelled` | 停止 AI 回复。 |

### 5.3 服务端推送事件

| event | payload | 语义 |
| --- | --- | --- |
| `chat.message.created` | `conversation`, `message`, `sender`, `eventId`, `messageSeq`, `serverReceivedAt` | 新消息已创建；同时更新会话摘要和未读状态。 |
| `chat.message.recalled` | `conversationId`, `messageId`, `replacementMessage` | 消息已撤回。 |
| `chat.conversation.updated` | `conversationId`, `summary`, `syncState` | 会话摘要、置顶、免打扰、最后消息或未读状态变化。 |
| `chat.read.updated` | `conversationId`, `userUuid`, `lastReadMessageId`, `unreadCount` | 已读状态变化。 |
| `friend.request.created` | `request`, `notification` | 好友申请新增。 |
| `friend.request.updated` | `request`, `friendship?` | 好友申请状态变化。 |
| `friendship.updated` | `friendUserUuid`, `friendship`, `user` | 好友关系或好友资料变化。 |
| `group.updated` | `groupId`, `group`, `version` | 群资料变化。 |
| `group.member.updated` | `groupId`, `member`, `operation` | 群成员、权限或昵称变化。 |
| `group.notification.created` | `notification` | 群通知新增。 |
| `post.updated` | `postId`, `likeCount`, `commentCount`, `version` | 帖子计数或版本变化。 |
| `notification.created` | `notification`, `unreadCount` | 通知新增。 |
| `notification.read.updated` | `type`, `unreadCount` | 通知已读状态变化。 |
| `presence.updated` | `userUuid`, `status`, `lastSeenAt` | 在线状态变化。 |
| `profile.updated` | `userUuid`, `userId`, `profile`, `avatarUrl`, `avatarVersion`, `avatarEtag`, `avatarContentHash` | 用户资料、公开 ID 或头像变化。 |
| `preference.updated` | `preferences`, `version` | 用户偏好变化。 |
| `sync.required` | `scope`, `reason` | 事件无法完整补发，需要通过 REST 重新拉取对应范围。 |

## 6. 响应模型字段建议

资料类响应应统一包含：

```json
{
  "userUuid": "018f4f7e-7a1e-7c3a-9f5b-1d5f6b2d9a11",
  "userId": "u007",
  "gender": "unspecified",
  "genderCustom": "",
  "version": 12,
  "etag": "\"user-u007-v12\"",
  "updatedAt": "2026-05-31T10:20:30.000Z"
}
```

头像字段必须直接返回 URL，同时返回版本和校验信息：

```json
{
  "avatarUrl": "https://cdn.example.com/avatar/u007/v12.png",
  "avatarVersion": 12,
  "avatarEtag": "\"file-avatar-u007-v12\"",
  "avatarContentHash": "sha256:..."
}
```

帖子列表和详情统一返回 `media[]`，图片、视频、实况等媒体都走同一结构：

```json
{
  "media": [
    {
      "mediaId": "pm_001",
      "mediaType": "image",
      "url": "https://cdn.example.com/posts/p001/1.jpg",
      "thumbnailUrl": "https://cdn.example.com/posts/p001/1_thumb.jpg",
      "width": 1280,
      "height": 720,
      "durationMs": 0,
      "version": 3,
      "etag": "\"post-media-pm001-v3\"",
      "contentHash": "sha256:..."
    }
  ]
}
```

帖子、评论、回复响应必须包含当前登录用户视角的 `viewer` 字段：

```json
{
  "postId": "p001",
  "likeCount": 12,
  "commentCount": 3,
  "viewer": {
    "isLiked": true,
    "isAuthor": false,
    "isFollowedAuthor": true
  }
}
```

列表分页响应统一：

```json
{
  "items": [],
  "offset": 0,
  "limit": 20,
  "total": 100,
  "hasMore": true
}
```

错误响应统一：

```json
{
  "code": "PERMISSION_DENIED",
  "message": "无权操作该群",
  "requestId": "req_..."
}
```

## 7. 实施优先级

| 优先级 | 后端能力 | 原因 |
| --- | --- | --- |
| P0 | Auth 注册/登录/刷新、`/me`、`/me/preferences`、token 验证 | 所有登录态业务依赖账号、鉴权和当前用户资料。 |
| P0 | 用户/好友/群详情 GET、头像文件元数据 | 基础社交关系和资料展示依赖这些接口。 |
| P0 | WebSocket 鉴权、消息发送/接收、历史消息分页 | 聊天是实时核心功能，必须先保证消息可靠性和补偿拉取。 |
| P1 | 好友申请、群申请、通知未读 | 社交关系变更和通知需要后端统一裁决。 |
| P1 | 群成员和权限操作 | 敏感操作必须由服务端校验权限。 |
| P1 | 文件上传和文件版本 | 头像、帖子图和聊天附件都依赖文件服务。 |
| P2 | 帖子流、帖子详情、点赞、评论、回复 | 内容社区能力。 |
| P2 | AI 聊天会话、消息、流式回复 | AI 会话能力。 |
| P3 | 幂等操作回执和批量补偿同步 | 网络不稳定和多端一致性增强。 |
