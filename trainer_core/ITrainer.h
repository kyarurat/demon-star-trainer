#ifndef DEMONSTAR_ITRAINER_H
#define DEMONSTAR_ITRAINER_H
/*
 * DemonStar 修改器控制接口
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * 该接口是外部程序使用 trainer_core 的主要入口，保持无 Qt 依赖。
 */

#include "ITrainerListener.h"

namespace demonstar {

class ITrainer
{
public:
    virtual ~ITrainer() = default;

    // 设置事件监听器。传入 nullptr 可取消回调；监听器生命周期由调用方管理。
    virtual void setListener(ITrainerListener *listener) = 0;

    // 周期性轮询入口。调用方应定时调用，当前 UI 默认每 100ms 调一次。
    // tick 会处理进程检测、游戏退出/重启检测、锁定值写回。
    virtual void tick() = 0;

    // 开启/关闭某个数值项的锁定。
    // 开启时会读取游戏当前值作为锁定目标；关闭时不会还原游戏内数值。
    virtual bool setValueLocked(TrainerValueId valueId, bool enabled) = 0;

    // 将某个数值项增加 amount。
    // 未锁定时写入“当前游戏值 + amount”；锁定时写入“当前锁定值 + amount”并更新锁定目标。
    virtual bool addValue(TrainerValueId valueId, int amount) = 0;

    // 查询某项当前是否处于锁定状态。
    virtual bool isValueLocked(TrainerValueId valueId) const = 0;

    // 设置低难度模式档位。Off 表示关闭；开启后会和严格锁定功能互斥。
    virtual bool setEasyModeLevel(TrainerEasyModeLevel level) = 0;

    // 查询当前低难度模式档位。
    virtual TrainerEasyModeLevel easyModeLevel() const = 0;

    // 查询核心当前是否认为 demonstar.exe 可用。
    virtual bool isGameAvailable() const = 0;

    // 主动释放资源。析构时也会调用，但宿主程序可以提前显式关闭。
    virtual void shutdown() = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_ITRAINER_H
