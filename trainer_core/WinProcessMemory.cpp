#include "WinProcessMemory.h"
/*
 * Windows 进程内存访问实现
 * Author: KyaruRat
 * Date: 20260511
 * Version: V1.0
 *
 * 该文件是 trainer_core 唯一直接包含 windows.h 的位置。
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace demonstar {
namespace {

// Windows 进程/模块名比较需要忽略大小写。
bool executableNameEquals(const wchar_t *name, const std::wstring &expected)
{
    return _wcsicmp(name, expected.c_str()) == 0;
}

// 在系统进程快照中查找指定 exe 名称的进程 ID。
std::uint32_t findProcessId(const std::wstring &processName)
{
    // TH32CS_SNAPPROCESS 会创建一个进程列表快照，后续遍历该快照即可。
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    std::uint32_t processId = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            // DemonStar 目标进程名是 demonstar.exe，比较时忽略大小写。
            if (executableNameEquals(entry.szExeFile, processName)) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processId;
}

// 查找目标进程中指定模块的基址。正常情况下模块名就是 demonstar.exe。
std::uintptr_t findModuleBaseAddress(std::uint32_t processId, const std::wstring &moduleName)
{
    // 同时使用 TH32CS_SNAPMODULE32，兼容 64 位进程枚举 32 位模块的情况。
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    std::uintptr_t firstModuleBaseAddress = 0;
    std::uintptr_t moduleBaseAddress = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const auto currentBaseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            if (firstModuleBaseAddress == 0) {
                // 兜底：如果没有找到同名模块，就使用第一个模块基址。
                firstModuleBaseAddress = currentBaseAddress;
            }

            if (executableNameEquals(entry.szModule, moduleName)) {
                moduleBaseAddress = currentBaseAddress;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return moduleBaseAddress != 0 ? moduleBaseAddress : firstModuleBaseAddress;
}

} // namespace

WinProcessMemory::~WinProcessMemory()
{
    // 保证异常退出或宿主忘记调用 detach 时也能关闭句柄。
    detach();
}

bool WinProcessMemory::attach(const std::wstring &processName)
{
    // 如果已有有效句柄，直接复用，避免重复 OpenProcess。
    if (isAttached()) {
        return true;
    }

    // 清理旧状态后再查找，避免旧 PID/基址残留。
    detach();

    const std::uint32_t foundProcessId = findProcessId(processName);
    if (foundProcessId == 0) {
        return false;
    }

    // 需要读写目标进程内存，同时保留 QUERY 权限用于检查进程状态。
    const HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                                             FALSE,
                                             foundProcessId);
    if (processHandle == nullptr) {
        return false;
    }

    const std::uintptr_t foundBaseAddress = findModuleBaseAddress(foundProcessId, processName);
    if (foundBaseAddress == 0) {
        // 没有基址就无法把偏移转换成绝对地址，必须关闭句柄并失败返回。
        CloseHandle(processHandle);
        return false;
    }

    processHandle_ = processHandle;
    processId_ = foundProcessId;
    baseAddress_ = foundBaseAddress;
    processName_ = processName;
    return true;
}

bool WinProcessMemory::isAttached() const
{
    if (processHandle_ == nullptr) {
        return false;
    }

    // STILL_ACTIVE 表示进程仍然运行；如果查询失败，也按不可用处理。
    DWORD exitCode = 0;
    return GetExitCodeProcess(static_cast<HANDLE>(processHandle_), &exitCode) && exitCode == STILL_ACTIVE;
}

std::uint32_t WinProcessMemory::processId() const
{
    return processId_;
}

void WinProcessMemory::detach()
{
    if (processHandle_ != nullptr) {
        // processHandle_ 实际保存的是 Windows HANDLE。
        CloseHandle(static_cast<HANDLE>(processHandle_));
        processHandle_ = nullptr;
    }

    processId_ = 0;
    baseAddress_ = 0;
    processName_.clear();
}

bool WinProcessMemory::readInt32(std::uintptr_t offset, std::int32_t &value) const
{
    if (processHandle_ == nullptr || baseAddress_ == 0) {
        return false;
    }

    SIZE_T bytesRead = 0;
    // trainer_core 暴露的 offset 是相对模块基址的偏移，这里转换为目标进程绝对地址。
    const auto address = reinterpret_cast<LPCVOID>(baseAddress_ + offset);
    // 必须确认读取字节数完整，否则视为失败，避免使用半截数据。
    return ReadProcessMemory(static_cast<HANDLE>(processHandle_), address, &value, sizeof(value), &bytesRead)
           && bytesRead == sizeof(value);
}

bool WinProcessMemory::writeInt32(std::uintptr_t offset, std::int32_t value) const
{
    if (processHandle_ == nullptr || baseAddress_ == 0) {
        return false;
    }

    SIZE_T bytesWritten = 0;
    // 写入同样使用“模块基址 + 偏移”的绝对地址。
    const auto address = reinterpret_cast<LPVOID>(baseAddress_ + offset);
    // 必须完整写入 4 字节 int32，部分写入视为失败。
    return WriteProcessMemory(static_cast<HANDLE>(processHandle_), address, &value, sizeof(value), &bytesWritten)
           && bytesWritten == sizeof(value);
}

} // namespace demonstar
