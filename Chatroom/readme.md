# PyFeiQ - A LAN Messenger in Python
这是一个使用 Python 和 PyQt5 重构的局域网即时通讯工具，灵感来源于经典的 FeiQ (飞秋)。

## 项目目标
旨在实现一个跨平台的、无需服务器的局域网聊天工具，满足以下核心功能：

- 局域网内用户自动发现

- 一对一聊天和群聊

- 用户分组管理

- 文件和文件夹传输

- 表情包和截图发送

## 技术栈
语言: Python 3

GUI框架: PyQt5

网络通信:

- UDP (用于广播和用户发现)

- TCP (用于文件传输和大数据交换)

## 如何运行
### 安装依赖:

```
pip install PyQt5

```
启动应用:
```
python main.py

```

## 项目结构

```
.
py-feiq/
|
├── main.py               # 应用程序主入口
|
├── core/                   # 核心业务逻辑模块
|   ├── __init__.py
|   ├── network.py          # 网络通信 (UDP/TCP)，用户发现，消息收发
|   ├── protocol.py         # 定义通信协议的常量和消息格式化工具
|   └── user.py             # 用户信息数据模型
|
├── ui/                     # PyQt 界面模块
|   ├── __init__.py
|   ├── main_window.py      # 主窗口 (用户列表、搜索、分组)
|   ├── chat_window.py      # 聊天窗口 (支持表情、截图)
|   └── components/         # 可复用的 UI 组件 (如表情选择器)
|
├── resources/              # 资源文件
|   ├── __init__.py
|   ├── icons/              # 图标
|   └── emojis/             # 表情图片
|
└── utils/                  # 通用工具模块
    ├── __init__.py
    └── config.py           # 配置文件读写等
```