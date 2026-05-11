#ifndef DEMONSTAR_WIN_PROCESS_MEMORY_H
#define DEMONSTAR_WIN_PROCESS_MEMORY_H

#include "IProcessMemory.h"

namespace demonstar {

class WinProcessMemory final : public IProcessMemory
{
public:
    ~WinProcessMemory() override;

    bool attach(const std::wstring &processName) override;
    bool isAttached() const override;
    std::uint32_t processId() const override;
    void detach() override;
    bool readInt32(std::uintptr_t offset, std::int32_t &value) const override;
    bool writeInt32(std::uintptr_t offset, std::int32_t value) const override;

private:
    bool ensureStillActive();

    void *processHandle_ = nullptr;
    std::uint32_t processId_ = 0;
    std::uintptr_t baseAddress_ = 0;
    std::wstring processName_;
};

} // namespace demonstar

#endif // DEMONSTAR_WIN_PROCESS_MEMORY_H
