#include "DemonStarTrainer.h"
/*
 * DemonStar 修改器核心实现
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * 这里实现的是纯业务逻辑，不包含 Qt 或窗口代码。UI 只需要周期性调用 tick，
 * 并通过 ITrainerListener 接收状态变化即可。
 */

#include "WinProcessMemory.h"

#include <limits>
#include <stdexcept>

namespace demonstar {
namespace {

constexpr std::uintptr_t HealthOffset = 0x14C7950;
constexpr std::uintptr_t PlanesOffset = 0x14C7954;
constexpr std::uintptr_t NukesOffset = 0x14C80E4;
constexpr wchar_t ProcessName[] = L"demonstar.exe";
constexpr std::int32_t EasyModeNukeLimit = 5;
constexpr std::int32_t EasyModeHealthLimit = 160;

// 将内部枚举转换成用户可读的中文名称，用于事件 message。
const char *valueName(TrainerValueId valueId)
{
    switch (valueId) {
    case TrainerValueId::Planes:
        return "飞机数量";
    case TrainerValueId::Nukes:
        return "核弹数量";
    case TrainerValueId::Health:
        return "生命值";
    case TrainerValueId::None:
        return "修改项";
    }

    return "修改项";
}

const char *easyModeName(TrainerEasyModeLevel level)
{
    switch (level) {
    case TrainerEasyModeLevel::Off:
        return "关闭";
    case TrainerEasyModeLevel::Low:
        return "低档";
    case TrainerEasyModeLevel::Medium:
        return "中档";
    case TrainerEasyModeLevel::High:
        return "高档";
    }

    return "未知档位";
}

bool isValidEasyModeLevel(TrainerEasyModeLevel level)
{
    switch (level) {
    case TrainerEasyModeLevel::Off:
    case TrainerEasyModeLevel::Low:
    case TrainerEasyModeLevel::Medium:
    case TrainerEasyModeLevel::High:
        return true;
    }

    return false;
}

std::chrono::seconds easyModeNukeInterval(TrainerEasyModeLevel level)
{
    switch (level) {
    case TrainerEasyModeLevel::Low:
        return std::chrono::seconds(15);
    case TrainerEasyModeLevel::Medium:
        return std::chrono::seconds(10);
    case TrainerEasyModeLevel::High:
        return std::chrono::seconds(5);
    case TrainerEasyModeLevel::Off:
        break;
    }

    return std::chrono::seconds::max();
}

std::chrono::seconds easyModeHealthInterval(TrainerEasyModeLevel level)
{
    switch (level) {
    case TrainerEasyModeLevel::Low:
        return std::chrono::seconds(3);
    case TrainerEasyModeLevel::Medium:
        return std::chrono::seconds(2);
    case TrainerEasyModeLevel::High:
        return std::chrono::seconds(1);
    case TrainerEasyModeLevel::Off:
        break;
    }

    return std::chrono::seconds::max();
}

// 把业务上的数值项映射到实际内存偏移。
// 注意：这些偏移是相对于 demonstar.exe 主模块基址的偏移。
std::uintptr_t offsetFor(TrainerValueId valueId)
{
    switch (valueId) {
    case TrainerValueId::Planes:
        return PlanesOffset;
    case TrainerValueId::Nukes:
        return NukesOffset;
    case TrainerValueId::Health:
        return HealthOffset;
    case TrainerValueId::None:
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
    // 可复用模块允许测试代码注入 IProcessMemory。
    // 为了避免 Release 环境下 nullptr 导致崩溃，这里做兜底。
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
    // tick 是核心状态机的驱动入口：
    // 1. 确保游戏进程存在；
    // 2. 发现退出/重启时统一清理；
    // 3. 对启用锁定的数值项进行写回。
    const bool wasGameAvailable = gameAvailable_;
    const bool hadEnabledLock = planes_.enabled || nukes_.enabled || health_.enabled;
    const std::uint32_t previousProcessId = lastProcessId_;

    gameAvailable_ = ensureGameProcess();

    if (!gameAvailable_) {
        // 如果之前检测到游戏或仍有锁定项，说明这是一次状态变化，需要通知 UI 清空状态。
        if (wasGameAvailable || hadEnabledLock) {
            handleDetached(false);
        }
        return;
    }

    const bool processChanged = wasGameAvailable && previousProcessId != 0 && lastProcessId_ != previousProcessId;
    if (processChanged) {
        // 进程 ID 变化说明游戏重启了。旧锁定值不一定适用于新进程，必须全部关闭。
        handleDetached(true);
        return;
    }

    if (!wasGameAvailable) {
        notify(TrainerEventType::GameAttached, TrainerValueId::None, "已检测到 demonstar.exe，可以启用修改项");
    }

    if (easyModeLevel_ != TrainerEasyModeLevel::Off) {
        updateEasyMode();
        return;
    }

    if (planes_.enabled) {
        updateValueLock(TrainerValueId::Planes);
    }

    if (nukes_.enabled) {
        updateValueLock(TrainerValueId::Nukes);
    }

    if (health_.enabled) {
        updateValueLock(TrainerValueId::Health);
    }

    if (!planes_.enabled && !nukes_.enabled && !health_.enabled) {
        resetState(TrainerValueId::Planes);
        resetState(TrainerValueId::Nukes);
        resetState(TrainerValueId::Health);
    }
}

bool DemonStarTrainer::setValueLocked(TrainerValueId valueId, bool enabled)
{
    if (!isValidValue(valueId)) {
        return false;
    }

    if (enabled && easyModeLevel_ != TrainerEasyModeLevel::Off) {
        notify(TrainerEventType::EasyModeChanged,
               valueId,
               std::string("低难度模式已开启，不能启用") + valueName(valueId) + "锁定");
        return false;
    }

    LockState &state = stateFor(valueId);
    // 每次切换锁定都清理旧锁定值，避免复用过期数据。
    resetState(valueId);

    if (enabled) {
        if (!ensureGameProcess()) {
            gameAvailable_ = false;
            state.enabled = false;
            notify(TrainerEventType::AttachFailed, valueId, std::string("游戏未启动，无法启用") + valueName(valueId));
            return false;
        }

        std::int32_t currentValue = 0;
        if (!readValue(valueId, currentValue)) {
            // 开启锁定时必须先读取当前值作为锁定目标；读不到就不能进入锁定状态。
            handleMemoryAccessFailed(valueId,
                                     TrainerEventType::ReadFailed,
                                     std::string("读取") + valueName(valueId) + "失败，修改项已关闭");
            return false;
        }

        gameAvailable_ = true;
        state.enabled = true;
        state.hasLockedValue = true;
        // 这里的语义是“锁当前游戏值”，输入框不会影响锁定目标。
        state.lockedValue = currentValue;
        notify(TrainerEventType::ValueLocked, valueId, std::string(valueName(valueId)) + "已锁定到 " + std::to_string(currentValue), currentValue);
        return state.enabled;
    }

    state.enabled = false;
    notify(TrainerEventType::ValueUnlocked, valueId, std::string(valueName(valueId)) + "已关闭");
    if (!planes_.enabled && !nukes_.enabled && !health_.enabled) {
        processMemory_->detach();
        lastProcessId_ = 0;
    }
    return true;
}

bool DemonStarTrainer::addValue(TrainerValueId valueId, int amount)
{
    if (!isValidValue(valueId) || amount <= 0) {
        return false;
    }

    if (!ensureGameProcess()) {
        gameAvailable_ = false;
        notify(TrainerEventType::AttachFailed, valueId, std::string("游戏未启动，无法增加") + valueName(valueId));
        return false;
    }

    LockState &state = stateFor(valueId);
    std::int32_t currentValue = 0;
    if (!readValue(valueId, currentValue)) {
        // 点击“增加”时也要先读取当前值，用来确认目标地址仍然可读。
        handleMemoryAccessFailed(valueId,
                                 TrainerEventType::ReadFailed,
                                 std::string("读取") + valueName(valueId) + "失败，无法增加");
        return false;
    }

    // 未锁定：以游戏当前值为基准增加。
    // 已锁定：以当前锁定值为基准增加，并在写入成功后更新锁定目标。
    const std::int32_t baseValue = state.enabled && state.hasLockedValue ? state.lockedValue : currentValue;
    const auto calculatedValue = static_cast<std::int64_t>(baseValue) + static_cast<std::int64_t>(amount);
    if (calculatedValue > std::numeric_limits<std::int32_t>::max()) {
        // 不做 clamp，直接拒绝写入，避免把异常值写进游戏。
        notify(TrainerEventType::WriteFailed, valueId, std::string(valueName(valueId)) + "数值超出范围，未写入");
        return false;
    }

    const auto newValue = static_cast<std::int32_t>(calculatedValue);
    if (!writeValue(valueId, newValue)) {
        handleMemoryAccessFailed(valueId,
                                 TrainerEventType::WriteFailed,
                                 std::string("写入") + valueName(valueId) + "失败，修改项已关闭");
        return false;
    }

    gameAvailable_ = true;
    if (state.enabled) {
        // 锁定状态下“增加”必须联动锁定值，否则下一次 tick 会把新值写回旧锁定值。
        state.lockedValue = newValue;
        state.hasLockedValue = true;
        notify(TrainerEventType::LockUpdated, valueId, std::string(valueName(valueId)) + "已增加并锁定到 " + std::to_string(newValue), newValue);
    } else {
        notify(TrainerEventType::ValueAdded, valueId, std::string(valueName(valueId)) + "已增加到 " + std::to_string(newValue), newValue);
    }

    return true;
}

bool DemonStarTrainer::isValueLocked(TrainerValueId valueId) const
{
    if (!isValidValue(valueId)) {
        return false;
    }

    return stateFor(valueId).enabled;
}

bool DemonStarTrainer::setEasyModeLevel(TrainerEasyModeLevel level)
{
    if (!isValidEasyModeLevel(level)) {
        return false;
    }

    if (level == TrainerEasyModeLevel::Off) {
        if (easyModeLevel_ != TrainerEasyModeLevel::Off) {
            disableEasyMode();
            notify(TrainerEventType::EasyModeChanged, TrainerValueId::None, "低难度模式已关闭");
        }
        if (!planes_.enabled && !nukes_.enabled && !health_.enabled) {
            processMemory_->detach();
            lastProcessId_ = 0;
        }
        return true;
    }

    disableAllLocks();
    easyModeLevel_ = level;
    resetEasyModeTimers();
    notify(TrainerEventType::EasyModeChanged,
           TrainerValueId::None,
           std::string("低难度模式已开启：") + easyModeName(level));
    return true;
}

TrainerEasyModeLevel DemonStarTrainer::easyModeLevel() const
{
    return easyModeLevel_;
}

bool DemonStarTrainer::isGameAvailable() const
{
    return gameAvailable_;
}

void DemonStarTrainer::shutdown()
{
    disableAllLocks();
    disableEasyMode();
    processMemory_->detach();
    gameAvailable_ = false;
    lastProcessId_ = 0;
}

bool DemonStarTrainer::ensureGameProcess()
{
    // 句柄仍然有效时直接复用，减少每次 tick 都枚举进程的成本。
    if (processMemory_->isAttached()) {
        lastProcessId_ = processMemory_->processId();
        return true;
    }

    // 旧句柄无效时先清理，再尝试重新查找 demonstar.exe。
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
    // 游戏退出或重启时不能保留任何锁定值，因为旧地址和旧数值都可能失效。
    disableAllLocks();
    resetEasyModeTimers();
    processMemory_->detach();
    gameAvailable_ = false;
    lastProcessId_ = 0;
    notify(restarted ? TrainerEventType::GameRestarted : TrainerEventType::GameDetached,
           TrainerValueId::None,
           restarted ? "游戏进程已重启，修改项已关闭" : "游戏未启动或已退出，修改项已关闭");
}

void DemonStarTrainer::handleMemoryAccessFailed(TrainerValueId valueId, TrainerEventType type, const std::string &message)
{
    // 内存读写失败通常有两类原因：
    // 1. 游戏已经退出或句柄失效；
    // 2. 游戏仍在，但偏移地址失效或权限/页面状态异常。
    // 第一类按游戏退出处理；第二类关闭全部锁定，避免继续向错误地址写入。
    if (!processMemory_->isAttached()) {
        handleDetached(false);
        return;
    }

    // 进程仍在但读写失败时，保留进程检测能力，但禁用所有锁定项。
    disableAllLocks();
    disableEasyMode();
    // 使用 TrainerValueId::None 通知 UI 重新同步所有复选框。
    notify(type, TrainerValueId::None, message);
}

void DemonStarTrainer::updateValueLock(TrainerValueId valueId)
{
    updateLockedValue(valueId);
}

void DemonStarTrainer::updateEasyMode()
{
    if (easyModeLevel_ == TrainerEasyModeLevel::Off) {
        return;
    }

    if (!ensureGameProcess()) {
        handleDetached(false);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (easyLastNukeTick_ == std::chrono::steady_clock::time_point{}) {
        easyLastNukeTick_ = now;
    }
    if (easyLastHealthTick_ == std::chrono::steady_clock::time_point{}) {
        easyLastHealthTick_ = now;
    }

    std::int32_t healthValue = 0;
    if (!readValue(TrainerValueId::Health, healthValue)) {
        handleMemoryAccessFailed(TrainerValueId::Health,
                                 TrainerEventType::ReadFailed,
                                 "读取生命值失败，低难度模式已关闭");
        return;
    }

    if (healthValue > EasyModeHealthLimit) {
        if (!writeValue(TrainerValueId::Health, EasyModeHealthLimit)) {
            handleMemoryAccessFailed(TrainerValueId::Health,
                                     TrainerEventType::WriteFailed,
                                     "写入生命值失败，低难度模式已关闭");
            return;
        }
        healthValue = EasyModeHealthLimit;
    }

    if (now - easyLastNukeTick_ >= easyModeNukeInterval(easyModeLevel_)) {
        easyLastNukeTick_ = now;
        std::int32_t nukeValue = 0;
        if (!readValue(TrainerValueId::Nukes, nukeValue)) {
            handleMemoryAccessFailed(TrainerValueId::Nukes,
                                     TrainerEventType::ReadFailed,
                                     "读取核弹数量失败，低难度模式已关闭");
            return;
        }

        if (nukeValue < EasyModeNukeLimit) {
            const std::int32_t newNukeValue = nukeValue + 1;
            if (!writeValue(TrainerValueId::Nukes, newNukeValue)) {
                handleMemoryAccessFailed(TrainerValueId::Nukes,
                                         TrainerEventType::WriteFailed,
                                         "写入核弹数量失败，低难度模式已关闭");
                return;
            }
        }
    }

    if (now - easyLastHealthTick_ >= easyModeHealthInterval(easyModeLevel_)) {
        easyLastHealthTick_ = now;
        if (healthValue < EasyModeHealthLimit) {
            const std::int32_t newHealthValue = healthValue + 1;
            if (!writeValue(TrainerValueId::Health, newHealthValue)) {
                handleMemoryAccessFailed(TrainerValueId::Health,
                                         TrainerEventType::WriteFailed,
                                         "写入生命值失败，低难度模式已关闭");
            }
        }
    }
}

void DemonStarTrainer::updateLockedValue(TrainerValueId valueId)
{
    if (!ensureGameProcess()) {
        handleDetached(false);
        return;
    }

    LockState &state = stateFor(valueId);
    if (!state.enabled || !state.hasLockedValue) {
        // 只有已经成功保存锁定目标的项才需要写回。
        return;
    }

    std::int32_t currentValue = 0;
    if (!readValue(valueId, currentValue)) {
        handleMemoryAccessFailed(valueId,
                                 TrainerEventType::ReadFailed,
                                 std::string("读取") + valueName(valueId) + "失败，修改项已关闭");
        return;
    }

    if (currentValue == state.lockedValue) {
        // 游戏值没有偏离锁定值，无需写入，减少对目标进程的写操作。
        return;
    }

    // 严格锁定：不管游戏值变大还是变小，只要和 lockedValue 不一致就写回。
    if (!writeValue(valueId, state.lockedValue)) {
        handleMemoryAccessFailed(valueId,
                                 TrainerEventType::WriteFailed,
                                 std::string("写入") + valueName(valueId) + "失败，修改项已关闭");
    }
}

void DemonStarTrainer::resetState(TrainerValueId valueId)
{
    LockState &state = stateFor(valueId);
    // 保留 enabled 是为了让调用方可以在清空旧锁定值后继续决定是否开启。
    const bool enabled = state.enabled;
    state = {};
    state.enabled = enabled;
}

void DemonStarTrainer::disableAllLocks()
{
    planes_ = {};
    nukes_ = {};
    health_ = {};
}

void DemonStarTrainer::disableEasyMode()
{
    easyModeLevel_ = TrainerEasyModeLevel::Off;
    resetEasyModeTimers();
}

void DemonStarTrainer::resetEasyModeTimers()
{
    easyLastNukeTick_ = {};
    easyLastHealthTick_ = {};
}

void DemonStarTrainer::notify(TrainerEventType type, TrainerValueId valueId, const std::string &message, int value)
{
    if (listener_ == nullptr) {
        return;
    }

    // valueId 为 None 时表示全局事件，locked 没有单项意义，因此固定为 false。
    const bool enabled = valueId != TrainerValueId::None && isValueLocked(valueId);
    listener_->onTrainerEvent({type, valueId, message, gameAvailable_, enabled, value});
}

bool DemonStarTrainer::isValidValue(TrainerValueId valueId) const
{
    return valueId == TrainerValueId::Planes || valueId == TrainerValueId::Nukes || valueId == TrainerValueId::Health;
}

bool DemonStarTrainer::readValue(TrainerValueId valueId, std::int32_t &value) const
{
    const std::uintptr_t offset = offsetFor(valueId);
    // offset 为 0 表示传入了无效数值项，直接失败，不访问目标进程。
    return offset != 0 && processMemory_->readInt32(offset, value);
}

bool DemonStarTrainer::writeValue(TrainerValueId valueId, std::int32_t value) const
{
    const std::uintptr_t offset = offsetFor(valueId);
    // 统一通过 IProcessMemory 写入，便于测试和替换底层实现。
    return offset != 0 && processMemory_->writeInt32(offset, value);
}

DemonStarTrainer::LockState &DemonStarTrainer::stateFor(TrainerValueId valueId)
{
    switch (valueId) {
    case TrainerValueId::Planes:
        return planes_;
    case TrainerValueId::Nukes:
        return nukes_;
    case TrainerValueId::Health:
        return health_;
    case TrainerValueId::None:
        break;
    }

    throw std::invalid_argument("invalid trainer value id");
}

const DemonStarTrainer::LockState &DemonStarTrainer::stateFor(TrainerValueId valueId) const
{
    switch (valueId) {
    case TrainerValueId::Planes:
        return planes_;
    case TrainerValueId::Nukes:
        return nukes_;
    case TrainerValueId::Health:
        return health_;
    case TrainerValueId::None:
        break;
    }

    throw std::invalid_argument("invalid trainer value id");
}

} // namespace demonstar
