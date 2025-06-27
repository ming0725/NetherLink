# 📦 NetherLink

> 基于 Qt 的聊天社交客户端，Minecraft 风格，实现群聊/私聊、帖子中心、AI 对话等功能的完整应用。

---

## 🧭 目录

- [项目介绍](#-项目介绍)
- [核心功能](#-核心功能)
- [技术栈](#-技术栈)
- [界面预览](#-界面预览)
- [安装与启动](#-安装与启动)
- [已知问题](#-已知问题)

---

## 📖 项目介绍

NetherLink 是一个基于 Qt 的聊天社交客户端，**包含完整的前后端交互逻辑**。支持用户注册登录、实时通讯、数据持久化等功能。采用 WebSocket 实现即时通讯，SQLite 数据库进行本地数据存储，打造流畅的社交体验。

---

## ✨ 核心功能

- 🔹 **用户系统**
    - 用户注册与登录
    - 个人信息管理
    - 好友添加与管理

- 🔹 **群聊 & 私聊**
    - 多人群组实时聊天
    - 单对单私聊
    - 消息类型：文字、图片
    - 消息历史记录
    - 未读消息提醒

- 🔹 **帖子中心**
    - 发布、浏览帖子
    - 帖子点赞功能
    - 评论系统
    - 帖子内容持久化

- 🔹 **AI 对话**
    - 实时 AI 对话功能
    - 流式响应

- 🔹 **界面美化 & 动画**
    - Minecraft 像素风图标与配色
    - 界面元素动效（滑动、弹出、淡入淡出等）
    - 流畅的用户交互体验

---

## 🛠 技术栈

| 类别       | 技术                           |
|----------|------------------------------|
| UI 框架    | Qt Widgets (C++ / Qt 6)      |
| 网络通信     | Qt WebSocket                 |
| 数据库      | SQLite3 + Qt SQL             |
| 动画       | QPropertyAnimation           |
| 图标 & 资源  | Minecraft 风格 PNG             |
| 构建工具     | CMake                        |

### 资源引用
- Minecraft 风格字体（需手动开启）：[【教程】我的世界Minecraft中文字体制作教程](https://www.bilibili.com/opus/650428219754807321)

---

## 🖼 界面预览

> 以下为示例截图，展示主要页面风格。

1. **主界面**
2. **好友列表**
3. **帖子中心**
4. **AI 对话窗口**

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/1_chat.jpg?raw=true" alt="聊天界面" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/2_friends.png?raw=true" alt="联系人界面" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/3_posts.png?raw=true" alt="帖子中心" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/4_detail_post.png?raw=true" alt="帖子详情页" width="600"/>

<img src="https://github.com/ming0725/NetherLink-static/blob/master/doc/images/5_aichat.png?raw=true" alt="AI 对话" width="600"/>

---

## 🚀 安装与启动

### 环境要求

- Qt 6.x（需包含 WebSocket 和 SQL 模块）
- SQLite 3.x
- CMake ≥ 3.10
- 支持平台：Windows 11

### 依赖配置

1. **WebSocket 支持**
   - 方式一：在 Qt 安装程序中勾选 WebSocket 模块
   - 方式二：手动安装 WebSocket 模块
     - 详细安装教程：[QtWebsockets安装教程](https://github.com/ming0725/NetherLink/blob/master/QtWebsockets安装教程.md)

2. **SQLite 支持**
   - 下载并安装 SQLite3
   - 配置 SQLite3 环境变量（将 SQLite3 的 bin 目录添加到 PATH）
   - 确保 Qt 的 QSQLITE 驱动已编译（通常位于 plugins/sqldrivers 目录）
   - 如果 QSQLITE 驱动不存在，需要手动编译

### 服务器配置

1. **服务器部署**
   - 服务器端代码仓库：[NetherLink-server](https://github.com/ming0725/NetherLink-server)
   - 按照服务器仓库的说明进行部署

2. **客户端配置**
   - 部署完成后，修改 `network_config.json` 文件中的服务器配置：
     ```json
     {
         "server": {
             "ip": "你的服务器IP",
             "http_port": 8080,
             "websocket_port": 8081
         }
     }
     ```

### 快速运行

```bash
# 克隆仓库
git clone https://github.com/ming0725/NetherLink.git
cd NetherLink

# 创建构建目录并编译
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 运行可执行文件
./NetherLink
```

## 🚧 功能限制与开发中的特性

### 💬 聊天功能
- 图片发送功能仅支持本地预览
- 不支持滚动到顶部加载更多历史消息
- 聊天输入栏功能未完全实现：
  - 表情选择功能
  - 截图功能
  - 历史消息查看
- 群聊功能受限：
  - 不支持创建新群组（需通过数据库手动创建）
  - 不支持加入群组（需通过数据库手动添加）
  - 好友列表中暂不支持群聊显示

### 👤 用户系统
- 个人信息修改受限：
  - 不支持裁切头像（仅支持居中显示）
  - 不支持修改在线状态（需通过数据库手动修改）
  - 不支持设置个性签名（需通过数据库手动修改）
  - 不支持修改用户名（需通过数据库手动修改）
  - 不支持设置好友备注

### 🤖 AI 对话
- 不支持中断 AI 响应
- 对话历史记录未实现存储功能

### 🔍 搜索与导航
- 左侧列表顶部搜索功能未实现
- ApplicationBar 底部按钮功能未实现

### 💾 数据存储
- 本地 SQLite 存储功能尚未完全实现

---

## ⚠️ 已知问题

- **网络连接不稳定**：在某些网络环境下可能出现连接断开的情况。
- **内存占用较高**：部分页面连续切换或加载大量图片时内存飙升。
- **界面卡顿**：动画切换偶有卡顿现象。
- **字体渲染**：由于 Qt 显示优化，默认关闭了部分字体渲染选项。如需开启完整字体显示效果，请取消 `main.cpp` 中相关代码的注释。

📌 **后续计划**：
- 优化网络重连机制
- 实现消息队列与离线消息
- 优化资源加载
- 添加虚拟化/懒加载机制
- 进一步调优动画曲线

📅 **更新日志**：

**2025/6/27 更新**
- ✨ AI 对话优化
  - 支持 Markdown 格式消息显示
  - 修复 AI 对话消息气泡显示异常问题
- 👥 好友系统增强
  - 新增好友搜索功能
  - 实现好友添加请求系统
    - 支持发送添加好友请求
    - 支持添加好友留言
    - 好友可接收请求通知
    - 支持同意/拒绝好友申请 