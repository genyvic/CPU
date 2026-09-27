// Direct unit tests of Decoder::decode() -- field extraction and immediate
// sign-extension for every format, including negative immediates, checked
// against the raw decoded struct rather than by running a program. These
// don't touch a CPU at all, so they're reported as their own TestResults
// rather than through TestHarness.
#include "TestRegistry.h"

#include <sstream>

#include "Assembler.h"
#include "Decoder.h"

namespace {

std::string hex32(uint32_t v) {
    std::ostringstream out;
    out << "0x" << std::hex << v;
    return out.str();
}

struct Check {
    std::string label;
    bool ok;
};

TestResult makeResult(const std::string& name, const DecodedInstr& d, std::initializer_list<Check> checks) {
    TestResult r;
    r.name = name;
    r.passed = true;
    for (const auto& c : checks) {
        if (!c.ok) {
            r.passed = false;
            r.failures.push_back(c.label);
        }
    }
    if (d.valid) r.instructionsExecuted.insert(d.name);
    return r;
}

} // namespace

std::vector<TestResult> runDecoderUnitTests() {
    using namespace asmb;
    std::vector<TestResult> results;

    {
        DecodedInstr d = Decoder::decode(ADD(5, 6, 7));
        results.push_back(makeResult("decode_r_type_add", d, {
            {"fmt == R", d.fmt == Format::R},
            {"name == ADD", d.name == InstrName::ADD},
            {"rd == 5", d.rd == 5},
            {"rs1 == 6", d.rs1 == 6},
            {"rs2 == 7", d.rs2 == 7},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(SUB(1, 2, 3));
        results.push_back(makeResult("decode_r_type_funct7_distinguishes_sub_from_add", d, {
            {"name == SUB, not ADD", d.name == InstrName::SUB},
        }));
    }
    {
        // Negative I-type immediate: -5 must come back as exactly -5, not
        // some positive 12-bit pattern.
        DecodedInstr d = Decoder::decode(ADDI(3, 1, -5));
        results.push_back(makeResult("decode_i_type_negative_immediate", d, {
            {"fmt == I", d.fmt == Format::I},
            {"name == ADDI", d.name == InstrName::ADDI},
            {"rd == 3", d.rd == 3},
            {"rs1 == 1", d.rs1 == 1},
            {"imm == -5, got " + std::to_string(d.imm), d.imm == -5},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(ADDI(3, 1, -2048));
        results.push_back(makeResult("decode_i_type_immediate_min", d, {
            {"imm == -2048, got " + std::to_string(d.imm), d.imm == -2048},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(ADDI(3, 1, 2047));
        results.push_back(makeResult("decode_i_type_immediate_max", d, {
            {"imm == 2047, got " + std::to_string(d.imm), d.imm == 2047},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(SRAI(4, 1, 17));
        results.push_back(makeResult("decode_i_type_shift_shamt_and_funct7_bit", d, {
            {"name == SRAI, not SRLI", d.name == InstrName::SRAI},
            {"shamt == 17", d.shamt == 17},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(SRLI(4, 1, 17));
        results.push_back(makeResult("decode_i_type_srli_vs_srai", d, {
            {"name == SRLI", d.name == InstrName::SRLI},
        }));
    }
    {
        // S-type: negative store offset, split across imm[11:5]/imm[4:0].
        DecodedInstr d = Decoder::decode(SW(2, -8, 1));
        results.push_back(makeResult("decode_s_type_negative_immediate", d, {
            {"fmt == S", d.fmt == Format::S},
            {"name == SW", d.name == InstrName::SW},
            {"rs1 == 1", d.rs1 == 1},
            {"rs2 == 2", d.rs2 == 2},
            {"imm == -8, got " + std::to_string(d.imm), d.imm == -8},
        }));
    }
    {
        // B-type: negative (backward) branch offset.
        DecodedInstr d = Decoder::decode(BEQ(1, 2, -16));
        results.push_back(makeResult("decode_b_type_negative_offset", d, {
            {"fmt == B", d.fmt == Format::B},
            {"name == BEQ", d.name == InstrName::BEQ},
            {"imm == -16, got " + std::to_string(d.imm), d.imm == -16},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(BEQ(1, 2, 4094)); // near the positive edge of B's range
        results.push_back(makeResult("decode_b_type_large_positive_offset", d, {
            {"imm == 4094, got " + std::to_string(d.imm), d.imm == 4094},
        }));
    }
    {
        // U-type: the 20-bit immediate lands in bits[31:12] with the low
        // 12 bits clear -- imm >> 12 should recover exactly what was
        // passed to the encoder.
        DecodedInstr d = Decoder::decode(LUI(9, 0xABCDE));
        results.push_back(makeResult("decode_u_type_upper_immediate", d, {
            {"fmt == U", d.fmt == Format::U},
            {"name == LUI", d.name == InstrName::LUI},
            {"rd == 9", d.rd == 9},
            {"imm >> 12 == 0xABCDE, got " + hex32(static_cast<uint32_t>(d.imm) >> 12),
             (static_cast<uint32_t>(d.imm) >> 12) == 0xABCDEu},
            {"low 12 bits of imm are clear", (d.imm & 0xFFF) == 0},
        }));
    }
    {
        // J-type: large negative jump offset.
        DecodedInstr d = Decoder::decode(JAL(1, -1024));
        results.push_back(makeResult("decode_j_type_negative_offset", d, {
            {"fmt == J", d.fmt == Format::J},
            {"name == JAL", d.name == InstrName::JAL},
            {"rd == 1", d.rd == 1},
            {"imm == -1024, got " + std::to_string(d.imm), d.imm == -1024},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(JALR(1, 5, -100));
        results.push_back(makeResult("decode_jalr_is_i_type", d, {
            {"fmt == I", d.fmt == Format::I},
            {"name == JALR", d.name == InstrName::JALR},
            {"rs1 == 5", d.rs1 == 5},
            {"imm == -100, got " + std::to_string(d.imm), d.imm == -100},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(LW(3, -20, 2));
        results.push_back(makeResult("decode_load_is_i_type_with_negative_offset", d, {
            {"fmt == I", d.fmt == Format::I},
            {"name == LW", d.name == InstrName::LW},
            {"imm == -20, got " + std::to_string(d.imm), d.imm == -20},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(ECALL());
        results.push_back(makeResult("decode_ecall", d, {
            {"name == ECALL", d.name == InstrName::ECALL},
            {"valid", d.valid},
        }));
    }
    {
        DecodedInstr d = Decoder::decode(EBREAK());
        results.push_back(makeResult("decode_ebreak", d, {
            {"name == EBREAK", d.name == InstrName::EBREAK},
        }));
    }
    {
        // Opcode field 0x7F (all ones) matches no RV32I opcode.
        DecodedInstr d = Decoder::decode(0xFFFFFFFFu);
        results.push_back(makeResult("decode_illegal_opcode", d, {
            {"valid == false", !d.valid},
            {"name == ILLEGAL", d.name == InstrName::ILLEGAL},
        }));
    }
    {
        // A well-formed opcode (OP) with a funct3/funct7 combination that
        // doesn't correspond to any real instruction (funct7 field set to
        // something other than 0000000/0100000).
        uint32_t bogus = R(0b0011001, 2, 1, 0b000, 3, static_cast<uint32_t>(Opcode::OP));
        DecodedInstr d = Decoder::decode(bogus);
        results.push_back(makeResult("decode_illegal_funct7_on_op", d, {
            {"valid == false", !d.valid},
        }));
    }

    return results;
}
