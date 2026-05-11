#include "DemonStarTrainer.h"

#include "WinProcessMemory.h"

#include <limits>
#include <stdexcept>

namespace demonstar {
namespace {

constexpr std::uintptr_t InfiniteHealthOffset = 0x14C7950;
constexpr std::uintptr_t InfinitePlanesOffset = 0x14C7954;
constexpr std::uintptr_t InfiniteNukesOffset = 0x14C80E4;
constexpr wchar_t ProcessName[] = L"demonstar.exe";

const char *cheatName(CheatId cheat)
{
    switch (cheat) {
    case CheatId::InfinitePlanes:
        return "无限飞机";
    case CheatId::InfiniteNukes:
        return "无限核弹";
    case CheatId::InfiniteHealth:
        return "无限生命值";
    case CheatId::None:
        return "修改项";
    }

    return "修改项";
}

std::uintptr_t offsetFor(CheatId cheat)
{
    switch (cheat) {
    case CheatId::InfinitePlanes:
        return InfinitePlanesOffset;
    case CheatId::InfiniteNukes:
        return InfiniteNukesOffset;
    case CheatId::InfiniteHealth:
        return InfiniteHealthOffset;
    case CheatId::None:
        break;
    }

    return 0;
}

} // namespace

DemonStarTrainer::DemonStarTrainer()
    : DemonStarTrainer(std::make_unique<WinProcessMemory>())
{
}

DemonStarTrainer::DemonStarTrainer(std::unique_ptr<IProcessMemory> processMemory)
    : processMemory_(std::move(processMemory))
{
    if (processMemory_ == nullptr) {
        processMemory_ = std::make_unique<WinProcessMemory>();
    }
}

DemonStarTrainer::~DemonStarTrainer()
{
    shutdown();
}

void DemonStarTrainer::setListener(ITrainerListener *listener)
{
    listener_ = listener;
}

void DemonStarTrainer::tick()
{
    const bool wasGameAvailable = gameAvailable_;
    const bool hadEnabledCheat = planes_.enabled || nukes_.enabled || health_.enabled;
    const std::uint32_t previousProcessId = lastProcessId_;

    gameAvailable_ = ensureGameProcess();

    if (!gameAvailable_) {
        if (wasGameAvailable || hadEnabledCheat) {
            handleDetached(false);
        }
        return;
    }

    const bool processChanged = wasGameAvailable && previousProcessId != 0 && lastProcessId_ != previousProcessId;
    if (processChanged) {
        handleDetached(true);
        return;
    }

    if (!wasGameAvailable) {
        notify(TrainerEventType::GameAttached, CheatId::None, "已检测到 demonstar.exe，可以启用修改项");
    }

    if (planes_.enabled) {
        updateCheat(CheatId::InfinitePlanes);
    }

    if (nukes_.enabled) {
        updateCheat(CheatId::InfiniteNukes);
    }

    if (health_.enabled) {
        updateCheat(CheatId::InfiniteHealth);
    }

    if (!planes_.enabled && !nukes_.enabled && !health_.enabled) {
        resetState(CheatId::InfinitePlanes);
        resetState(CheatId::InfiniteNukes);
        resetState(CheatId::InfiniteHealth);
    }
}

bool DemonStarTrainer::setCheatEnabled(CheatId cheat, bool enabled)
{
    if (!isValidCheat(cheat)) {
        return false;
    }

    LockState &state = stateFor(cheat);
    resetState(cheat);

    if (enabled) {
        if (!ensureGameProcess()) {
            gameAvailable_ = false;
            state.enabled = false;
            notify(TrainerEventType::AttachFailed, cheat, std::string("游戏未启动，无法启用") + cheatName(cheat));
            return false;
        }

        std::int32_t currentValue = 0;
        if (!readValue(cheat, currentValue)) {
            handleMemoryAccessFailed(cheat,
                                     TrainerEventType::ReadFailed,
                                     std::string("读取") + cheatName(cheat) + "失败，修改项已关闭");
            return false;
        }

        gameAvailable_ = true;
        state.enabled = true;
        state.hasLockedValue = true;
        state.lockedValue = currentValue;
        notify(TrainerEventType::CheatLocked, cheat, std::string(cheatName(cheat)) + "已锁定到 " + std::to_string(currentValue), currentValue);
        return state.enabled;
    }

    state.enabled = false;
    notify(TrainerEventType::CheatDisabled, cheat, std::string(cheatName(cheat)) + "已关闭");
    if (!planes_.enabled && !nukes_.enabled && !health_.enabled) {
        processMemory_->detach();
        lastProcessId_ = 0;
    }
    return true;
}

bool DemonStarTrainer::addCheatValue(CheatId cheat, int amount)
{
    if (!isValidCheat(cheat) || amount <= 0) {
        return false;
    }

    if (!ensureGameProcess()) {
        gameAvailable_ = false;
        notify(TrainerEventType::AttachFailed, cheat, std::string("游戏未启动，无法增加") + cheatName(cheat));
        return false;
    }

    LockState &state = stateFor(cheat);
    std::int32_t currentValue = 0;
    if (!readValue(cheat, currentValue)) {
        handleMemoryAccessFailed(cheat,
                                 TrainerEventType::ReadFailed,
                                 std::string("读取") + cheatName(cheat) + "失败，无法增加");
        return false;
    }

    const std::int32_t baseValue = state.enabled && state.hasLockedValue ? state.lockedValue : currentValue;
    const auto calculatedValue = static_cast<std::int64_t>(baseValue) + static_cast<std::int64_t>(amount);
    if (calculatedValue > std::numeric_limits<std::int32_t>::max()) {
        notify(TrainerEventType::WriteFailed, cheat, std::string(cheatName(cheat)) + "数值超出范围，未写入");
        return false;
    }

    const auto newValue = static_cast<std::int32_t>(calculatedValue);
    if (!writeValue(cheat, newValue)) {
        handleMemoryAccessFailed(cheat,
                                 TrainerEventType::WriteFailed,
                                 std::string("写入") + cheatName(cheat) + "失败，修改项已关闭");
        return false;
    }

    gameAvailable_ = true;
    if (state.enabled) {
        state.lockedValue = newValue;
        state.hasLockedValue = true;
        notify(TrainerEventType::LockUpdated, cheat, std::string(cheatName(cheat)) + "已增加并锁定到 " + std::to_string(newValue), newValue);
    } else {
        notify(TrainerEventType::ValueAdded, cheat, std::string(cheatName(cheat)) + "已增加到 " + std::to_string(newValue), newValue);
    }

    return true;
}

bool DemonStarTrainer::isCheatEnabled(CheatId cheat) const
{
    if (!isValidCheat(cheat)) {
        return false;
    }

    return stateFor(cheat).enabled;
}

bool DemonStarTrainer::isGameAvailable() const
{
    return gameAvailable_;
}

void DemonStarTrainer::shutdown()
{
    disableAllCheats();
    processMemory_->detach();
    gameAvailable_ = false;
    lastProcessId_ = 0;
}

bool DemonStarTrainer::ensureGameProcess()
{
    if (processMemory_->isAttached()) {
        lastProcessId_ = processMemory_->processId();
        return true;
    }

    processMemory_->detach();
    if (!processMemory_->attach(ProcessName)) {
        lastProcessId_ = 0;
        return false;
    }

    lastProcessId_ = processMemory_->processId();
    return true;
}

void DemonStarTrainer::handleDetached(bool restarted)
{
    disableAllCheats();
    processMemory_->detach();
    gameAvailable_ = false;
    lastProcessId_ = 0;
    notify(restarted ? TrainerEventType::GameRestarted : TrainerEventType::GameDetached,
           CheatId::None,
           restarted ? "游戏进程已重启，修改项已关闭" : "游戏未启动或已退出，修改项已关闭");
}

void DemonStarTrainer::handleMemoryAccessFailed(CheatId cheat, TrainerEventType type, const std::string &message)
{
    if (!processMemory_->isAttached()) {
        handleDetached(false);
        return;
    }

    disableAllCheats();
    notify(type, CheatId::None, message);
}

void DemonStarTrainer::updateCheat(CheatId cheat)
{
    updateLockedValue(cheat);
}

void DemonStarTrainer::updateLockedValue(CheatId cheat)
{
    if (!ensureGameProcess()) {
        handleDetached(false);
        return;
    }

    LockState &state = stateFor(cheat);
    if (!state.enabled || !state.hasLockedValue) {
        return;
    }

    std::int32_t currentValue = 0;
    if (!readValue(cheat, currentValue)) {
        handleMemoryAccessFailed(cheat,
                                 TrainerEventType::ReadFailed,
                                 std::string("读取") + cheatName(cheat) + "失败，修改项已关闭");
        return;
    }

    if (currentValue == state.lockedValue) {
        return;
    }

    if (!writeValue(cheat, state.lockedValue)) {
        handleMemoryAccessFailed(cheat,
                                 TrainerEventType::WriteFailed,
                                 std::string("写入") + cheatName(cheat) + "失败，修改项已关闭");
    }
}

void DemonStarTrainer::resetState(CheatId cheat)
{
    LockState &state = stateFor(cheat);
    const bool enabled = state.enabled;
    state = {};
    state.enabled = enabled;
}

void DemonStarTrainer::disableAllCheats()
{
    planes_ = {};
    nukes_ = {};
    health_ = {};
}

void DemonStarTrainer::notify(TrainerEventType type, CheatId cheat, const std::string &message, int value)
{
    if (listener_ == nullptr) {
        return;
    }

    const bool enabled = cheat != CheatId::None && isCheatEnabled(cheat);
    listener_->onTrainerEvent({type, cheat, message, gameAvailable_, enabled, value});
}

bool DemonStarTrainer::isValidCheat(CheatId cheat) const
{
    return cheat == CheatId::InfinitePlanes || cheat == CheatId::InfiniteNukes || cheat == CheatId::InfiniteHealth;
}

bool DemonStarTrainer::readValue(CheatId cheat, std::int32_t &value) const
{
    const std::uintptr_t offset = offsetFor(cheat);
    return offset != 0 && processMemory_->readInt32(offset, value);
}

bool DemonStarTrainer::writeValue(CheatId cheat, std::int32_t value) const
{
    const std::uintptr_t offset = offsetFor(cheat);
    return offset != 0 && processMemory_->writeInt32(offset, value);
}

DemonStarTrainer::LockState &DemonStarTrainer::stateFor(CheatId cheat)
{
    switch (cheat) {
    case CheatId::InfinitePlanes:
        return planes_;
    case CheatId::InfiniteNukes:
        return nukes_;
    case CheatId::InfiniteHealth:
        return health_;
    case CheatId::None:
        break;
    }

    throw std::invalid_argument("invalid cheat id");
}

const DemonStarTrainer::LockState &DemonStarTrainer::stateFor(CheatId cheat) const
{
    switch (cheat) {
    case CheatId::InfinitePlanes:
        return planes_;
    case CheatId::InfiniteNukes:
        return nukes_;
    case CheatId::InfiniteHealth:
        return health_;
    case CheatId::None:
        break;
    }

    throw std::invalid_argument("invalid cheat id");
}

} // namespace demonstar
