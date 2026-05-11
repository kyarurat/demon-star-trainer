#ifndef DEMONSTAR_IPROCESS_MEMORY_H
#define DEMONSTAR_IPROCESS_MEMORY_H

#include <cstdint>
#include <string>

namespace demonstar {

class IProcessMemory
{
public:
    virtual ~IProcessMemory() = default;

    virtual bool attach(const std::wstring &processName) = 0;
    virtual bool isAttached() const = 0;
    virtual std::uint32_t processId() const = 0;
    virtual void detach() = 0;
    virtual bool readInt32(std::uintptr_t offset, std::int32_t &value) const = 0;
    virtual bool writeInt32(std::uintptr_t offset, std::int32_t value) const = 0;
};

} // namespace demonstar

#endif // DEMONSTAR_IPROCESS_MEMORY_H
