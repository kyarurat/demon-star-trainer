# DemonStar Trainer

DemonStar Trainer 是一个面向 DemonStar 单人模式的 Windows 修改器。当前项目由两层组成：

- Qt Widgets 界面层：负责窗口、复选框、状态栏、定时器和快捷键。
- `trainer_core/` 修改核心：负责检测 `demonstar.exe`、读取/写入进程内存、维护修改项状态。核心不依赖 Qt，方便拷贝到其他 C++ 项目里使用。

## 功能

- 自动检测本机运行中的 `demonstar.exe`
- 无限飞机，快捷键 `Ctrl+1`
- 无限核弹，快捷键 `Ctrl+2`
- 无限生命值，快捷键 `Ctrl+3`
- 游戏退出或进程重启后自动关闭已启用的修改项

## 项目结构

- `main.cpp`、`mainwindow.*`、`mainwindow.ui`：Qt 界面和热键集成
- `trainer_core/`：可复用修改核心
- `offsets.md`：当前记录的内存偏移

`trainer_core/` 内部使用接口 + 实现的形式：

- `ITrainer`：修改器控制接口
- `ITrainerListener`：事件回调接口
- `DemonStarTrainer`：DemonStar 修改逻辑实现
- `IProcessMemory`：进程内存访问抽象
- `WinProcessMemory`：Windows API 进程内存读写实现

更详细的复用说明见 [trainer_core/README.md](trainer_core/README.md)。

## 环境要求

- Windows
- CMake 3.16 或更高版本
- Qt 5 或 Qt 6，需包含 Widgets 模块
- 支持 C++17 的编译器

修改核心目前只支持 Windows。`trainer_core/` 本身不依赖 Qt，但会使用 Windows API，集成到其他项目时需要链接 `kernel32`。

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

Qt Creator 生成的构建目录也可以直接使用，例如：

```powershell
cmake --build build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug --config Debug
```

如果命令行找不到 `cmake`，请把 Qt 自带的 CMake 和编译器目录加入 `PATH`。

## 使用

1. 启动 DemonStar，并确保进程名为 `demonstar.exe`。
2. 启动 `DemonStarTrainer`。
3. 在窗口中勾选需要的修改项，或使用 `Ctrl+1`、`Ctrl+2`、`Ctrl+3` 切换。

## 复用修改核心

如果只想在其他项目里使用修改核心，可以复制整个 `trainer_core/` 目录，并把以下 `.cpp` 文件加入目标项目：

- `trainer_core/DemonStarTrainer.cpp`
- `trainer_core/WinProcessMemory.cpp`

最小调用方式：

```cpp
#include "trainer_core/DemonStarTrainer.h"

class Listener : public demonstar::ITrainerListener {
public:
    void onTrainerEvent(const demonstar::TrainerEvent &event) override {
        // event.message 可用于日志或 UI 状态提示。
    }
};

Listener listener;
demonstar::DemonStarTrainer trainer;
trainer.setListener(&listener);

trainer.tick(); // 建议每 100 ms 调用一次
trainer.setCheatEnabled(demonstar::CheatId::InfinitePlanes, true);
```

## 偏移量

当前偏移量记录见 [offsets.md](offsets.md)。这些偏移量只适用于已记录的游戏版本和单人模式，游戏版本变化后可能需要重新确认。
