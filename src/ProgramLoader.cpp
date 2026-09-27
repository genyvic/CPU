#include "ProgramLoader.h"

#include <fstream>
#include <stdexcept>

void ProgramLoader::loadHexFile(const std::string& path, Memory& mem, uint32_t baseAddress) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("could not open program file: " + path);
    }

    uint32_t addr = baseAddress;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;

        // Strip a trailing '#' comment (if any), then trim whitespace.
        size_t hashPos = line.find('#');
        if (hashPos != std::string::npos) {
            line = line.substr(0, hashPos);
        }
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue; // blank line
        }
        size_t end = line.find_last_not_of(" \t\r\n");
        std::string token = line.substr(start, end - start + 1);

        if (token.size() >= 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            token = token.substr(2);
        }

        uint32_t word;
        try {
            word = static_cast<uint32_t>(std::stoul(token, nullptr, 16));
        } catch (const std::exception&) {
            throw std::runtime_error(path + ":" + std::to_string(lineNo) + ": not a valid hex word: " + line);
        }

        try {
            mem.storeWord(addr, word);
        } catch (const MemoryAccessError&) {
            throw std::runtime_error(path + ": program is too large for memory (overflowed at address " +
                                      std::to_string(addr) + ")");
        }
        addr += 4;
    }
}
