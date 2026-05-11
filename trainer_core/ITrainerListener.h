#ifndef DEMONSTAR_ITRAINER_LISTENER_H
#define DEMONSTAR_ITRAINER_LISTENER_H
/*
 * DemonStar 修改器事件监听接口
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * UI、日志或其他宿主程序通过实现该接口接收核心事件。
 */

#include "TrainerTypes.h"

namespace demonstar {

class ITrainerListener
{
public:
    virtual ~ITrainerListener() = default;

    // 核心状态变化、读写成功/失败、进程启动/退出等情况都会通过该回调通知外部。
    // 回调在调用核心接口的同一线程触发，当前 Qt 程序中就是 UI 线程定时器/按钮线程。
    virtual void onTrainerEvent(const TrainerEvent &event) = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_ITRAINER_LISTENER_H
