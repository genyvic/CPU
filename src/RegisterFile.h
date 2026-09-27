// 32 general-purpose 32-bit registers, x0 hardwired to zero.
#pragma once

#include <array>
#include <cstdint>

class RegisterFile {
public:
    RegisterFile();

    uint32_t read(uint32_t index) const;

    // Writes are silently dropped for x0, matching real hardware -- the
    // caller (CPU) is responsible for reporting that a write to x0 was
    // attempted, since that's a coverage/trace concern, not a register-file
    // one.
    void write(uint32_t index, uint32_t value);

    static bool isZeroRegister(uint32_t index) { return (index & 0x1F) == 0; }

    const std::array<uint32_t, 32>& all() const { return regs_; }

private:
    std::array<uint32_t, 32> regs_;
};
