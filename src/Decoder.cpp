#include "Decoder.h"

namespace {

// Sign-extends a `width`-bit field that has already been assembled into the
// low bits of a uint32_t (with any high bits above `width` clear). Works by
// pushing the field's sign bit up to bit 31, then doing an arithmetic right
// shift back down -- the classic shift-left-then-arithmetic-shift-right
// sign-extension trick, used below for the awkward, non-contiguous B/J
// immediate encodings.
int32_t signExtend(uint32_t value, int width) {
    int shift = 32 - width;
    return static_cast<int32_t>(value << shift) >> shift;
}

int32_t decodeImmI(Word instr) {
    // imm[11:0] = instr[31:20]; an arithmetic shift of the whole signed word
    // does the sign extension for free since instr[31] is also imm[11].
    return static_cast<int32_t>(instr) >> 20;
}

int32_t decodeImmS(Word instr) {
    int32_t hi = static_cast<int32_t>(instr) >> 25; // imm[11:5], sign-extended
    uint32_t lo = (instr >> 7) & 0x1F;               // imm[4:0]
    return (hi << 5) | static_cast<int32_t>(lo);
}

int32_t decodeImmB(Word instr) {
    uint32_t bit12 = (instr >> 31) & 0x1;
    uint32_t bit11 = (instr >> 7) & 0x1;
    uint32_t bits10_5 = (instr >> 25) & 0x3F;
    uint32_t bits4_1 = (instr >> 8) & 0xF;
    uint32_t field = (bit12 << 12) | (bit11 << 11) | (bits10_5 << 5) | (bits4_1 << 1);
    return signExtend(field, 13); // bit 0 is always 0 (branch targets are 2-byte aligned at least)
}

int32_t decodeImmU(Word instr) {
    // imm[31:12] = instr[31:12], imm[11:0] = 0. That's already the final
    // value -- no further shifting or sign-extension needed.
    return static_cast<int32_t>(instr & 0xFFFFF000u);
}

int32_t decodeImmJ(Word instr) {
    uint32_t bit20 = (instr >> 31) & 0x1;
    uint32_t bits19_12 = (instr >> 12) & 0xFF;
    uint32_t bit11 = (instr >> 20) & 0x1;
    uint32_t bits10_1 = (instr >> 21) & 0x3FF;
    uint32_t field = (bit20 << 20) | (bits19_12 << 12) | (bit11 << 11) | (bits10_1 << 1);
    return signExtend(field, 21);
}

} // namespace

DecodedInstr Decoder::decode(Word instr) {
    DecodedInstr d;
    d.raw = instr;

    uint32_t opcodeBits = instr & 0x7F;
    uint32_t rd = (instr >> 7) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t rs2 = (instr >> 20) & 0x1F;
    uint32_t funct7 = (instr >> 25) & 0x7F;
    Opcode opcode = static_cast<Opcode>(opcodeBits);

    switch (opcode) {
        case Opcode::LUI_OP:
            d.fmt = Format::U; d.name = InstrName::LUI;
            d.rd = rd; d.imm = decodeImmU(instr);
            break;

        case Opcode::AUIPC_OP:
            d.fmt = Format::U; d.name = InstrName::AUIPC;
            d.rd = rd; d.imm = decodeImmU(instr);
            break;

        case Opcode::JAL_OP:
            d.fmt = Format::J; d.name = InstrName::JAL;
            d.rd = rd; d.imm = decodeImmJ(instr);
            break;

        case Opcode::JALR_OP:
            if (funct3 != 0) goto illegal;
            d.fmt = Format::I; d.name = InstrName::JALR;
            d.rd = rd; d.rs1 = rs1; d.imm = decodeImmI(instr);
            break;

        case Opcode::BRANCH: {
            InstrName name;
            switch (funct3) {
                case 0b000: name = InstrName::BEQ; break;
                case 0b001: name = InstrName::BNE; break;
                case 0b100: name = InstrName::BLT; break;
                case 0b101: name = InstrName::BGE; break;
                case 0b110: name = InstrName::BLTU; break;
                case 0b111: name = InstrName::BGEU; break;
                default: goto illegal;
            }
            d.fmt = Format::B; d.name = name;
            d.rs1 = rs1; d.rs2 = rs2; d.imm = decodeImmB(instr);
            break;
        }

        case Opcode::LOAD: {
            InstrName name;
            switch (funct3) {
                case 0b000: name = InstrName::LB; break;
                case 0b001: name = InstrName::LH; break;
                case 0b010: name = InstrName::LW; break;
                case 0b100: name = InstrName::LBU; break;
                case 0b101: name = InstrName::LHU; break;
                default: goto illegal;
            }
            d.fmt = Format::I; d.name = name;
            d.rd = rd; d.rs1 = rs1; d.imm = decodeImmI(instr);
            break;
        }

        case Opcode::STORE: {
            InstrName name;
            switch (funct3) {
                case 0b000: name = InstrName::SB; break;
                case 0b001: name = InstrName::SH; break;
                case 0b010: name = InstrName::SW; break;
                default: goto illegal;
            }
            d.fmt = Format::S; d.name = name;
            d.rs1 = rs1; d.rs2 = rs2; d.imm = decodeImmS(instr);
            break;
        }

        case Opcode::OP_IMM: {
            d.fmt = Format::I;
            d.rd = rd; d.rs1 = rs1; d.imm = decodeImmI(instr);
            switch (funct3) {
                case 0b000: d.name = InstrName::ADDI; break;
                case 0b010: d.name = InstrName::SLTI; break;
                case 0b011: d.name = InstrName::SLTIU; break;
                case 0b100: d.name = InstrName::XORI; break;
                case 0b110: d.name = InstrName::ORI; break;
                case 0b111: d.name = InstrName::ANDI; break;
                case 0b001:
                    d.name = InstrName::SLLI;
                    d.shamt = (instr >> 20) & 0x1F;
                    break;
                case 0b101:
                    // Bit 30 (part of what would be funct7 on an R-type)
                    // tells SRLI apart from SRAI -- it's the same bit that
                    // distinguishes SRL/SRA and ADD/SUB below.
                    d.name = ((instr >> 30) & 0x1) ? InstrName::SRAI : InstrName::SRLI;
                    d.shamt = (instr >> 20) & 0x1F;
                    break;
                default: goto illegal;
            }
            break;
        }

        case Opcode::OP: {
            if (funct7 != 0b0000000 && funct7 != 0b0100000) goto illegal;
            bool alt = (funct7 == 0b0100000); // the SUB/SRA variant
            InstrName name;
            switch (funct3) {
                case 0b000: name = alt ? InstrName::SUB : InstrName::ADD; break;
                case 0b001: if (alt) goto illegal; name = InstrName::SLL; break;
                case 0b010: if (alt) goto illegal; name = InstrName::SLT; break;
                case 0b011: if (alt) goto illegal; name = InstrName::SLTU; break;
                case 0b100: if (alt) goto illegal; name = InstrName::XOR; break;
                case 0b101: name = alt ? InstrName::SRA : InstrName::SRL; break;
                case 0b110: if (alt) goto illegal; name = InstrName::OR; break;
                case 0b111: if (alt) goto illegal; name = InstrName::AND; break;
                default: goto illegal;
            }
            d.fmt = Format::R; d.name = name;
            d.rd = rd; d.rs1 = rs1; d.rs2 = rs2;
            break;
        }

        case Opcode::MISC_MEM:
            if (funct3 != 0) goto illegal;
            d.fmt = Format::I; d.name = InstrName::FENCE;
            break;

        case Opcode::SYSTEM: {
            if (funct3 != 0) goto illegal;
            uint32_t imm12 = (instr >> 20) & 0xFFF;
            if (imm12 == 0) d.name = InstrName::ECALL;
            else if (imm12 == 1) d.name = InstrName::EBREAK;
            else goto illegal;
            d.fmt = Format::I;
            break;
        }

        default:
            goto illegal;
    }

    d.valid = true;
    return d;

illegal:
    d.name = InstrName::ILLEGAL;
    d.fmt = Format::INVALID;
    d.valid = false;
    return d;
}
