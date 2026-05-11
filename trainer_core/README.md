# DemonStar trainer core

This directory is a copyable C++17 trainer module. It has no Qt dependency.

## Dependencies

- Windows API
- C++17 standard library
- Link with `kernel32`

## Files to copy

- `TrainerTypes.h`
- `ITrainerListener.h`
- `ITrainer.h`
- `IProcessMemory.h`
- `WinProcessMemory.h/.cpp`
- `DemonStarTrainer.h/.cpp`

## Minimal usage

```cpp
#include "trainer_core/DemonStarTrainer.h"

class Listener : public demonstar::ITrainerListener {
public:
    void onTrainerEvent(const demonstar::TrainerEvent &event) override {
        // Show event.message in your UI/log and sync state from event fields.
    }
};

Listener listener;
demonstar::DemonStarTrainer trainer;
trainer.setListener(&listener);

// Call periodically, for example every 100 ms.
trainer.tick();

trainer.addCheatValue(demonstar::CheatId::InfinitePlanes, 5);
trainer.addCheatValue(demonstar::CheatId::InfiniteNukes, 5);
trainer.addCheatValue(demonstar::CheatId::InfiniteHealth, 160);

// Lock the current in-game values. While locked, tick() writes the locked value
// back whenever the game value changes. Calling addCheatValue while locked also
// raises the locked value.
trainer.setCheatEnabled(demonstar::CheatId::InfinitePlanes, true);

trainer.shutdown();
```

The module targets `demonstar.exe` and uses the recorded single-player offsets in `DemonStarTrainer.cpp`.
