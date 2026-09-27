// Functional coverage model: a generic "coverpoint -> bin -> hit count"
// table, fed one RetireRecord at a time. All expected bins are declared up
// front (in the constructor) so a bin that's never hit shows up as a real
// coverage hole in the report instead of just being silently absent.
//
// Coverpoints implemented: instruction, format, branch (taken/not-taken +
// forward/backward), ALU operand/value coverage, memory access coverage,
// register read/write coverage, and a small instruction x operand-sign cross
// coverpoint.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "CPU.h"

class Coverage {
public:
    Coverage();

    void record(const RetireRecord& rec);
    void merge(const Coverage& other);

    size_t instructionsHit() const;
    size_t instructionsTotal() const { return kNumInstructions; }
    double instructionCoveragePercent() const;

    // Average hit-ratio across every coverpoint except "instruction" --
    // this is the "functional coverage %" printed in the console summary.
    double functionalCoveragePercent() const;

    // "<coverpoint>: <bin>" for every bin still sitting at zero hits.
    std::vector<std::string> coverageHoles() const;

    void writeTextReport(const std::string& path) const;
    void writeCsvReport(const std::string& path) const;

private:
    void declare(const std::string& coverpoint, const std::string& bin);
    void hit(const std::string& coverpoint, const std::string& bin);
    void declareAllBins();

    std::map<std::string, std::map<std::string, uint64_t>> bins_;
};
