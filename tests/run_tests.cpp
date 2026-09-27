// Test runner: pulls in every tests_*.cpp file's directed tests plus the
// decoder unit tests and the constrained-random tests, runs them all against
// one shared Coverage model, and writes out test_results.txt,
// coverage_report.txt and coverage.csv, plus a one-line console summary.
// Exits nonzero if anything failed, so it's CI-friendly.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Coverage.h"
#include "TestHarness.h"
#include "TestRegistry.h"

namespace {

void printUsage() {
    std::cerr <<
        "usage: run_tests [options]\n"
        "  --filter NAME       only run tests whose name contains NAME\n"
        "  --seed N            RNG seed for the constrained-random tests (default 12345)\n"
        "  --random-count N    number of random test cases to generate (default 20)\n"
        "  --trace-all         write a .trace file for every directed test\n"
        "  --out-dir DIR       where reports/traces go (default out)\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string filter;
    uint32_t seed = 12345;
    uint32_t randomCount = 20;
    bool traceAll = false;
    std::string outDir = "out";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << flag << " needs an argument\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--filter") filter = next("--filter");
        else if (arg == "--seed") seed = static_cast<uint32_t>(std::stoul(next("--seed")));
        else if (arg == "--random-count") randomCount = static_cast<uint32_t>(std::stoul(next("--random-count")));
        else if (arg == "--trace-all") traceAll = true;
        else if (arg == "--out-dir") outDir = next("--out-dir");
        else {
            std::cerr << "error: unknown option " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    std::filesystem::create_directories(outDir);

    TestHarness harness;
    registerAluTests(harness);
    registerLoadStoreTests(harness);
    registerBranchJumpTests(harness);
    registerProgramTests(harness);
    registerEdgeCaseTests(harness);
    registerCoverageClosureTests(harness);

    Coverage coverage;
    std::vector<TestResult> results = harness.runAll(coverage, outDir, traceAll);

    for (auto& r : runDecoderUnitTests()) results.push_back(std::move(r));
    for (auto& r : runRandomTests(coverage, seed, randomCount)) results.push_back(std::move(r));

    if (!filter.empty()) {
        std::vector<TestResult> filtered;
        for (auto& r : results) {
            if (r.name.find(filter) != std::string::npos) filtered.push_back(std::move(r));
        }
        results = std::move(filtered);
    }

    // --- test_results.txt -------------------------------------------------
    {
        std::ofstream out(outDir + "/test_results.txt");
        size_t passed = 0;
        for (const auto& r : results) {
            out << (r.passed ? "PASS" : "FAIL") << "  " << r.name << "  (cycles=" << r.cycles << ")\n";
            if (r.passed) {
                ++passed;
            } else {
                for (const auto& f : r.failures) out << "       " << f << "\n";
            }
        }
        out << "\n" << passed << "/" << results.size() << " tests passed\n";
    }

    // --- coverage reports ---------------------------------------------------
    coverage.writeTextReport(outDir + "/coverage_report.txt");
    coverage.writeCsvReport(outDir + "/coverage.csv");

    // --- console summary -----------------------------------------------------
    size_t passed = 0;
    for (const auto& r : results) if (r.passed) ++passed;

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Tests: " << passed << "/" << results.size() << " passed | "
               << "Instruction coverage: " << coverage.instructionCoveragePercent() << "% ("
               << coverage.instructionsHit() << "/" << coverage.instructionsTotal() << ") | "
               << "Functional coverage: " << coverage.functionalCoveragePercent() << "%\n";

    auto holes = coverage.coverageHoles();
    if (!holes.empty()) {
        std::cout << holes.size() << " coverage hole(s) -- see " << outDir << "/coverage_report.txt\n";
    }

    if (passed != results.size()) {
        std::cout << "\nFAILURES:\n";
        for (const auto& r : results) {
            if (!r.passed) {
                std::cout << "  " << r.name << ":\n";
                for (const auto& f : r.failures) std::cout << "    " << f << "\n";
            }
        }
    }

    return (passed == results.size()) ? 0 : 1;
}
