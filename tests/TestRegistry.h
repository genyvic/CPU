// Declares the registration function each tests_*.cpp exposes, so
// run_tests.cpp can pull every test file into one suite without them
// knowing about each other.
#pragma once

#include <cstdint>
#include <vector>

#include "TestHarness.h"

void registerAluTests(TestHarness& h);
void registerLoadStoreTests(TestHarness& h);
void registerBranchJumpTests(TestHarness& h);
void registerProgramTests(TestHarness& h);
void registerEdgeCaseTests(TestHarness& h);
void registerCoverageClosureTests(TestHarness& h);

// Unlike the directed tests above, random testing doesn't fit the
// run-to-completion-then-check-final-state TestCase model: it compares the
// real CPU against an independent reference model after *every* generated
// instruction, so it drives its own CPU/Memory and returns TestResults
// directly instead of registering TestCases. `count` random cases are
// generated, seeded seed, seed+1, seed+2, ... for reproducibility.
std::vector<TestResult> runRandomTests(Coverage& coverage, uint32_t seed, uint32_t count);

// Also not CPU-execution tests -- straight unit tests of Decoder::decode()
// on hand-crafted instruction words. Returned as TestResults (cycles=0, no
// instructionsExecuted) purely so run_tests can report them alongside
// everything else through the same summary/CSV/console-line code path.
std::vector<TestResult> runDecoderUnitTests();
