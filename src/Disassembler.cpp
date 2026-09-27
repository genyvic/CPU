#include "Disassembler.h"

#include <sstream>

namespace {

std::string reg(uint32_t idx) {
    return "x" + std::to_string(idx & 0x1F);
}

} // namespace

std::string disassemble(const DecodedInstr& d) {
    if (!d.valid) {
        std::ostringstream out;
        out << "illegal 0x" << std::hex << d.raw;
        return out.str();
    }

    const std::string mnem = instrName(d.name);
    std::ostringstream out;

    switch (d.name) {
        // R-type ALU: mnem rd, rs1, rs2
        case InstrName::ADD: case InstrName::SUB: case InstrName::SLL:
        case InstrName::SLT: case InstrName::SLTU: case InstrName::XOR:
        case InstrName::SRL: case InstrName::SRA: case InstrName::OR:
        case InstrName::AND:
            out << mnem << " " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2);
            break;

        // I-type shifts: mnem rd, rs1, shamt
        case InstrName::SLLI: case InstrName::SRLI: case InstrName::SRAI:
            out << mnem << " " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.shamt;
            break;

        // I-type ALU: mnem rd, rs1, imm
        case InstrName::ADDI: case InstrName::SLTI: case InstrName::SLTIU:
        case InstrName::XORI: case InstrName::ORI: case InstrName::ANDI:
            out << mnem << " " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm;
            break;

        // Loads: mnem rd, imm(rs1)
        case InstrName::LB: case InstrName::LH: case InstrName::LW:
        case InstrName::LBU: case InstrName::LHU:
            out << mnem << " " << reg(d.rd) << ", " << d.imm << "(" << reg(d.rs1) << ")";
            break;

        // Stores: mnem rs2, imm(rs1)
        case InstrName::SB: case InstrName::SH: case InstrName::SW:
            out << mnem << " " << reg(d.rs2) << ", " << d.imm << "(" << reg(d.rs1) << ")";
            break;

        // Branches: mnem rs1, rs2, imm  (imm is the PC-relative byte offset)
        case InstrName::BEQ: case InstrName::BNE: case InstrName::BLT:
        case InstrName::BGE: case InstrName::BLTU: case InstrName::BGEU:
            out << mnem << " " << reg(d.rs1) << ", " << reg(d.rs2) << ", " << d.imm;
            break;

        case InstrName::JAL:
            out << mnem << " " << reg(d.rd) << ", " << d.imm;
            break;

        case InstrName::JALR:
            out << mnem << " " << reg(d.rd) << ", " << d.imm << "(" << reg(d.rs1) << ")";
            break;

        // U-type: shown as the 20-bit upper immediate, the way assembly
        // listings conventionally write it (imm here is already the full
        // shifted-into-place value, so >> 12 recovers that 20-bit number).
        case InstrName::LUI: case InstrName::AUIPC:
            out << mnem << " " << reg(d.rd) << ", " << (d.imm >> 12);
            break;

        case InstrName::FENCE:
        case InstrName::ECALL:
        case InstrName::EBREAK:
            out << mnem;
            break;

        default:
            out << mnem;
            break;
    }

    return out.str();
}
