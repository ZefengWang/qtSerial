<div align="center">

# Serial Debug Assistant

**现代化跨平台串口调试助手** | 基于 Qt5/Qt6 | 支持 Linux（X11/Wayland）和 Windows

![License](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)
![Qt](https://img.shields.io/badge/Qt-5.15%20%7C%206.5%2B-green.svg?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg?style=flat-square)
![CI](https://github.com/ZefengWang/qtSerial/actions/workflows/build.yml/badge.svg?style=flat-square)
![Version](https://img.shields.io/badge/version-2.0-blue.svg?style=flat-square)

</div>

---

## 目录

- [功能特性](#-功能特性)
- [快速开始](#-快速开始)
- [下载安装](#-下载安装)
- [从源码编译](#-从源码编译)
- [项目结构](#-项目结构)
- [CI/CD 自动构建](#-cicd-自动构建)
- [界面预览](#-界面预览)
- [常见问题](#-常见问题)
- [开发计划](#-开发计划)
- [许可证](#-许可证)

## ✨ 功能特性

### 串口通信

| 功能 | 说明 |
|------|------|
| 串口配置 | 波特率（4800 ~ 1500000）、数据位、停止位、校验位、流控 |
| 数据收发 | ASCII / Hex 双模式，一键切换 |
| 定时发送 | 可配置间隔的自动循环发送 |
| CR+LF 换行 | 发送时可选附加回车换行符 |
| 日志保存 | 接收数据导出为带时间戳的日志文件 |

### 数据显示

| 功能 | 说明 |
|------|------|
| 时间戳 | 接收数据可选显示精确到毫秒的时间戳 |
| 自动滚动 | 接收区自动滚动到底部，可开关 |
| 收发计数 | 实时统计 RX/TX 字节数（自动格式化 B/KB/MB） |
| Hex 格式化 | 十六进制模式自动按字节空格分隔 |

### 界面设计

- **Tokyo Night 深色主题** — 护眼配色，长时间使用不疲劳
- **侧边栏布局** — 左侧配置区 + 右侧数据区，层次分明
- **流畅交互** — 圆角控件、悬停动画、平滑状态过渡
- **状态指示灯** — 绿色已连接 / 红色未连接，一目了然
- **跨平台一致** — Linux 和 Windows 下界面风格统一

## 🚀 快速开始

**最快的方式** — 直接下载预编译版本：

1. 前往 [Actions 构建页面](https://github.com/ZefengWang/qtSerial/actions)
2. 点击最新一次绿色对勾的构建
3. 拉到页面底部 **Artifacts** 区域，下载对应平台的压缩包
4. 解压后直接运行

> 详细步骤见下方 [下载安装](#-下载安装) 章节。

## 📦 下载安装

### 方式一：从 GitHub Actions 下载（最新构建）

每次推送代码后都会自动构建，可直接下载最新版本：

1. 打开 [Actions 页面](https://github.com/ZefengWang/qtSerial/actions)
2. 点击列表中最上方一次 **绿色对勾** 的构建
3. 滚动到页面底部，找到 **Artifacts** 区域
4. 点击对应平台的压缩包即可下载（需要登录 GitHub 账号）

**可用构建产物：**

| 平台 | 下载文件名 | Qt 版本 | 说明 |
|------|-----------|---------|------|
| Linux x86_64 | `SerialDebug-Linux-ubuntu2604-qt6-x86_64.tar.gz` | Qt 6.5.3 | Ubuntu 26.04 构建，推荐 |
| Linux x86_64 | `SerialDebug-Linux-ubuntu2404-qt5-x86_64.tar.gz` | Qt 5.15 | Ubuntu 24.04 构建，兼容旧系统 |
| Linux x86_64 | `SerialDebug-Linux-ubuntu2204-qt5-x86_64.tar.gz` | Qt 5.15 | Ubuntu 22.04 构建，兼容旧系统 |
| Windows x64 | `SerialDebug-Windows-x86_64.zip` | Qt 6.5.3 | Windows 10/11 |

### 方式二：从 Release 下载（稳定版本）

访问 [Releases 页面](https://github.com/ZefengWang/qtSerial/releases)，下载已发布的稳定版本。打 `v*` 标签时会自动发布 Release。

### Linux 运行方式

```bash
# 解压
tar -xzf SerialDebug-Linux-*.tar.gz
cd SerialDebug/

# 方式一：使用启动脚本（自动检测 X11/Wayland）
./run.sh

# 方式二：直接运行
./SerialDebug
```

> 如果运行报错找不到 Qt 库，请使用 `run.sh` 启动脚本，它会自动设置库路径。

### Windows 运行方式

解压 zip 文件后，双击 `SerialDebug.exe` 即可运行。

## 🔨 从源码编译

### 环境要求

| 依赖 | Qt5 | Qt6 |
|------|-----|-----|
| Qt 版本 | 5.15+ | 6.5+ |
| C++ 标准 | C++11 | C++17 |
| serialport 模块 | 必需 | 必需 |
| 编译器 | GCC / MSVC | GCC / MSVC |

### Linux 编译

```bash
# ---------- 安装依赖 ----------

# Qt5（Ubuntu 22.04 / 24.04）
sudo apt install build-essential qtbase5-dev libqt5serialport5-dev

# 或 Qt6（Ubuntu 26.04+）
sudo apt install build-essential qt6-base-dev libqt6serialport6-dev

# Wayland 支持（可选）
sudo apt install libwayland-dev wayland-protocols libxkbcommon-dev

# ---------- 编译 ----------

qmake MySerial.pro    # Qt5
# 或
qmake6 MySerial.pro   # Qt6

make -j$(nproc)

# ---------- 运行 ----------
./SerialDebug
```

### Windows 编译

1. 安装 [Qt](https://www.qt.io/download)（选择 5.15 或 6.5+，勾选 serialport 模块）
2. 使用 **Qt Creator** 打开 `MySerial.pro`，点击构建即可
3. 或命令行编译：
   ```cmd
   qmake MySerial.pro
   nmake
   ```

### CMake 编译（可选）

项目同时支持 CMake 构建：

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

## 🏗 项目结构

```
qtSerial/
├── main.cpp                  # 程序入口
├── uart_interaction.h        # 主窗口类定义
├── user_interaction.cpp      # 主窗口交互逻辑
├── uart_interface.ui         # 主界面 UI（Qt Designer）
├── uart_core.h               # 串口核心类定义
├── uart_core.cpp             # 串口核心实现
├── uart_setting.h            # 设置对话框类定义
├── uart_setting.cpp          # 设置对话框实现
├── uart_setting.ui           # 设置对话框 UI
├── styles/
│   └── dark.qss              # 深色主题样式表（Tokyo Night）
├── res.qrc                   # Qt 资源文件
├── logo.ico                  # 应用图标
├── MySerial.pro              # qmake 项目文件
├── CMakeLists.txt            # CMake 项目文件
└── .github/workflows/
    └── build.yml             # 多平台 CI/CD 工作流
```

## 🤖 CI/CD 自动构建

项目使用 GitHub Actions 实现多平台自动构建。每次推送代码或打 Tag 时自动触发。

### 构建矩阵

```
                    ┌──────────────────────────┐
                    │     Push / Tag 触发       │
                    └────────────┬─────────────┘
                    ┌────────────┴─────────────┐
                    │                          │
              ┌─────▼─────┐            ┌───────▼───────┐
              │  Linux    │            │   Windows     │
              │  x3 矩阵  │            │   Qt 6.5.3    │
              └─────┬─────┘            │   MSVC 2019   │
          ┌─────────┼─────────┐        └───────┬───────┘
          │         │         │                │
     ┌────▼───┐┌───▼────┐┌───▼────┐     ┌─────▼─────┐
     │ 26.04  ││ 24.04  ││ 22.04  │     │  .zip     │
     │ Qt6    ││ Qt5    ││ Qt5    │     │  Artifact │
     │aqtinst ││ system ││ system │     └───────────┘
     └────────┘└────────┘└────────┘
          │         │         │
          └─────────┼─────────┘
                    │
              ┌─────▼─────┐
              │  Tag 触发  │
              │  Release  │
              └───────────┘
```

### 构建策略

| Runner | Qt 版本 | 安装方式 | 目标用户 |
|--------|---------|---------|---------|
| `ubuntu-26.04` | Qt 6.5.3 | aqtinstall | 最新 LTS 系统，推荐 |
| `ubuntu-24.04` | Qt 5.15 | 系统包 (`apt`) | Ubuntu 24.04 及类似系统 |
| `ubuntu-22.04` | Qt 5.15 | 系统包 (`apt`) | Ubuntu 22.04 及更旧系统 |
| `windows-2022` | Qt 6.5.3 | aqtinstall | Windows 10/11 |

**为什么 26.04 用 Qt6，旧版本用 Qt5？**

- Ubuntu 26.04 LTS 默认搭载 Qt6，使用 Qt6 编译获得最新特性和性能
- Ubuntu 24.04 / 22.04 默认搭载 Qt5，使用系统 Qt5 避免额外依赖，保证兼容性
- 所有 Linux 构建同时打包 X11 和 Wayland 平台插件
- 推送 `v*` 标签时自动创建 GitHub Release，附带全部构建产物

## 📸 界面预览

> 截图将在后续更新中添加

## ❓ 常见问题

<details>
<summary><b>Linux 下提示找不到串口 / 权限不足？</b></summary>

```bash
# 将当前用户加入 dialout 组
sudo usermod -aG dialout $USER

# 注销重新登录后生效，或临时执行
sudo chmod 666 /dev/ttyUSB0
```
</details>

<details>
<summary><b>Wayland 下界面异常或闪退？</b></summary>

```bash
# 强制使用 X11 后端
QT_QPA_PLATFORM=xcb ./SerialDebug

# 或使用启动脚本（自动检测）
./run.sh
```
</details>

<details>
<summary><b>下载的 Artifacts 提示已过期？</b></summary>

GitHub Actions 的 Artifacts 默认保留 90 天。请重新触发一次构建，或从 [Releases](https://github.com/ZefengWang/qtSerial/releases) 下载稳定版本。
</details>

<details>
<summary><b>Windows 下提示缺少 DLL？</b></summary>

请确保下载的是完整 zip 包并已解压。程序依赖的 Qt DLL 文件已打包在同一目录下，不要单独移动 exe 文件。
</details>

## 📋 开发计划

**已完成：**

- [x] 现代化 Tokyo Night 深色主题界面
- [x] 侧边栏布局（配置区 + 数据区）
- [x] Hex 发送 / 接收
- [x] 定时发送
- [x] 时间戳显示（毫秒级）
- [x] 日志保存
- [x] 自动滚动
- [x] 收发字节计数
- [x] X11 + Wayland 支持
- [x] 多平台 CI/CD（Ubuntu 22.04 / 24.04 / 26.04 + Windows）
- [x] Qt5 / Qt6 兼容编译

**规划中：**

- [ ] 多语言切换（中文 / 英文）
- [ ] 数据波形图显示
- [ ] 自定义快捷发送按钮
- [ ] 串口数据过滤与搜索
- [ ] 脚本化自动测试支持

## 📄 许可证

本项目基于 [MIT 许可证](LICENSE) 开源。

## 🙏 致谢

- [Qt Framework](https://www.qt.io/) — 跨平台应用开发框架
- [Tokyo Night](https://github.com/enkia/tokyo-night-vscode-theme) — 配色方案灵感
- [aqtinstall](https://github.com/miurahr/aqtinstall) — Qt 自动化安装工具
