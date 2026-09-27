#include "Memory.h"

MemoryAccessError::MemoryAccessError(uint32_t address, uint32_t width)
    : std::runtime_error("out-of-bounds memory access"), address(address), width(width) {}

Memory::Memory(size_t sizeBytes) : data_(sizeBytes, 0) {}

void Memory::checkBounds(uint32_t addr, uint32_t width) const {
    // addr + width could overflow a 32-bit address on the very top of the
    // space; do the comparison in 64 bits so that case is still caught
    // instead of wrapping around to something that looks in-bounds.
    uint64_t end = static_cast<uint64_t>(addr) + width;
    if (end > data_.size()) {
        throw MemoryAccessError(addr, width);
    }
}

uint8_t Memory::loadByte(uint32_t addr) const {
    checkBounds(addr, 1);
    return data_[addr];
}

uint16_t Memory::loadHalf(uint32_t addr) const {
    checkBounds(addr, 2);
    // Little-endian: low address holds the low byte.
    return static_cast<uint16_t>(data_[addr]) | (static_cast<uint16_t>(data_[addr + 1]) << 8);
}

uint32_t Memory::loadWord(uint32_t addr) const {
    checkBounds(addr, 4);
    return static_cast<uint32_t>(data_[addr]) |
           (static_cast<uint32_t>(data_[addr + 1]) << 8) |
           (static_cast<uint32_t>(data_[addr + 2]) << 16) |
           (static_cast<uint32_t>(data_[addr + 3]) << 24);
}

void Memory::storeByte(uint32_t addr, uint8_t value) {
    checkBounds(addr, 1);
    data_[addr] = value;
}

void Memory::storeHalf(uint32_t addr, uint16_t value) {
    checkBounds(addr, 2);
    data_[addr] = static_cast<uint8_t>(value & 0xFF);
    data_[addr + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void Memory::storeWord(uint32_t addr, uint32_t value) {
    checkBounds(addr, 4);
    data_[addr] = static_cast<uint8_t>(value & 0xFF);
    data_[addr + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data_[addr + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data_[addr + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}
