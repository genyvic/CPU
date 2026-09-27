#include "Coverage.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

bool isAluR(InstrName n) {
    switch (n) {
        case InstrName::ADD: case InstrName::SUB: case InstrName::SLL:
        case InstrName::SLT: case InstrName::SLTU: case InstrName::XOR:
        case InstrName::SRL: case InstrName::SRA: case InstrName::OR:
        case InstrName::AND:
            return true;
        default:
            return false;
    }
}

bool isAluI(InstrName n) {
    switch (n) {
        case InstrName::ADDI: case InstrName::SLTI: case InstrName::SLTIU:
        case InstrName::XORI: case InstrName::ORI: case InstrName::ANDI:
        case InstrName::SLLI: case InstrName::SRLI: case InstrName::SRAI:
            return true;
        default:
            return false;
    }
}

bool isAlu(InstrName n) { return isAluR(n) || isAluI(n); }

bool isShift(InstrName n) {
    switch (n) {
        case InstrName::SLL: case InstrName::SRL: case InstrName::SRA:
        case InstrName::SLLI: case InstrName::SRLI: case InstrName::SRAI:
            return true;
        default:
            return false;
    }
}

bool isLoad(InstrName n) {
    switch (n) {
        case InstrName::LB: case InstrName::LH: case InstrName::LW:
        case InstrName::LBU: case InstrName::LHU:
            return true;
        default:
            return false;
    }
}

bool isStore(InstrName n) {
    switch (n) {
        case InstrName::SB: case InstrName::SH: case InstrName::SW:
            return true;
        default:
            return false;
    }
}

// SLT/SLTU/SLTI/SLTIU only ever produce 0 or 1 -- their result is never
// negative, so a "_result_negative" bin for them would be unfillable.
bool isCompare(InstrName n) {
    switch (n) {
        case InstrName::SLT: case InstrName::SLTU:
        case InstrName::SLTI: case InstrName::SLTIU:
            return true;
        default:
            return false;
    }
}

bool isBranchInstr(InstrName n) {
    switch (n) {
        case InstrName::BEQ: case InstrName::BNE: case InstrName::BLT:
        case InstrName::BGE: case InstrName::BLTU: case InstrName::BGEU:
            return true;
        default:
            return false;
    }
}

// Which formats actually read rs1/rs2 -- gates register_read coverage.
// (FENCE/ECALL/EBREAK are Format::I but the decoder leaves rs1/rs2 at their
// default of 0, i.e. there's no real register read to record for them.)
bool usesRs1(InstrName n) {
    return isAlu(n) || isLoad(n) || isStore(n) || isBranchInstr(n) || n == InstrName::JALR;
}
bool usesRs2(InstrName n) {
    return isAluR(n) || isStore(n) || isBranchInstr(n);
}

std::vector<std::string> classifyValue(uint32_t v) {
    std::vector<std::string> tags;
    if (v == 0) tags.push_back("zero");
    int32_t sv = static_cast<int32_t>(v);
    if (sv > 0) tags.push_back("positive");
    if (sv < 0) tags.push_back("negative");
    if (v == 0x7FFFFFFFu) tags.push_back("int32max");
    if (v == 0x80000000u) tags.push_back("int32min");
    if (v == 0xFFFFFFFFu) tags.push_back("allones");
    return tags;
}

std::vector<std::string> classifyImm(int32_t imm) {
    std::vector<std::string> tags;
    if (imm == 0) tags.push_back("imm_zero");
    if (imm > 0) tags.push_back("imm_positive");
    if (imm < 0) tags.push_back("imm_negative");
    if (imm == 2047) tags.push_back("imm_max");
    if (imm == -2048) tags.push_back("imm_min");
    return tags;
}

std::string classifyShamt(uint32_t s) {
    if (s == 0) return "shamt_0";
    if (s == 31) return "shamt_31";
    return "shamt_mid";
}

bool addOverflows(int32_t a, int32_t b) {
    int64_t sum = static_cast<int64_t>(a) + static_cast<int64_t>(b);
    return sum > INT32_MAX || sum < INT32_MIN;
}

bool subOverflows(int32_t a, int32_t b) {
    int64_t diff = static_cast<int64_t>(a) - static_cast<int64_t>(b);
    return diff > INT32_MAX || diff < INT32_MIN;
}

std::vector<InstrName> allAluInstrs() {
    std::vector<InstrName> v;
    for (size_t i = 0; i < kNumInstructions; ++i) {
        auto n = static_cast<InstrName>(i);
        if (isAlu(n)) v.push_back(n);
    }
    return v;
}

} // namespace

Coverage::Coverage() {
    declareAllBins();
}

void Coverage::declare(const std::string& coverpoint, const std::string& bin) {
    bins_[coverpoint][bin] += 0; // creates the entry at 0 if it doesn't exist yet
}

void Coverage::hit(const std::string& coverpoint, const std::string& bin) {
    bins_[coverpoint][bin] += 1;
}

void Coverage::declareAllBins() {
    // 1. Instruction coverage.
    for (size_t i = 0; i < kNumInstructions; ++i) {
        declare("instruction", instrName(static_cast<InstrName>(i)));
    }

    // 2. Format coverage.
    for (Format f : {Format::R, Format::I, Format::S, Format::B, Format::U, Format::J}) {
        declare("format", formatName(f));
    }

    // 3. Branch coverage: taken/not-taken and forward/backward per branch.
    for (size_t i = 0; i < kNumInstructions; ++i) {
        auto n = static_cast<InstrName>(i);
        if (!isBranchInstr(n)) continue;
        std::string m = instrName(n);
        declare("branch", m + "_taken");
        declare("branch", m + "_not_taken");
        declare("branch", m + "_forward");
        declare("branch", m + "_backward");
    }

    // 4. Operand/value coverage, per ALU instruction.
    for (InstrName n : allAluInstrs()) {
        std::string m = instrName(n);
        bool r = isAluR(n);

        declare("operand", m + "_rd_x0");
        declare("operand", m + "_rs1_x0");
        declare("operand", m + "_rd_eq_rs1");
        if (r) {
            declare("operand", m + "_rs2_x0");
            declare("operand", m + "_rs1_eq_rs2");
        }

        for (const char* tag : {"zero", "positive", "negative", "int32max", "int32min", "allones"}) {
            declare("operand", m + "_rs1_" + tag);
            if (r) declare("operand", m + "_rs2_" + tag);
        }
        // Shift-immediates (SLLI/SRLI/SRAI) don't have a true "immediate"
        // operand -- decodeImmI() applied to their shamt+funct7 field isn't
        // meaningful as a signed value, so imm_* bins for them would be
        // permanently unfillable. Their shift amount is covered separately
        // below, via shamt_*.
        if (!r && !isShift(n)) {
            for (const char* tag : {"imm_zero", "imm_positive", "imm_negative", "imm_max", "imm_min"}) {
                declare("operand", m + "_" + tag);
            }
        }
        if (isShift(n)) {
            declare("operand", m + "_shamt_0");
            declare("operand", m + "_shamt_mid");
            declare("operand", m + "_shamt_31");
        }

        declare("operand", m + "_result_zero");
        if (!isCompare(n)) declare("operand", m + "_result_negative");
        if (n == InstrName::ADD || n == InstrName::ADDI || n == InstrName::SUB) {
            declare("operand", m + "_result_overflow_wrap");
        }
    }

    // 5. Memory coverage.
    for (size_t i = 0; i < kNumInstructions; ++i) {
        auto n = static_cast<InstrName>(i);
        if (isLoad(n) || isStore(n)) {
            declare("memory", std::string(instrName(n)) + "_used");
        }
    }
    for (int off = 0; off < 4; ++off) {
        declare("memory", "offset_" + std::to_string(off));
    }
    declare("memory", "LB_sign_extended_negative");
    declare("memory", "LB_sign_extended_nonneg");
    declare("memory", "LH_sign_extended_negative");
    declare("memory", "LH_sign_extended_nonneg");
    declare("memory", "LBU_zero_extended_highbit");
    declare("memory", "LHU_zero_extended_highbit");

    // 6. Register coverage. x0's "write" bin tracks that a write targeting
    // x0 was attempted (exercising the hardwired-zero path), not that x0's
    // value ever changed -- it structurally can't.
    for (int r = 0; r < 32; ++r) {
        declare("register_write", "x" + std::to_string(r));
        declare("register_read", "x" + std::to_string(r));
    }

    // 7. Cross coverage: R-type ALU instruction x operand-sign combination.
    for (InstrName n : allAluInstrs()) {
        if (!isAluR(n)) continue;
        std::string m = instrName(n);
        for (const char* combo : {"nonneg_nonneg", "nonneg_neg", "neg_nonneg", "neg_neg"}) {
            declare("cross_instr_sign", m + "_" + combo);
        }
    }
}

void Coverage::record(const RetireRecord& rec) {
    const DecodedInstr& d = rec.decoded;
    if (!d.valid) return;

    const std::string m = instrName(d.name);

    hit("instruction", m);
    hit("format", formatName(d.fmt));

    if (isBranchInstr(d.name)) {
        hit("branch", m + (rec.branchTaken ? "_taken" : "_not_taken"));
        hit("branch", m + (d.imm >= 0 ? "_forward" : "_backward"));
    }

    if (isAlu(d.name)) {
        bool r = isAluR(d.name);

        if (d.rd == 0) hit("operand", m + "_rd_x0");
        if (d.rs1 == 0) hit("operand", m + "_rs1_x0");
        if (d.rd == d.rs1) hit("operand", m + "_rd_eq_rs1");
        if (r) {
            if (d.rs2 == 0) hit("operand", m + "_rs2_x0");
            if (d.rs1 == d.rs2) hit("operand", m + "_rs1_eq_rs2");
        }

        for (const auto& tag : classifyValue(rec.rs1Value)) hit("operand", m + "_rs1_" + tag);
        if (r) {
            for (const auto& tag : classifyValue(rec.rs2Value)) hit("operand", m + "_rs2_" + tag);
        } else if (!isShift(d.name)) {
            for (const auto& tag : classifyImm(d.imm)) hit("operand", m + "_" + tag);
        }

        if (isShift(d.name)) {
            uint32_t shamt = r ? (rec.rs2Value & 0x1F) : d.shamt;
            hit("operand", m + "_" + classifyShamt(shamt));
        }

        if (rec.regWriteAttempted) {
            if (rec.regWriteValue == 0) hit("operand", m + "_result_zero");
            if (static_cast<int32_t>(rec.regWriteValue) < 0) hit("operand", m + "_result_negative");
        }
        if (d.name == InstrName::ADD) {
            if (addOverflows(static_cast<int32_t>(rec.rs1Value), static_cast<int32_t>(rec.rs2Value)))
                hit("operand", m + "_result_overflow_wrap");
        } else if (d.name == InstrName::ADDI) {
            if (addOverflows(static_cast<int32_t>(rec.rs1Value), d.imm))
                hit("operand", m + "_result_overflow_wrap");
        } else if (d.name == InstrName::SUB) {
            if (subOverflows(static_cast<int32_t>(rec.rs1Value), static_cast<int32_t>(rec.rs2Value)))
                hit("operand", m + "_result_overflow_wrap");
        }

        if (r) {
            bool s1neg = static_cast<int32_t>(rec.rs1Value) < 0;
            bool s2neg = static_cast<int32_t>(rec.rs2Value) < 0;
            std::string combo = std::string(s1neg ? "neg" : "nonneg") + "_" + (s2neg ? "neg" : "nonneg");
            hit("cross_instr_sign", m + "_" + combo);
        }
    }

    if (isLoad(d.name) || isStore(d.name)) {
        hit("memory", m + "_used");
        hit("memory", "offset_" + std::to_string(rec.memAddr & 0x3));

        if (d.name == InstrName::LB) {
            hit("memory", (static_cast<int8_t>(rec.memValue & 0xFF) < 0)
                              ? "LB_sign_extended_negative" : "LB_sign_extended_nonneg");
        } else if (d.name == InstrName::LH) {
            hit("memory", (static_cast<int16_t>(rec.memValue & 0xFFFF) < 0)
                              ? "LH_sign_extended_negative" : "LH_sign_extended_nonneg");
        } else if (d.name == InstrName::LBU) {
            if (rec.memValue & 0x80) hit("memory", "LBU_zero_extended_highbit");
        } else if (d.name == InstrName::LHU) {
            if (rec.memValue & 0x8000) hit("memory", "LHU_zero_extended_highbit");
        }
    }

    if (rec.regWriteAttempted) {
        hit("register_write", "x" + std::to_string(rec.regWriteIndex));
    }
    if (usesRs1(d.name)) hit("register_read", "x" + std::to_string(d.rs1));
    if (usesRs2(d.name)) hit("register_read", "x" + std::to_string(d.rs2));
}

void Coverage::merge(const Coverage& other) {
    for (const auto& [coverpoint, otherBins] : other.bins_) {
        for (const auto& [bin, count] : otherBins) {
            bins_[coverpoint][bin] += count;
        }
    }
}

size_t Coverage::instructionsHit() const {
    auto it = bins_.find("instruction");
    if (it == bins_.end()) return 0;
    size_t n = 0;
    for (const auto& [bin, count] : it->second) {
        if (count > 0) ++n;
    }
    return n;
}

double Coverage::instructionCoveragePercent() const {
    return instructionsTotal() ? 100.0 * static_cast<double>(instructionsHit()) / static_cast<double>(instructionsTotal()) : 0.0;
}

double Coverage::functionalCoveragePercent() const {
    uint64_t hitBins = 0, totalBins = 0;
    for (const auto& [coverpoint, binMap] : bins_) {
        if (coverpoint == "instruction") continue;
        for (const auto& [bin, count] : binMap) {
            ++totalBins;
            if (count > 0) ++hitBins;
        }
    }
    return totalBins ? 100.0 * static_cast<double>(hitBins) / static_cast<double>(totalBins) : 0.0;
}

std::vector<std::string> Coverage::coverageHoles() const {
    std::vector<std::string> holes;
    for (const auto& [coverpoint, binMap] : bins_) {
        for (const auto& [bin, count] : binMap) {
            if (count == 0) {
                holes.push_back(coverpoint + ": " + bin);
            }
        }
    }
    return holes;
}

void Coverage::writeTextReport(const std::string& path) const {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("could not open coverage report for writing: " + path);

    out << "=== Coverage Report ===\n\n";
    out << "Instruction coverage: " << instructionsHit() << "/" << instructionsTotal()
        << " (" << instructionCoveragePercent() << "%)\n";
    out << "Functional coverage (all coverpoints, excl. instruction): "
        << functionalCoveragePercent() << "%\n\n";

    out << "--- Per-coverpoint summary ---\n";
    for (const auto& [coverpoint, binMap] : bins_) {
        uint64_t total = binMap.size(), hitCount = 0;
        for (const auto& [bin, count] : binMap) if (count > 0) ++hitCount;
        double pct = total ? 100.0 * static_cast<double>(hitCount) / static_cast<double>(total) : 0.0;
        out << "  " << coverpoint << ": " << hitCount << "/" << total << " (" << pct << "%)\n";
    }
    out << "\n--- Instructions ---\n";
    auto instrIt = bins_.find("instruction");
    if (instrIt != bins_.end()) {
        for (const auto& [bin, count] : instrIt->second) {
            out << "  " << bin << ": " << (count > 0 ? "HIT   " : "MISSED") << "  hits=" << count << "\n";
        }
    }

    auto holes = coverageHoles();
    out << "\n--- COVERAGE HOLES (" << holes.size() << ") ---\n";
    if (holes.empty()) {
        out << "  none\n";
    } else {
        for (const auto& h : holes) out << "  " << h << "\n";
    }
}

void Coverage::writeCsvReport(const std::string& path) const {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("could not open coverage CSV for writing: " + path);

    out << "coverpoint,bin,hits\n";
    for (const auto& [coverpoint, binMap] : bins_) {
        for (const auto& [bin, count] : binMap) {
            out << coverpoint << "," << bin << "," << count << "\n";
        }
    }
}
