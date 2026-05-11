# DemonStar Trainer

DemonStar Trainer 是一个基于 Qt Widgets 的 DemonStar 单人模式修改器。程序会检测本机运行中的 `demonstar.exe`，并通过 Windows 进程内存读写实现修改功能。

## 功能

- 检测 `demonstar.exe` 是否正在运行
- 无限飞机，快捷键 `Ctrl+1`
- 无限核弹，快捷键 `Ctrl+2`
- 无限生命值，快捷键 `Ctrl+3`
- 游戏退出或进程重启后自动关闭已启用的修改项

## 环境要求

- Windows
- CMake 3.16 或更高版本
- Qt 5 或 Qt 6，需包含 Widgets 模块
- 支持 C++17 的编译器

内存修改功能只在 Windows 下可用。非 Windows 平台可以编译界面代码，但修改功能会被禁用。

## 构建

在项目根目录执行：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

如果 CMake 无法自动找到 Qt，请指定 Qt 安装目录，例如：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
cmake --build build --config Release
```

## 使用

1. 启动 DemonStar，并确保进程名为 `demonstar.exe`。
2. 启动 `DemonStarTrainer`。
3. 在窗口中勾选需要的修改项，或使用 `Ctrl+1`、`Ctrl+2`、`Ctrl+3` 切换。

## 偏移量

当前偏移量记录见 [offsets.md](offsets.md)。这些偏移量只适用于已记录的游戏版本和单人模式，游戏版本变化后可能需要重新确认。

