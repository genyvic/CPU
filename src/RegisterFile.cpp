#include "RegisterFile.h"

RegisterFile::RegisterFile() {
    regs_.fill(0);
}

uint32_t RegisterFile::read(uint32_t index) const {
    return regs_[index & 0x1F];
}

void RegisterFile::write(uint32_t index, uint32_t value) {
    index &= 0x1F;
    if (index == 0) {
        return; // x0 stays zero no matter what
    }
    regs_[index] = value;
}
