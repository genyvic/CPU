// Writes the per-instruction "commit log" trace file: one line per retired
// instruction with its cycle, PC, raw word, disassembly, and effects (what
// actually changed), finishing with a summary footer once the run halts.
#pragma once

#include <fstream>
#include <string>

#include "CPU.h"

class Tracer {
public:
    // Throws std::runtime_error if the file can't be opened for writing.
    explicit Tracer(const std::string& path);

    Tracer(const Tracer&) = delete;
    Tracer& operator=(const Tracer&) = delete;

    void traceInstruction(const RetireRecord& rec);

    // Call once after the run halts: halt reason, total cycles, and a dump
    // of every non-zero register.
    void traceFooter(const CPU& cpu);

private:
    std::ofstream out_;
};
