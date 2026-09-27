// A small directed-test harness: build a program (as encoded words) plus
// optional initial register/memory state, describe the expected final
// register/memory state, and let the harness run it on a fresh CPU and
// compare. Every retired instruction from every test also feeds the shared
// Coverage model passed into runAll(), so coverage accumulates across the
// whole suite as it runs.
#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Coverage.h"
#include "Types.h"

struct RegExpectation {
    uint32_t index;
    uint32_t value;
};

struct MemExpectation {
    uint32_t addr;
    uint32_t value;
    uint32_t width; // 1, 2, or 4 bytes
};

struct TestCase {
    std::string name;
    std::vector<uint32_t> program; // encoded instruction words, loaded at baseAddress
    uint32_t baseAddress = 0;
    uint32_t memSize = 4096;
    uint32_t maxCycles = 10000;

    std::vector<RegExpectation> initialRegs;                // poked in before running
    std::vector<std::pair<uint32_t, uint32_t>> initialMemWords; // (addr, word) poked in before running

    std::vector<RegExpectation> expectedRegs;
    std::vector<MemExpectation> expectedMem;

    // Left empty to skip the check (most tests don't halt via a specific
    // path they care about beyond "it halted"); set to e.g. "ecall",
    // "illegal instruction", "out-of-bounds memory access", or
    // "max-cycle timeout" for the negative tests that do care.
    std::string expectedHaltReason;
};

struct TestResult {
    std::string name;
    bool passed = false;
    uint32_t cycles = 0;
    std::vector<std::string> failures;
    std::set<InstrName> instructionsExecuted;
};

class TestHarness {
public:
    void addTest(TestCase tc);
    const std::vector<TestCase>& tests() const { return tests_; }

    // Runs every registered test in order. Every retired instruction from
    // every test is recorded into `coverage`. If `traceAll` is set, each
    // test's execution is written to "<traceDir>/<test name>.trace".
    std::vector<TestResult> runAll(Coverage& coverage, const std::string& traceDir, bool traceAll);

private:
    TestResult runOne(const TestCase& tc, Coverage& coverage, const std::string& traceDir, bool traceAll);

    std::vector<TestCase> tests_;
};
