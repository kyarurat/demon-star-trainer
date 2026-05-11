#ifndef DEMONSTAR_TRAINER_H
#define DEMONSTAR_TRAINER_H

#include "IProcessMemory.h"
#include "ITrainer.h"

#include <memory>

namespace demonstar {

class DemonStarTrainer final : public ITrainer
{
public:
    DemonStarTrainer();
    explicit DemonStarTrainer(std::unique_ptr<IProcessMemory> processMemory);
    ~DemonStarTrainer() override;

    void setListener(ITrainerListener *listener) override;
    void tick() override;
    bool setCheatEnabled(CheatId cheat, bool enabled) override;
    bool isCheatEnabled(CheatId cheat) const override;
    bool isGameAvailable() const override;
    void shutdown() override;

private:
    struct LockState {
        bool enabled = false;
        bool hasLockedValue = false;
        bool pausedAboveLimit = false;
        std::int32_t lockedValue = 0;
    };

    bool ensureGameProcess();
    void handleDetached(bool restarted);
    void updateCheat(CheatId cheat);
    void updateLimitedLock(CheatId cheat, LockState &state, std::uintptr_t offset);
    void updateHealth();
    void resetState(CheatId cheat);
    void disableAllCheats();
    void notify(TrainerEventType type, CheatId cheat, const std::string &message);
    LockState &stateFor(CheatId cheat);
    const LockState &stateFor(CheatId cheat) const;

    std::unique_ptr<IProcessMemory> processMemory_;
    ITrainerListener *listener_ = nullptr;
    bool gameAvailable_ = false;
    std::uint32_t lastProcessId_ = 0;
    LockState planes_;
    LockState nukes_;
    LockState health_;
};

} // namespace demonstar

#endif // DEMONSTAR_TRAINER_H
