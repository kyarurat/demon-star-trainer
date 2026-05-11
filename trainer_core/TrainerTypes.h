#ifndef DEMONSTAR_TRAINER_TYPES_H
#define DEMONSTAR_TRAINER_TYPES_H

#include <string>

namespace demonstar {

enum class CheatId {
    InfinitePlanes,
    InfiniteNukes,
    InfiniteHealth,
    None
};

enum class TrainerEventType {
    GameAttached,
    GameDetached,
    GameRestarted,
    CheatEnabled,
    CheatDisabled,
    CheatLocked,
    LockUpdated,
    ValueAdded,
    ReadFailed,
    WriteFailed,
    AttachFailed
};

struct TrainerEvent {
    TrainerEventType type = TrainerEventType::AttachFailed;
    CheatId cheat = CheatId::None;
    std::string message;
    bool gameAvailable = false;
    bool cheatEnabled = false;
    int value = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_TRAINER_TYPES_H
