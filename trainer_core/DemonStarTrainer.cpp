#include "DemonStarTrainer.h"

#include "WinProcessMemory.h"

#include <cassert>

namespace demonstar {
namespace {

constexpr std::uintptr_t InfiniteHealthOffset = 0x14C7950;
constexpr std::uintptr_t InfinitePlanesOffset = 0x14C7954;
constexpr std::uintptr_t InfiniteNukesOffset = 0x14C80E4;
constexpr std::int32_t InfiniteHealthLockedValue = 160;
constexpr std::int32_t CheatValueLimit = 10;
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

} // namespace

DemonStarTrainer::DemonStarTrainer()
    : DemonStarTrainer(std::make_unique<WinProcessMemory>())
{
}

DemonStarTrainer::DemonStarTrainer(std::unique_ptr<IProcessMemory> processMemory)
    : processMemory_(std::move(processMemory))
{
    assert(processMemory_ != nullptr);
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
    if (cheat == CheatId::None) {
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

        gameAvailable_ = true;
        state.enabled = true;
        notify(TrainerEventType::CheatEnabled, cheat, std::string(cheatName(cheat)) + "已开启");
        updateCheat(cheat);
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

bool DemonStarTrainer::isCheatEnabled(CheatId cheat) const
{
    if (cheat == CheatId::None) {
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

void DemonStarTrainer::updateCheat(CheatId cheat)
{
    switch (cheat) {
    case CheatId::InfinitePlanes:
        updateLimitedLock(cheat, planes_, InfinitePlanesOffset);
        break;
    case CheatId::InfiniteNukes:
        updateLimitedLock(cheat, nukes_, InfiniteNukesOffset);
        break;
    case CheatId::InfiniteHealth:
        updateHealth();
        break;
    case CheatId::None:
        break;
    }
}

void DemonStarTrainer::updateLimitedLock(CheatId cheat, LockState &state, std::uintptr_t offset)
{
    if (!ensureGameProcess()) {
        handleDetached(false);
        return;
    }

    std::int32_t currentValue = 0;
    if (!processMemory_->readInt32(offset, currentValue)) {
        resetState(cheat);
        state.enabled = false;
        notify(TrainerEventType::ReadFailed, cheat, std::string("读取") + cheatName(cheat) + "失败，修改项已关闭");
        return;
    }

    if (currentValue > CheatValueLimit) {
        state.hasLockedValue = false;
        if (!state.pausedAboveLimit) {
            state.pausedAboveLimit = true;
            notify(TrainerEventType::CheatPaused, cheat, std::string(cheatName(cheat)) + "数值超过 10，暂停写入");
        }
        return;
    }

    if (!state.hasLockedValue || state.pausedAboveLimit) {
        state.lockedValue = currentValue;
        state.hasLockedValue = true;
        state.pausedAboveLimit = false;
        notify(TrainerEventType::CheatLocked, cheat, std::string(cheatName(cheat)) + "已锁定");
        return;
    }

    if (currentValue < state.lockedValue) {
        if (!processMemory_->writeInt32(offset, state.lockedValue)) {
            resetState(cheat);
            state.enabled = false;
            notify(TrainerEventType::WriteFailed, cheat, std::string("写入") + cheatName(cheat) + "失败，修改项已关闭");
        }
        return;
    }

    if (currentValue > state.lockedValue) {
        state.lockedValue = currentValue;
    }
}

void DemonStarTrainer::updateHealth()
{
    if (!ensureGameProcess()) {
        handleDetached(false);
        return;
    }

    std::int32_t currentValue = 0;
    if (!processMemory_->readInt32(InfiniteHealthOffset, currentValue)) {
        resetState(CheatId::InfiniteHealth);
        health_.enabled = false;
        notify(TrainerEventType::ReadFailed, CheatId::InfiniteHealth, "读取无限生命值失败，修改项已关闭");
        return;
    }

    if (!health_.hasLockedValue) {
        health_.lockedValue = InfiniteHealthLockedValue;
        health_.hasLockedValue = true;
        notify(TrainerEventType::CheatLocked, CheatId::InfiniteHealth, "无限生命值已锁定");
    }

    if (currentValue < health_.lockedValue
        && !processMemory_->writeInt32(InfiniteHealthOffset, health_.lockedValue)) {
        resetState(CheatId::InfiniteHealth);
        health_.enabled = false;
        notify(TrainerEventType::WriteFailed, CheatId::InfiniteHealth, "写入无限生命值失败，修改项已关闭");
        return;
    }

    health_.lockedValue = InfiniteHealthLockedValue;
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

void DemonStarTrainer::notify(TrainerEventType type, CheatId cheat, const std::string &message)
{
    if (listener_ == nullptr) {
        return;
    }

    const bool enabled = cheat != CheatId::None && isCheatEnabled(cheat);
    listener_->onTrainerEvent({type, cheat, message, gameAvailable_, enabled});
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

    return planes_;
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

    return planes_;
}

} // namespace demonstar
