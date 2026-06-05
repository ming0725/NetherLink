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
| `files` | `id`, `owner_user_id`, `original_file_name`, `bucket`, `object_key`, `mime_type`, `size_bytes`, `etag`, `content_hash`, `storage_status`, `visibility`, `version` | MinIO/S3 对象元数据。对象路径只在后端内部使用，API 不直接暴露 `object_key`。 |
| `file_variants` | `id`, `file_id`, `variant_type`, `bucket`, `object_key`, `mime_type`, `size_bytes`, `width`, `height`, `duration_ms`, `etag`, `content_hash` | 缩略图、转码视频、海报图、头像裁剪图等派生文件。 |
| `file_usages` | `id`, `file_id`, `usage_type`, `business_id`, `owner_user_id`, `visibility`, `created_at`, `deleted_at` | 文件被头像、聊天、帖子、AI 会话等业务引用的关系，用于权限、清理和审计。 |
| `file_upload_sessions` | `id`, `user_id`, `target_type`, `status`, `upload_mode`, `s3_upload_id`, `part_size_bytes`, `total_parts`, `uploaded_parts_json`, `expected_mime_type`, `expected_size_bytes`, `expires_at`, `client_operation_id` | 直传对象存储前的上传会话、分片上传状态和幂等控制。 |
| `file_processing_jobs` | `id`, `file_id`, `job_type`, `status`, `result_json`, `error_code`, `started_at`, `finished_at` | 图片压缩、视频转码、实况拆分、AI 文件解析等异步处理任务。 |
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
| `ai_uploaded_files` | `id`, `conversation_id`, `message_id`, `file_id`, `purpose`, `parse_status`, `extracted_text_file_id`, `token_count`, `retention_policy`, `expires_at` | AI 会话上传文件、解析结果和保留策略。 |
| `ai_file_chunks` | `id`, `ai_file_id`, `chunk_index`, `content`, `token_count`, `embedding_vector`, `metadata_json` | AI 文件解析后的文本片段；需要检索增强时可用 pgvector 或外部向量库。 |
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
| PATCH | `/ai/conversations/{conversationId}` | `title`, `clientOperationId` | `conversation` | 修改 AI 会话标题。 |
| DELETE | `/ai/conversations/{conversationId}` | `clientOperationId` | `ok` | 删除 AI 会话。 |
| GET | `/ai/conversations/{conversationId}/messages` | - | `messages[]` | 查询 AI 会话消息。 |
| POST | `/ai/conversations/{conversationId}/messages` | `message`, `clientMessageId`, `aiFileIds?` | `userMessage`, `assistantMessageId`, `streamId`, `usedFiles[]`, `title?` | 触发 AI 回复，正文可用 SSE 或 WS 流式返回；首轮完成后可返回正式标题。 |
| POST | `/ai/conversations/{conversationId}/files` | `fileId`, `purpose=reference/attachment`, `retentionPolicy?` | `aiFile`, `parseStatus` | 将已上传文件绑定到 AI 会话并启动解析。 |
| GET | `/ai/conversations/{conversationId}/files` | - | `files[]` | 查询当前 AI 会话可引用文件和解析状态。 |
| GET | `/ai/files/{aiFileId}` | - | `aiFile`, `parseStatus`, `tokenCount`, `previewText?` | 查询 AI 文件解析结果摘要。 |
| DELETE | `/ai/files/{aiFileId}` | - | `ok` | 从 AI 会话移除文件；按保留策略决定是否清理原文件。 |
| GET | `/ai/conversations/{conversationId}/context` | - | `usedTokens`, `maxTokens`, `available` | 查询上下文用量。 |
| POST | `/ai/messages/{messageId}/feedback` | `feedback=liked/disliked/none` | `message` | AI 消息反馈。 |

### 3.8 File / Notification / Operation Receipt

| 方法 | 路径 | 请求 | 响应 | 说明 |
| --- | --- | --- | --- | --- |
| POST | `/files/upload-session` | `targetType`, `fileName`, `mimeType`, `sizeBytes`, `contentHash?`, `uploadMode=auto/single/multipart`, `clientOperationId?` | `uploadSessionId`, `fileId`, `uploadMode`, `partSizeBytes?`, `totalParts?`, `uploadUrl?`, `headers?`, `expiresAt` | 创建 MinIO/S3 预签名上传会话；小文件单 PUT，大文件分片断点续传。 |
| GET | `/files/upload-session/{uploadSessionId}` | - | `status`, `uploadMode`, `partSizeBytes`, `totalParts`, `uploadedParts[]`, `expiresAt` | 查询上传会话状态，用于断点续传和补偿。 |
| POST | `/files/upload-session/{uploadSessionId}/parts` | `partNumbers[]` | `parts[]`，每项含 `partNumber`, `uploadUrl`, `headers`, `expiresAt` | 为指定分片签发短时效上传 URL；仅 `multipart` 模式使用。 |
| POST | `/files/upload-session/{uploadSessionId}/complete` | `etag?`, `parts?`, `contentHash?` | `fileId`, `url`, `etag`, `contentHash`, `version`, `processingStatus` | 客户端直传完成后确认；单 PUT 传 `etag`，分片上传传 `parts[]`。 |
| POST | `/files/upload-session/{uploadSessionId}/abort` | - | `ok` | 取消未完成上传，并调用 MinIO Abort Multipart Upload 或删除临时对象。 |
| POST | `/files` | multipart/form-data file, `targetType` | `fileId`, `url`, `etag`, `contentHash`, `version`, `processingStatus` | 小文件或不支持直传时走后端转存到 MinIO。 |
| GET | `/files/{fileId}` | - | 文件元数据、变体和受控下载 URL | 查询文件元数据；私有文件返回短时效签名 URL，不直接暴露对象路径。 |
| GET | `/files/{fileId}/download-url` | `variant?` | `url`, `expiresAt`, `headers?` | 获取短时效下载 URL。 |
| DELETE | `/files/{fileId}` | - | `ok` | 仅允许删除未被业务引用或当前用户有权删除的文件。 |
| GET | `/notifications` | `type`, `limit`, `offset` | `notifications[]`, `unreadCount` | 统一通知入口。 |
| POST | `/notifications/read-all` | `type?` | `ok`, `unreadCount=0` | 按类型或全部标记已读。 |
| POST | `/notifications/{notificationId}/read` | - | `notification` | 单条通知已读。 |
| POST | `/operations/receipt` | `clientOperationId`, `operationType`, `payload` | `status`, `serverResult` | 幂等操作回执查询或补偿。 |

## 4. 文件存储需求

文件服务基于 MinIO，使用 S3 兼容 API。后端必须把 MinIO 当作对象存储底座，而不是把对象路径暴露给调用方。业务系统只持有 `files.id`、公开 URL 或短时效签名 URL；对象桶名、`object_key`、内部 endpoint、access key 只存在于后端配置和数据库元数据中。

实现上建议在后端抽象 `ObjectStorageProvider`，第一实现固定为 MinIO；只允许使用 S3 兼容操作，例如 `PUT Object`、`GET Object`、`HEAD Object`、`Delete Object`、presigned URL 和 multipart upload。后续如迁移到云厂商 S3 兼容存储，不应影响业务表结构和 API。

### 4.1 存储范围

| 文件类型 | 是否进入 MinIO | 业务表关联 | 访问策略 | 处理要求 |
| --- | --- | --- | --- | --- |
| 用户头像 | 是 | `users.avatar_file_id` + `file_usages(avatar)` | 公开读或 CDN 公开读 | 校验图片格式，生成标准头像、缩略图，更新 `avatarVersion`。 |
| 群头像 | 是 | `groups.avatar_file_id` + `file_usages(group_avatar)` | 群成员可读，公开群可公开读 | 同用户头像。 |
| 聊天图片/语音/普通文件 | 是 | `message_attachments.file_id` + `file_usages(chat_attachment)` | 仅会话参与者可读 | 图片生成缩略图；语音记录时长；普通文件保留原始文件名和 MIME。 |
| 聊天视频 | 是 | `message_attachments.file_id` | 仅会话参与者可读 | 生成封面图、低码率预览或转码结果。 |
| 帖子图片 | 是 | `post_media.file_id` + `file_usages(post_media)` | 按帖子可见性裁决 | 生成多尺寸缩略图，保留宽高和内容 hash。 |
| 帖子视频 | 是 | `post_media.file_id` + `cover_file_id` | 按帖子可见性裁决 | 生成封面、HLS/MP4 转码变体、时长和分辨率元数据。 |
| 帖子实况 | 是 | `post_media.file_id` + `cover_file_id` + `metadata_json` | 按帖子可见性裁决 | 按产品形态保存为图片+短视频组合，或保存原始 Live Photo 包并生成可播放变体。 |
| AI 对话上传文件 | 是，默认先保存原文件 | `ai_uploaded_files.file_id` | 仅上传者和该 AI 会话可读 | 异步解析为文本/结构化片段；模型调用只读取解析结果和必要片段。 |
| AI 解析结果文本 | 是或数据库保存，按大小决定 | `ai_uploaded_files.extracted_text_file_id` 或 `ai_file_chunks` | 仅上传者和该 AI 会话可读 | 小文本可入库，大文本存 MinIO 并切片入 `ai_file_chunks`。 |
| 临时上传文件 | 是 | `file_upload_sessions` | 上传者短时可写，默认不可读 | 未完成或未绑定业务的对象定时清理。 |

### 4.2 MinIO / S3 约定

建议至少拆分以下 bucket，生产环境可通过 CDN 或网关映射公开读资源：

| bucket | 用途 | 默认权限 |
| --- | --- | --- |
| `netherlink-public` | 公开头像、公开帖子缩略图等可公开缓存资源 | 通过 CDN 或网关公开读。 |
| `netherlink-protected` | 登录后按权限访问的聊天文件、私密帖子媒体、群头像 | 后端签发短时效下载 URL。 |
| `netherlink-private` | AI 上传原文件、AI 解析结果、审核材料、内部处理产物 | 仅后端服务账号访问。 |
| `netherlink-tmp` | 上传中、未确认、转码中间文件 | 短生命周期，定时清理。 |

对象 key 由后端生成，不能信任客户端文件名。推荐格式：

```text
<target_type>/<yyyy>/<mm>/<owner_user_uuid>/<file_uuid>/<variant_or_original>
```

示例：

```text
avatar/2026/06/018f.../file_018f.../original.png
post-media/2026/06/018f.../file_018f.../thumb_720.jpg
ai-upload/2026/06/018f.../file_018f.../original.pdf
```

必须保存并校验：

- `mime_type`：以后端 sniff 结果为准，客户端传值只作参考。
- `size_bytes`：超过目标类型限制时拒绝。
- `etag`：对象存储返回的 ETag，用于缓存和完整性辅助判断。
- `content_hash`：建议使用 SHA-256，防重复上传、秒传和安全扫描。
- `storage_status`：`pending`、`uploaded`、`processing`、`ready`、`failed`、`deleted`。
- `visibility`：`public`、`authenticated`、`participants`、`owner_private`、`system_private`。

`targetType` 建议固定枚举：

```text
avatar
group_avatar
chat_image
chat_video
chat_audio
chat_file
post_image
post_video
post_live
ai_upload
```

### 4.3 上传流程

上传分三种模式：

| 模式 | 适用场景 | 后端行为 |
| --- | --- | --- |
| `single` | 小文件，建议 64 MiB 以下 | 返回一个预签名 `PUT Object` URL，客户端一次性直传。 |
| `multipart` | 大文件、弱网、需要暂停/恢复的文件 | 后端创建 MinIO multipart upload，按分片签发 `UploadPart` URL。 |
| `proxy` | 客户端不支持直传或极小文件 | 客户端用 `multipart/form-data` 上传到后端，后端转存 MinIO。 |

基础流程：

1. 客户端调用 `/files/upload-session`，传 `targetType`、`mimeType`、`sizeBytes`、`contentHash?`、`uploadMode`。
2. 后端校验登录态、业务上限、MIME 白名单和用户配额，创建 `files` 与 `file_upload_sessions`。
3. `single` 模式直接返回预签名 `PUT` URL；`multipart` 模式返回 `partSizeBytes`、`totalParts`，但分片 URL 可按需单独申请。
4. 客户端上传到 MinIO；上传 URL 过期时间建议 5-15 分钟，过期后重新申请 URL，不重新创建业务文件。
5. 客户端调用 `/files/upload-session/{id}/complete`，后端校验对象、写入元数据并启动处理任务。
6. 后端把文件状态改为 `uploaded` 或 `processing`，按 `targetType` 创建处理任务。
7. 处理完成后写入 `file_variants`、业务元数据和 `file_usages`，状态变为 `ready`，必要时通过 WebSocket 推送业务资源更新。

上传完成但没有被业务绑定的文件必须进入临时保留区，建议 24 小时内自动清理。业务删除不一定立即物理删除对象，应先移除 `file_usages`，再由后台任务确认无引用后删除对象或转入冷存储。

### 4.4 大文件分片和断点续传

大文件上传必须支持 MinIO/S3 multipart upload，满足视频、实况、聊天大附件和 AI 大文档的弱网场景。

分片规则：

- 触发阈值：建议文件大于 64 MiB 自动使用 `multipart`，客户端也可显式请求。
- 分片大小：默认 8-16 MiB；后端必须保证除最后一片外，每片不小于 S3 multipart 最小限制 5 MiB。
- 分片数量：必须不超过 S3 上限 10000 片；超出时后端自动增大 `partSizeBytes`。
- 分片编号：从 1 开始，按 `partNumber` 顺序完成；最后完成时 `parts[]` 必须按升序提交。
- 并发上传：客户端可并发上传 3-6 个分片；后端可按用户、IP、文件类型限制并发和速率。
- 分片 URL：每个分片使用独立短时效预签名 URL；URL 过期只影响该分片，不影响上传会话。

断点续传要求：

1. 客户端本地保存 `uploadSessionId`、`fileId`、`contentHash`、`partSizeBytes`、`totalParts` 和已上传分片的 `partNumber + etag`。
2. 应用重启、网络恢复或 URL 过期后，客户端调用 `GET /files/upload-session/{id}` 获取服务端已知的 `uploadedParts[]`。
3. 客户端只为缺失分片调用 `/files/upload-session/{id}/parts` 重新签发 URL，然后继续上传。
4. 同一个分片重复上传必须是幂等的；后端以最后一次 MinIO 返回的 ETag 为准，并更新 `uploaded_parts_json`。
5. 客户端调用 complete 时提交完整 `parts[]`，每项至少包含 `partNumber`、`etag`、`sizeBytes?`。
6. 后端调用 MinIO `CompleteMultipartUpload` 后必须执行 `HEAD Object`，校验最终大小、MIME、业务状态和 `contentHash?`。

`contentHash` 推荐由客户端在上传前计算 SHA-256。对于超大文件，如果一次性 hash 成本过高，允许客户端先不传；后端完成后异步计算并更新 `files.content_hash`。需要秒传、去重、AI 审计或安全扫描的目标类型应要求最终必须有 `contentHash`。

上传会话状态建议：

```text
created
uploading
completing
uploaded
processing
ready
failed
aborted
expired
```

会话过期策略：

- 未完成上传会话默认 24 小时过期，大视频可放宽到 72 小时。
- 过期会话不能 complete；客户端必须重新创建上传会话。
- 后台任务需要定期调用 MinIO `AbortMultipartUpload` 清理过期 multipart upload，避免残留分片占用存储。
- 用户主动取消时调用 `/files/upload-session/{id}/abort`，后端清理 MinIO 未完成分片和临时元数据。

错误处理：

- 单个分片上传失败时客户端只重试该分片，不重传整个文件。
- complete 发现缺片时返回 `UPLOAD_PART_MISSING`，响应里带缺失 `partNumbers[]`。
- complete 发现 ETag 不匹配时返回 `UPLOAD_PART_ETAG_MISMATCH`，客户端重新上传对应分片。
- 上传会话过期返回 `UPLOAD_SESSION_EXPIRED`。
- MinIO multipart upload 已被清理但业务会话仍存在时，返回 `UPLOAD_SESSION_INVALID`，客户端重新创建会话。

### 4.5 访问控制

文件下载必须先校验业务权限：

- 头像：公开资料可读，但仍应通过 CDN URL 或后端可控 URL 返回。
- 聊天附件：只有 `conversation_participants` 中的参与者可读；用户退出群后的历史访问策略由产品决定，后端必须可配置。
- 帖子媒体：按 `posts.visibility`、作者、关注关系和删除状态裁决。
- AI 上传文件：只允许上传者在对应 AI 会话中引用；不能被普通聊天、帖子或其它 AI 会话默认复用，除非用户显式重新绑定。
- 管理后台和安全扫描服务使用独立服务账号，不复用用户下载 URL。

公开 API 不返回 MinIO endpoint、bucket、object key、永久 access key。私有文件只返回短时效签名 URL，建议有效期 1-10 分钟；公开资源可返回 CDN URL，但仍需带 `etag`、`version`、`contentHash` 便于缓存失效。

### 4.6 AI 上传文件设计

AI 对话上传文件不建议“只解析后直接拼进提示词且不保存”。推荐默认流程是保存原文件、解析、切片、按需注入提示词：

1. 用户先通过 `/files/upload-session` 或 `/files` 上传文件，`targetType=ai_upload`。
2. 客户端调用 `/ai/conversations/{conversationId}/files` 把 `fileId` 绑定到 AI 会话。
3. 后端创建 `ai_uploaded_files`，启动 `file_processing_jobs(job_type=ai_parse)`。
4. 解析器按 MIME 分流：PDF、txt、Markdown、docx、xlsx、图片 OCR、代码文件等分别处理。
5. 小文件解析文本可直接写入 `ai_file_chunks.content`；大文件的完整抽取文本存为 `extracted_text_file_id`，再把摘要和切片入库。
6. 发送 AI 消息时，客户端传 `aiFileIds`；服务端只把相关摘要、命中的 chunks 或用户选中的文件片段加入模型上下文。
7. 若文件还在解析，消息接口返回 `AI_FILE_NOT_READY`，或按请求参数允许“仅使用文件名/摘要继续”。

这种设计的原因：

- 模型上下文有限，不能把大文件原文无条件拼进 prompt。
- 原文件需要用于重新解析、审计、下载、用户复查和不同模型策略复用。
- 异步解析失败可以重试，不要求用户重新上传。
- 可以通过 `retention_policy` 控制隐私：`conversation` 随会话保留、`temporary` 到期删除、`source_deleted_after_parse` 解析成功后删除原文件但保留抽取文本。

AI 文件默认保留策略建议：

| 策略 | 原文件 | 解析文本/切片 | 适用场景 |
| --- | --- | --- | --- |
| `conversation` | 随 AI 会话保留 | 随 AI 会话保留 | 默认策略，方便后续追问。 |
| `temporary` | 到期删除 | 到期删除 | 临时问答、敏感文件。 |
| `source_deleted_after_parse` | 解析成功后删除 | 保留到会话删除或到期 | 用户不希望长期保存原文件，但接受保存文本特征。 |
| `none` | 不允许持久化 | 不允许持久化 | 仅适合很小文本输入；后端不应把二进制文件直接塞进 prompt。 |

当 `retention_policy=none` 时，只允许纯文本且大小受限，服务端可在请求生命周期内解析并注入 prompt，不写 MinIO；但仍要写最小审计记录，例如文件名、大小、hash、处理时间和用户确认状态。

### 4.7 清理、扫描和限制

- 所有上传文件必须做 MIME 白名单、大小限制、扩展名规范化和恶意内容扫描。
- 图片需剥离不必要 EXIF，避免泄露地理位置；保留方向信息或转正后再生成变体。
- 视频转码和实况处理必须异步，不阻塞发帖主流程；帖子可先处于 `processing` 状态。
- 文件引用必须通过 `file_usages` 统计，后台任务只删除无引用且过保留期的对象。
- 配额至少按用户维度统计：总容量、单文件大小、每日上传流量、AI 解析 token 或页数。
- 删除用户或会话时，应根据合规要求软删元数据、撤销访问 URL，并异步物理删除对象。
- MinIO bucket 应开启版本控制或对象锁的取舍由部署环境决定；业务层仍以 `files.version` 和 `file_variants` 为准。

## 5. Token 验证机制

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
| 413 | `FILE_TOO_LARGE` | 文件超过目标类型或用户配额限制。 |
| 415 | `FILE_TYPE_NOT_ALLOWED` | MIME、扩展名或文件内容不符合白名单。 |
| 409 | `FILE_NOT_READY` | 文件仍在上传、处理或转码，暂不可绑定业务。 |
| 410 | `UPLOAD_SESSION_EXPIRED` | 上传会话已过期，需要重新创建。 |
| 409 | `UPLOAD_SESSION_INVALID` | MinIO multipart upload 不存在或已被清理，需要重新创建。 |
| 409 | `UPLOAD_PART_MISSING` | 分片上传 complete 时缺少必要分片。 |
| 409 | `UPLOAD_PART_ETAG_MISMATCH` | 分片 ETag 与对象存储记录不一致。 |
| 409 | `AI_FILE_NOT_READY` | AI 上传文件尚未解析完成，暂不可用于模型上下文。 |
| 422 | `AI_FILE_PARSE_FAILED` | AI 文件解析失败，需要用户删除、重试或换格式上传。 |

## 6. WebSocket 需求

### 6.1 连接

| 项 | 要求 |
| --- | --- |
| URL | `wss://<host>/ws` |
| 鉴权 | 握手携带 access token；失败返回 401 并关闭。 |
| 心跳 | 连接方每 25 秒 `ping`，服务端 10 秒内 `pong`；连续 2 次失败应断开重连。 |
| 恢复 | 连接方保存 `lastEventId`，重连后发送 `resume`；服务端补发最近事件，超出保留窗口则返回 `sync.required`。 |
| 顺序 | 每个用户连接收到的事件带全局递增 `eventId` 和服务端 `createdAt`；同一 conversation 内消息按服务端分配的 `messageSeq` 排序。 |

### 6.2 服务端接收事件

| event | payload | 返回/广播 | 说明 |
| --- | --- | --- | --- |
| `ping` | `timestamp` | `pong` | 心跳。 |
| `resume` | `lastEventId` | `event.replay` 或 `sync.required` | 断线恢复。 |
| `chat.message.send` | `conversationId`, `clientMessageId`, `clientSentAt?`, `type`, `content`, `attachments`, `referencedMessageId` | `chat.message.ack`，并向参与者广播 `chat.message.created` | 实时发送消息；ack 返回服务端消息 ID、`messageSeq` 和服务端时间。 |
| `chat.message.recall` | `conversationId`, `messageId` | `chat.message.recalled` | 撤回。 |
| `chat.read` | `conversationId`, `lastReadMessageId`, `lastReadAt` | `chat.read.updated` | 已读同步。 |
| `presence.update` | `status` | `presence.updated` | 在线状态。 |
| `ai.message.send` | `conversationId`, `clientMessageId`, `message`, `aiFileIds?` | `ai.stream.chunk`, `ai.stream.done` | AI 流式回复；服务端只使用解析完成且当前用户有权访问的文件。 |
| `ai.stream.cancel` | `streamId` | `ai.stream.cancelled` | 停止 AI 回复。 |

### 6.3 服务端推送事件

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

## 7. 响应模型字段建议

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

文件响应统一返回业务可用 URL，不返回对象存储内部路径：

```json
{
  "fileId": "file_018f4f7e",
  "fileName": "report.pdf",
  "mimeType": "application/pdf",
  "sizeBytes": 204800,
  "url": "https://cdn.example.com/protected/file_018f4f7e?sig=...",
  "thumbnailUrl": null,
  "visibility": "owner_private",
  "processingStatus": "ready",
  "version": 1,
  "etag": "\"minio-etag\"",
  "contentHash": "sha256:..."
}
```

AI 文件响应必须暴露解析状态和上下文成本：

```json
{
  "aiFileId": "aif_001",
  "fileId": "file_018f4f7e",
  "fileName": "report.pdf",
  "parseStatus": "ready",
  "tokenCount": 8420,
  "retentionPolicy": "conversation",
  "previewText": "前 200 字摘要或抽取片段..."
}
```

分片上传会话响应示例：

```json
{
  "uploadSessionId": "ups_001",
  "fileId": "file_018f4f7e",
  "uploadMode": "multipart",
  "partSizeBytes": 8388608,
  "totalParts": 18,
  "uploadedParts": [
    {
      "partNumber": 1,
      "etag": "\"part-etag-1\"",
      "sizeBytes": 8388608
    }
  ],
  "expiresAt": "2026-06-01T12:00:00.000Z"
}
```

分片 URL 响应示例：

```json
{
  "parts": [
    {
      "partNumber": 2,
      "uploadUrl": "https://minio.example.com/netherlink-tmp/...?partNumber=2&uploadId=...",
      "headers": {
        "Content-Type": "application/octet-stream"
      },
      "expiresAt": "2026-06-01T10:15:00.000Z"
    }
  ]
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

## 8. 实施优先级

| 优先级 | 后端能力 | 原因 |
| --- | --- | --- |
| P0 | Auth 注册/登录/刷新、`/me`、`/me/preferences`、token 验证 | 所有登录态业务依赖账号、鉴权和当前用户资料。 |
| P0 | 用户/好友/群详情 GET、头像文件元数据 | 基础社交关系和资料展示依赖这些接口。 |
| P0 | WebSocket 鉴权、消息发送/接收、历史消息分页 | 聊天是实时核心功能，必须先保证消息可靠性和补偿拉取。 |
| P1 | 好友申请、群申请、通知未读 | 社交关系变更和通知需要后端统一裁决。 |
| P1 | 群成员和权限操作 | 敏感操作必须由服务端校验权限。 |
| P1 | MinIO/S3 文件上传、预签名 URL、文件版本、缩略图/转码任务 | 头像、帖子媒体、聊天附件和 AI 上传文件都依赖统一文件服务。 |
| P2 | 帖子流、帖子详情、点赞、评论、回复 | 内容社区能力。 |
| P2 | AI 聊天会话、消息、上传文件解析、上下文切片和流式回复 | AI 会话能力，且需要避免把大文件直接塞进 prompt。 |
| P3 | 幂等操作回执和批量补偿同步 | 网络不稳定和多端一致性增强。 |
