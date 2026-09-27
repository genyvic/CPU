// Byte-addressable, little-endian main memory with bounds-checked 8/16/32-bit
// access. This is the only place address-out-of-range errors get detected --
// the CPU just catches MemoryAccessError and turns it into a halt.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

struct MemoryAccessError : std::runtime_error {
    MemoryAccessError(uint32_t address, uint32_t width);
    uint32_t address;
    uint32_t width;
};

class Memory {
public:
    explicit Memory(size_t sizeBytes);

    size_t size() const { return data_.size(); }

    uint8_t loadByte(uint32_t addr) const;
    uint16_t loadHalf(uint32_t addr) const;
    uint32_t loadWord(uint32_t addr) const;

    void storeByte(uint32_t addr, uint8_t value);
    void storeHalf(uint32_t addr, uint16_t value);
    void storeWord(uint32_t addr, uint32_t value);

private:
    void checkBounds(uint32_t addr, uint32_t width) const;

    std::vector<uint8_t> data_;
};
