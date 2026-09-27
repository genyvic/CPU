// Loads a .hex program file (one 32-bit instruction word per line, written
// in hex) into memory at a base address, for main.cpp and the test/program
// fixtures under programs/.
#pragma once

#include <string>

#include "Memory.h"

class ProgramLoader {
public:
    // Throws std::runtime_error on a malformed line or if the file doesn't
    // fit in `mem` starting at `baseAddress`.
    static void loadHexFile(const std::string& path, Memory& mem, uint32_t baseAddress);
};
