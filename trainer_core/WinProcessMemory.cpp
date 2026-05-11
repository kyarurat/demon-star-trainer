#include "WinProcessMemory.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace demonstar {
namespace {

bool executableNameEquals(const wchar_t *name, const std::wstring &expected)
{
    return _wcsicmp(name, expected.c_str()) == 0;
}

std::uint32_t findProcessId(const std::wstring &processName)
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    std::uint32_t processId = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (executableNameEquals(entry.szExeFile, processName)) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processId;
}

std::uintptr_t findModuleBaseAddress(std::uint32_t processId, const std::wstring &moduleName)
{
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
    detach();
}

bool WinProcessMemory::attach(const std::wstring &processName)
{
    if (isAttached()) {
        return true;
    }

    detach();

    const std::uint32_t foundProcessId = findProcessId(processName);
    if (foundProcessId == 0) {
        return false;
    }

    const HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                                             FALSE,
                                             foundProcessId);
    if (processHandle == nullptr) {
        return false;
    }

    const std::uintptr_t foundBaseAddress = findModuleBaseAddress(foundProcessId, processName);
    if (foundBaseAddress == 0) {
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
    const auto address = reinterpret_cast<LPCVOID>(baseAddress_ + offset);
    return ReadProcessMemory(static_cast<HANDLE>(processHandle_), address, &value, sizeof(value), &bytesRead)
           && bytesRead == sizeof(value);
}

bool WinProcessMemory::writeInt32(std::uintptr_t offset, std::int32_t value) const
{
    if (processHandle_ == nullptr || baseAddress_ == 0) {
        return false;
    }

    SIZE_T bytesWritten = 0;
    const auto address = reinterpret_cast<LPVOID>(baseAddress_ + offset);
    return WriteProcessMemory(static_cast<HANDLE>(processHandle_), address, &value, sizeof(value), &bytesWritten)
           && bytesWritten == sizeof(value);
}

} // namespace demonstar
