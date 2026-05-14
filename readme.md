# DemonStar Trainer

DemonStar Trainer 是一个面向 DemonStar 单人模式的 Windows 修改器。当前项目由两层组成：

- Qt Widgets 界面层：负责窗口、复选框、状态栏、定时器和快捷键。
- `trainer_core/` 修改核心：负责检测 `demonstar.exe`、读取/写入进程内存、维护修改项状态。核心不依赖 Qt，方便拷贝到其他 C++ 项目里使用。

## 功能

- 自动检测本机运行中的 `demonstar.exe`
- 飞机数量增加与锁定：`Ctrl+Shift+1` 增加，`Ctrl+1` 切换锁定
- 核弹数量增加与锁定：`Ctrl+Shift+2` 增加，`Ctrl+2` 切换锁定
- 生命值增加与锁定：`Ctrl+Shift+3` 增加，`Ctrl+3` 切换锁定
- 低难度模式：按档位自动恢复核弹和生命值，且不修改飞机数量
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
3. 在每个项目的输入框中设置每次增加的数值。
4. 点击“增加”会把游戏当前值增加输入框中的数值；默认飞机 `+5`，核弹 `+5`，生命值 `+160`。
5. 勾选“锁定”会读取当前游戏值并锁定到该值；锁定后如果点击“增加”，锁定值也会同步增加。
6. 勾选“低难度模式”会自动关闭三个锁定功能，并按档位恢复核弹和生命值；飞机数量不会被低难度模式读写。

低难度档位：

- 低档：核弹每 `15s` 增加 1，生命值每 `3s` 增加 1
- 中档：核弹每 `10s` 增加 1，生命值每 `2s` 增加 1
- 高档：核弹每 `5s` 增加 1，生命值每 `1s` 增加 1

低难度限制：

- 核弹最多补到 `5`，达到或超过 `5` 后不再增加
- 生命值最多 `160`，超过 `160` 会被拉回 `160`

快捷键：

- `Ctrl+1`、`Ctrl+2`、`Ctrl+3`：切换飞机、核弹、生命值锁定
- `Ctrl+Shift+1`、`Ctrl+Shift+2`、`Ctrl+Shift+3`：增加飞机、核弹、生命值

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
trainer.addValue(demonstar::TrainerValueId::Planes, 5);
trainer.setValueLocked(demonstar::TrainerValueId::Planes, true); // 锁定当前飞机数量
```

## 偏移量

当前偏移量记录见 [offsets.md](offsets.md)。这些偏移量只适用于已记录的游戏版本和单人模式，游戏版本变化后可能需要重新确认。
