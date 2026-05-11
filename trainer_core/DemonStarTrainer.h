#ifndef DEMONSTAR_TRAINER_H
#define DEMONSTAR_TRAINER_H
/*
 * DemonStar 修改器核心实现声明
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * 本类负责具体业务状态机：检测游戏进程、增加数值、锁定当前值、异常恢复。
 */

#include "IProcessMemory.h"
#include "ITrainer.h"

#include <memory>

namespace demonstar {

class DemonStarTrainer final : public ITrainer
{
public:
    // 使用默认 Windows 进程内存访问实现。
    DemonStarTrainer();

    // 注入进程内存访问实现。传入 nullptr 时会自动回退到 WinProcessMemory。
    explicit DemonStarTrainer(std::unique_ptr<IProcessMemory> processMemory);
    ~DemonStarTrainer() override;

    void setListener(ITrainerListener *listener) override;
    void tick() override;
    bool setValueLocked(TrainerValueId valueId, bool enabled) override;
    bool addValue(TrainerValueId valueId, int amount) override;
    bool isValueLocked(TrainerValueId valueId) const override;
    bool isGameAvailable() const override;
    void shutdown() override;

private:
    // 单个数值项的锁定状态。
    struct LockState {
        // 是否正在锁定该数值项。
        bool enabled = false;
        // 是否已经成功读取并保存锁定目标值。
        bool hasLockedValue = false;
        // 当前锁定目标。tick 时发现游戏值偏离该值，就会写回。
        std::int32_t lockedValue = 0;
    };

    // 确保已经附加到 demonstar.exe；未附加时尝试重新查找和打开进程。
    bool ensureGameProcess();

    // 处理游戏退出或重启后的统一清理逻辑。
    void handleDetached(bool restarted);

    // 处理读写内存失败。进程已失效时 detach；进程仍在但地址异常时关闭全部锁定。
    void handleMemoryAccessFailed(TrainerValueId valueId, TrainerEventType type, const std::string &message);

    // 更新单个锁定项，目前内部转发到 updateLockedValue。
    void updateValueLock(TrainerValueId valueId);

    // 锁定轮询：如果游戏内当前值和 lockedValue 不一致，就写回 lockedValue。
    void updateLockedValue(TrainerValueId valueId);

    // 重置单项锁定缓存，但保留 enabled 标志，便于调用方决定是否继续锁定。
    void resetState(TrainerValueId valueId);

    // 关闭所有锁定项并清空锁定值。
    void disableAllLocks();

    // 向监听器发送事件；没有监听器时直接忽略。
    void notify(TrainerEventType type, TrainerValueId valueId, const std::string &message, int value = 0);

    // 判断 TrainerValueId 是否是可读写的真实数值项。
    bool isValidValue(TrainerValueId valueId) const;

    // 根据 TrainerValueId 读取游戏内对应数值。
    bool readValue(TrainerValueId valueId, std::int32_t &value) const;

    // 根据 TrainerValueId 写入游戏内对应数值。
    bool writeValue(TrainerValueId valueId, std::int32_t value) const;

    // 获取指定数值项的状态对象；无效 ID 会抛异常，避免误改其它项。
    LockState &stateFor(TrainerValueId valueId);
    const LockState &stateFor(TrainerValueId valueId) const;

    // 进程读写实现，默认是 WinProcessMemory，也可以注入测试实现。
    std::unique_ptr<IProcessMemory> processMemory_;
    // 外部事件监听器，不拥有其生命周期。
    ITrainerListener *listener_ = nullptr;
    // 核心当前认为游戏是否可用。
    bool gameAvailable_ = false;
    // 上一次附加到的进程 ID，用于检测游戏重启。
    std::uint32_t lastProcessId_ = 0;
    // 三个数值项各自的锁定状态。
    LockState planes_;
    LockState nukes_;
    LockState health_;
};

} // namespace demonstar

#endif // DEMONSTAR_TRAINER_H
