#ifndef DEMONSTAR_IPROCESS_MEMORY_H
#define DEMONSTAR_IPROCESS_MEMORY_H
/*
 * 进程内存访问抽象接口
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * DemonStarTrainer 不直接依赖 WinAPI，而是通过该接口读写目标进程内存。
 * 这样可以在测试中注入假实现，也方便后续替换平台实现。
 */

#include <cstdint>
#include <string>

namespace demonstar {

class IProcessMemory
{
public:
    virtual ~IProcessMemory() = default;

    // 附加到指定进程名对应的进程，并记录主模块基址。
    virtual bool attach(const std::wstring &processName) = 0;

    // 判断当前保存的进程句柄是否仍然有效且进程仍在运行。
    virtual bool isAttached() const = 0;

    // 返回当前附加的进程 ID；未附加时返回 0。
    virtual std::uint32_t processId() const = 0;

    // 关闭进程句柄并清理缓存的进程 ID、模块基址等状态。
    virtual void detach() = 0;

    // 以“模块基址 + offset”为地址读取 32 位整数。
    virtual bool readInt32(std::uintptr_t offset, std::int32_t &value) const = 0;

    // 以“模块基址 + offset”为地址写入 32 位整数。
    virtual bool writeInt32(std::uintptr_t offset, std::int32_t value) const = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_IPROCESS_MEMORY_H
