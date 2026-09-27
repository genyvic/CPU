#include "TestHarness.h"

#include <iomanip>
#include <memory>
#include <sstream>

#include "CPU.h"
#include "Memory.h"
#include "Tracer.h"

namespace {

std::string hex32(uint32_t v) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return out.str();
}

} // namespace

void TestHarness::addTest(TestCase tc) {
    tests_.push_back(std::move(tc));
}

std::vector<TestResult> TestHarness::runAll(Coverage& coverage, const std::string& traceDir, bool traceAll) {
    std::vector<TestResult> results;
    results.reserve(tests_.size());
    for (const auto& tc : tests_) {
        results.push_back(runOne(tc, coverage, traceDir, traceAll));
    }
    return results;
}

TestResult TestHarness::runOne(const TestCase& tc, Coverage& coverage, const std::string& traceDir, bool traceAll) {
    TestResult result;
    result.name = tc.name;

    Memory mem(tc.memSize);
    try {
        for (size_t i = 0; i < tc.program.size(); ++i) {
            mem.storeWord(tc.baseAddress + static_cast<uint32_t>(i * 4), tc.program[i]);
        }
        for (const auto& word : tc.initialMemWords) {
            mem.storeWord(word.first, word.second);
        }
    } catch (const MemoryAccessError&) {
        result.failures.push_back("test setup failed: program/initial memory doesn't fit in memSize=" +
                                   std::to_string(tc.memSize));
        return result;
    }

    CPU cpu(mem, tc.baseAddress, tc.maxCycles);
    for (const auto& r : tc.initialRegs) {
        cpu.pokeRegister(r.index, r.value);
    }

    std::unique_ptr<Tracer> tracer;
    if (traceAll) {
        try {
            tracer = std::make_unique<Tracer>(traceDir + "/" + tc.name + ".trace");
        } catch (const std::exception&) {
            // Tracing is a convenience, not a correctness check -- a test
            // shouldn't fail just because its trace file couldn't be opened.
        }
    }

    while (!cpu.halted()) {
        RetireRecord rec = cpu.step();
        coverage.record(rec);
        if (rec.decoded.valid) {
            result.instructionsExecuted.insert(rec.decoded.name);
        }
        if (tracer) tracer->traceInstruction(rec);
    }
    if (tracer) tracer->traceFooter(cpu);

    result.cycles = cpu.cycles();
    bool ok = true;

    if (!tc.expectedHaltReason.empty() && cpu.haltReason() != tc.expectedHaltReason) {
        ok = false;
        result.failures.push_back("halt reason: expected '" + tc.expectedHaltReason + "', got '" + cpu.haltReason() + "'");
    }

    for (const auto& e : tc.expectedRegs) {
        uint32_t actual = cpu.regs().read(e.index);
        if (actual != e.value) {
            ok = false;
            result.failures.push_back("x" + std::to_string(e.index) + ": expected " + hex32(e.value) +
                                       ", got " + hex32(actual));
        }
    }

    for (const auto& e : tc.expectedMem) {
        uint32_t actual = 0;
        bool readOk = true;
        try {
            switch (e.width) {
                case 1: actual = mem.loadByte(e.addr); break;
                case 2: actual = mem.loadHalf(e.addr); break;
                case 4: actual = mem.loadWord(e.addr); break;
                default: readOk = false;
            }
        } catch (const MemoryAccessError&) {
            readOk = false;
        }
        if (!readOk) {
            ok = false;
            result.failures.push_back("mem[" + hex32(e.addr) + "]: could not read " + std::to_string(e.width) + " bytes");
        } else if (actual != e.value) {
            ok = false;
            result.failures.push_back("mem[" + hex32(e.addr) + "] (" + std::to_string(e.width) + "B): expected " +
                                       hex32(e.value) + ", got " + hex32(actual));
        }
    }

    result.passed = ok;
    return result;
}
