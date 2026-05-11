# DemonStar 修改器核心

这个目录是一组可以直接复制使用的 C++17 修改器核心代码，不依赖 Qt。

## 依赖

- Windows API
- C++17 标准库
- 链接 `kernel32`

## 需要复制的文件

- `TrainerTypes.h`
- `ITrainerListener.h`
- `ITrainer.h`
- `IProcessMemory.h`
- `WinProcessMemory.h/.cpp`
- `DemonStarTrainer.h/.cpp`

## 最小使用示例

```cpp
#include "trainer_core/DemonStarTrainer.h"

class Listener : public demonstar::ITrainerListener {
public:
    void onTrainerEvent(const demonstar::TrainerEvent &event) override {
        // 可以把 event.message 显示到界面或日志里。
        // 也可以根据 event.gameAvailable、event.valueId、event.locked 同步外部状态。
    }
};

Listener listener;
demonstar::DemonStarTrainer trainer;
trainer.setListener(&listener);

// 建议周期性调用，例如每 100 毫秒调用一次。
trainer.tick();

// 增加游戏内数值。
trainer.addValue(demonstar::TrainerValueId::Planes, 5);
trainer.addValue(demonstar::TrainerValueId::Nukes, 5);
trainer.addValue(demonstar::TrainerValueId::Health, 160);

// 锁定当前游戏内数值。
// 锁定后，tick() 会在数值变化时把它写回锁定值。
// 如果锁定状态下调用 addValue()，锁定值也会同步增加。
trainer.setValueLocked(demonstar::TrainerValueId::Planes, true);

// 不再使用时释放进程句柄并关闭所有锁定。
trainer.shutdown();
```

## 数值项

- `TrainerValueId::Planes`：飞机数量
- `TrainerValueId::Nukes`：核弹数量
- `TrainerValueId::Health`：生命值

## 说明

核心模块会检测并附加到 `demonstar.exe`，实际读写地址使用 `DemonStarTrainer.cpp` 中记录的单人模式偏移量。

如果游戏退出、进程重启或内存读写失败，核心会自动关闭锁定状态，并通过 `ITrainerListener` 通知外部程序同步界面状态。
