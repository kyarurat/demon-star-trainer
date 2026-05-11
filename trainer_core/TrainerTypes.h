#ifndef DEMONSTAR_TRAINER_TYPES_H
#define DEMONSTAR_TRAINER_TYPES_H
/*
 * DemonStar 修改器核心公共类型定义
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * 本文件只包含标准库类型，不依赖 Qt 或具体 UI 框架，方便其他项目直接复用。
 */

#include <string>

namespace demonstar {

// 可操作的游戏数值项。操作本身由 ITrainer 方法和 TrainerEventType 表达。
enum class TrainerValueId {
    Planes,
    Nukes,
    Health,
    // None 用于表示事件不针对单个数值项，例如游戏进程启动、退出或全部锁定被关闭。
    None
};

// 修改器核心向外部 UI/日志层报告的事件类型。
enum class TrainerEventType {
    // 已成功附加到 demonstar.exe。
    GameAttached,
    // 游戏进程不可用或已退出，核心已清理句柄和锁定状态。
    GameDetached,
    // 检测到游戏进程 ID 变化，认为游戏已重启，核心已关闭所有锁定。
    GameRestarted,
    // 某个锁定项被用户关闭。
    ValueUnlocked,
    // 某个数值项已读取当前游戏值并锁定。
    ValueLocked,
    // 锁定状态下增加数值，锁定目标也同步更新。
    LockUpdated,
    // 未锁定状态下完成一次数值增加。
    ValueAdded,
    // 读取游戏内存失败。
    ReadFailed,
    // 写入游戏内存失败，或写入前发现数值越界。
    WriteFailed,
    // 未能附加到游戏进程。
    AttachFailed
};

// 事件载荷。UI 层只需要根据这里的状态同步控件和显示 message。
struct TrainerEvent {
    // 事件类型。
    TrainerEventType type = TrainerEventType::AttachFailed;
    // 事件对应的数值项；None 表示需要整体同步或非单项事件。
    TrainerValueId valueId = TrainerValueId::None;
    // 面向用户或日志的说明文本，核心内部使用 UTF-8 字符串。
    std::string message;
    // 事件发生后核心认为游戏是否可用。
    bool gameAvailable = false;
    // 如果 valueId 不是 None，表示该项事件发生后是否仍处于锁定状态。
    bool locked = false;
    // 事件涉及的结果数值，例如增加后的值或锁定到的值；无数值时为 0。
    int value = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_TRAINER_TYPES_H
