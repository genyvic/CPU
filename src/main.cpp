// CLI front-end: loads a .hex program, runs it to completion on a fresh CPU
// model, and optionally writes a trace file and/or a coverage report for
// that single run. See README.md for full usage.
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "CPU.h"
#include "Coverage.h"
#include "Memory.h"
#include "ProgramLoader.h"
#include "Tracer.h"

namespace {

void printUsage() {
    std::cerr <<
        "usage: rv32i-sim <program.hex> [options]\n"
        "  --trace PATH          write a per-instruction trace file\n"
        "  --coverage PATH       write a coverage report for this run\n"
        "  --max-cycles N        cycle limit before forcing a timeout halt (default 100000)\n"
        "  --mem-size BYTES      memory size in bytes (default 65536)\n"
        "  --reset-pc ADDR       initial PC / program load address (default 0)\n"
        "  --dump-regs           print all 32 registers after halting\n"
        "  --dump-mem ADDR LEN   print LEN bytes of memory starting at ADDR\n";
}

uint32_t parseU32(const std::string& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0)); // base 0: accepts "0x..." or decimal
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string programPath = argv[1];
    std::string tracePath, coveragePath;
    uint32_t maxCycles = 100000;
    uint32_t memSize = 64 * 1024;
    uint32_t resetPC = 0;
    bool dumpRegs = false;
    bool dumpMem = false;
    uint32_t dumpMemAddr = 0, dumpMemLen = 0;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << flag << " needs an argument\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--trace") tracePath = next("--trace");
        else if (arg == "--coverage") coveragePath = next("--coverage");
        else if (arg == "--max-cycles") maxCycles = parseU32(next("--max-cycles"));
        else if (arg == "--mem-size") memSize = parseU32(next("--mem-size"));
        else if (arg == "--reset-pc") resetPC = parseU32(next("--reset-pc"));
        else if (arg == "--dump-regs") dumpRegs = true;
        else if (arg == "--dump-mem") {
            dumpMem = true;
            dumpMemAddr = parseU32(next("--dump-mem"));
            dumpMemLen = parseU32(next("--dump-mem"));
        } else {
            std::cerr << "error: unknown option " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    Memory mem(memSize);
    try {
        ProgramLoader::loadHexFile(programPath, mem, resetPC);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    CPU cpu(mem, resetPC, maxCycles);
    Coverage coverage;

    std::unique_ptr<Tracer> tracer;
    if (!tracePath.empty()) {
        try {
            tracer = std::make_unique<Tracer>(tracePath);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }

    uint32_t instructionsRetired = 0, branches = 0, branchesTaken = 0, loads = 0, stores = 0;

    while (!cpu.halted()) {
        RetireRecord rec = cpu.step();
        coverage.record(rec);
        if (tracer) tracer->traceInstruction(rec);

        if (rec.decoded.valid) {
            ++instructionsRetired;
            if (rec.isBranch) {
                ++branches;
                if (rec.branchTaken) ++branchesTaken;
            }
            if (rec.memRead) ++loads;
            if (rec.memWrite) ++stores;
        }
    }

    if (tracer) tracer->traceFooter(cpu);

    std::cout << "HALT: " << cpu.haltReason() << "\n";
    std::cout << "cycles: " << cpu.cycles() << "  instructions retired: " << instructionsRetired << "\n";
    std::cout << "branches: " << branches << " (" << branchesTaken << " taken, "
              << (branches - branchesTaken) << " not taken)\n";
    std::cout << "loads: " << loads << "  stores: " << stores << "\n";

    if (dumpRegs) {
        std::cout << "\nregisters:\n";
        const auto& regs = cpu.regs().all();
        for (int i = 0; i < 32; ++i) {
            std::cout << "  x" << std::setw(2) << std::setfill(' ') << std::left << i << std::right
                       << " = 0x" << std::hex << std::setw(8) << std::setfill('0') << regs[i]
                       << std::dec << "\n";
        }
    }

    if (dumpMem) {
        std::cout << "\nmemory [0x" << std::hex << dumpMemAddr << ", len=" << std::dec << dumpMemLen << "]:\n  ";
        for (uint32_t i = 0; i < dumpMemLen; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<int>(mem.loadByte(dumpMemAddr + i)) << " ";
            if ((i + 1) % 16 == 0) std::cout << "\n  ";
        }
        std::cout << std::dec << "\n";
    }

    if (!coveragePath.empty()) {
        try {
            coverage.writeTextReport(coveragePath);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }

    return 0;
}
