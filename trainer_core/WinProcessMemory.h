#ifndef DEMONSTAR_WIN_PROCESS_MEMORY_H
#define DEMONSTAR_WIN_PROCESS_MEMORY_H
/*
 * Windows 进程内存访问实现
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * 该类封装 WinAPI 的进程枚举、模块基址查找、ReadProcessMemory 和 WriteProcessMemory。
 */

#include "IProcessMemory.h"

namespace demonstar {

class WinProcessMemory final : public IProcessMemory
{
public:
    // 析构时自动 detach，确保进程句柄不会泄露。
    ~WinProcessMemory() override;

    bool attach(const std::wstring &processName) override;
    bool isAttached() const override;
    std::uint32_t processId() const override;
    void detach() override;
    bool readInt32(std::uintptr_t offset, std::int32_t &value) const override;
    bool writeInt32(std::uintptr_t offset, std::int32_t value) const override;

private:
    // 预留的内部检查函数声明，当前实现主要通过 isAttached() 完成活性检查。
    bool ensureStillActive();

    // 为了避免公共头文件暴露 windows.h，这里用 void* 保存 HANDLE。
    void *processHandle_ = nullptr;
    // 当前附加进程 ID；用于检测进程重启。
    std::uint32_t processId_ = 0;
    // 目标模块基址，所有偏移都以该基址为起点。
    std::uintptr_t baseAddress_ = 0;
    // 当前附加的进程名，便于调试和后续扩展。
    std::wstring processName_;
};

} // namespace demonstar

#endif // DEMONSTAR_WIN_PROCESS_MEMORY_H
