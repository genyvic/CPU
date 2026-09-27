#include "Tracer.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Disassembler.h"

namespace {

std::string hex32(uint32_t v) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return out.str();
}

} // namespace

Tracer::Tracer(const std::string& path) : out_(path) {
    if (!out_) {
        throw std::runtime_error("could not open trace file for writing: " + path);
    }
    out_ << "cycle   pc          instr       disassembly               effects\n";
}

void Tracer::traceInstruction(const RetireRecord& rec) {
    std::vector<std::string> effects;

    if (rec.regWriteAttempted) {
        if (rec.regWriteWasIgnored) {
            effects.push_back("x0 write ignored");
        } else {
            effects.push_back("x" + std::to_string(rec.regWriteIndex) + " <- " + hex32(rec.regWriteValue));
        }
    }
    if (rec.memRead) {
        effects.push_back("mem[" + hex32(rec.memAddr) + "] -> " + hex32(rec.memValue) +
                           " (" + std::to_string(rec.memWidth) + "B)");
    }
    if (rec.memWrite) {
        effects.push_back("mem[" + hex32(rec.memAddr) + "] <- " + hex32(rec.memValue) +
                           " (" + std::to_string(rec.memWidth) + "B)");
    }
    if (rec.isBranch) {
        effects.push_back(rec.branchTaken ? ("branch taken -> " + hex32(rec.branchTarget))
                                           : "branch NOT taken");
    }
    if (rec.halted) {
        effects.push_back("HALT (" + rec.haltReason + ")");
    }
    if (effects.empty()) {
        effects.push_back("-");
    }

    std::string joined;
    for (size_t i = 0; i < effects.size(); ++i) {
        if (i > 0) joined += ", ";
        joined += effects[i];
    }

    std::string disasm = rec.decoded.valid ? disassemble(rec.decoded) : ("illegal " + hex32(rec.decoded.raw));

    out_ << std::setw(7) << std::setfill('0') << rec.cycle << " "
         << hex32(rec.pc) << "  "
         << hex32(rec.decoded.raw) << "  "
         << std::setw(24) << std::setfill(' ') << std::left << disasm << std::right
         << "  " << joined << "\n";
}

void Tracer::traceFooter(const CPU& cpu) {
    out_ << "\n";
    out_ << "HALT: " << cpu.haltReason() << "\n";
    out_ << "total cycles: " << cpu.cycles() << "\n";
    out_ << "final non-zero registers:\n";

    const auto& regs = cpu.regs().all();
    bool any = false;
    for (uint32_t i = 1; i < 32; ++i) {
        if (regs[i] != 0) {
            out_ << "  x" << i << " = " << hex32(regs[i]) << "\n";
            any = true;
        }
    }
    if (!any) {
        out_ << "  (all zero)\n";
    }
}
