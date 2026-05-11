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
    bool addCheatValue(CheatId cheat, int amount) override;
    bool isCheatEnabled(CheatId cheat) const override;
    bool isGameAvailable() const override;
    void shutdown() override;

private:
    struct LockState {
        bool enabled = false;
        bool hasLockedValue = false;
        std::int32_t lockedValue = 0;
    };

    bool ensureGameProcess();
    void handleDetached(bool restarted);
    void handleMemoryAccessFailed(CheatId cheat, TrainerEventType type, const std::string &message);
    void updateCheat(CheatId cheat);
    void updateLockedValue(CheatId cheat);
    void resetState(CheatId cheat);
    void disableAllCheats();
    void notify(TrainerEventType type, CheatId cheat, const std::string &message, int value = 0);
    bool isValidCheat(CheatId cheat) const;
    bool readValue(CheatId cheat, std::int32_t &value) const;
    bool writeValue(CheatId cheat, std::int32_t value) const;
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
