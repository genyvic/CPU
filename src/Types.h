// Shared types for the RV32I model: architectural word types, instruction
// formats, and the instruction-name enum used everywhere from decode to
// coverage.
#pragma once

#include <cstdint>
#include <string>

using Word = uint32_t;
using SWord = int32_t;

// The base RV32I opcode field (instr[6:0]). Named after the RISC-V manual's
// informal group names, not the mnemonics they decode to -- e.g. OP_IMM
// covers ADDI/SLTI/.../ANDI, all distinguished later by funct3/funct7.
enum class Opcode : uint32_t {
    LOAD     = 0b0000011,
    STORE    = 0b0100011,
    BRANCH   = 0b1100011,
    JALR_OP  = 0b1100111,
    JAL_OP   = 0b1101111,
    OP_IMM   = 0b0010011,
    OP       = 0b0110011,
    AUIPC_OP = 0b0010111,
    LUI_OP   = 0b0110111,
    MISC_MEM = 0b0001111,
    SYSTEM   = 0b1110011,
};

enum class Format {
    R,
    I,
    S,
    B,
    U,
    J,
    INVALID,
};

// Every RV32I instruction this simulator implements. ILLEGAL is not a real
// instruction -- it's what the decoder returns when nothing above matches --
// so it's kept last and excluded from instruction-count/coverage totals.
enum class InstrName {
    LUI, AUIPC, JAL, JALR,
    BEQ, BNE, BLT, BGE, BLTU, BGEU,
    LB, LH, LW, LBU, LHU,
    SB, SH, SW,
    ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
    FENCE, ECALL, EBREAK,
    ILLEGAL,
};

// Number of real, decodable instructions (everything before ILLEGAL).
constexpr size_t kNumInstructions = static_cast<size_t>(InstrName::ILLEGAL);

// Small inline lookup tables -- kept header-only since they're one-liners
// used from decode, disassembly, tracing, and coverage reporting alike.
inline const char* instrName(InstrName name) {
    switch (name) {
        case InstrName::LUI: return "lui";     case InstrName::AUIPC: return "auipc";
        case InstrName::JAL: return "jal";     case InstrName::JALR: return "jalr";
        case InstrName::BEQ: return "beq";     case InstrName::BNE: return "bne";
        case InstrName::BLT: return "blt";     case InstrName::BGE: return "bge";
        case InstrName::BLTU: return "bltu";   case InstrName::BGEU: return "bgeu";
        case InstrName::LB: return "lb";       case InstrName::LH: return "lh";
        case InstrName::LW: return "lw";       case InstrName::LBU: return "lbu";
        case InstrName::LHU: return "lhu";     case InstrName::SB: return "sb";
        case InstrName::SH: return "sh";       case InstrName::SW: return "sw";
        case InstrName::ADDI: return "addi";   case InstrName::SLTI: return "slti";
        case InstrName::SLTIU: return "sltiu"; case InstrName::XORI: return "xori";
        case InstrName::ORI: return "ori";     case InstrName::ANDI: return "andi";
        case InstrName::SLLI: return "slli";   case InstrName::SRLI: return "srli";
        case InstrName::SRAI: return "srai";   case InstrName::ADD: return "add";
        case InstrName::SUB: return "sub";     case InstrName::SLL: return "sll";
        case InstrName::SLT: return "slt";     case InstrName::SLTU: return "sltu";
        case InstrName::XOR: return "xor";     case InstrName::SRL: return "srl";
        case InstrName::SRA: return "sra";     case InstrName::OR: return "or";
        case InstrName::AND: return "and";     case InstrName::FENCE: return "fence";
        case InstrName::ECALL: return "ecall"; case InstrName::EBREAK: return "ebreak";
        case InstrName::ILLEGAL: return "illegal";
    }
    return "?";
}

inline const char* formatName(Format fmt) {
    switch (fmt) {
        case Format::R: return "R";
        case Format::I: return "I";
        case Format::S: return "S";
        case Format::B: return "B";
        case Format::U: return "U";
        case Format::J: return "J";
        case Format::INVALID: return "INVALID";
    }
    return "?";
}

// A fully-decoded instruction. `imm` holds the format's immediate,
// sign-extended and already shifted into place where the ISA does that
// (U-type: imm is the final value with bits[11:0] = 0, ready to add to PC or
// write to rd directly). `shamt` is only meaningful for the shift-immediate
// ops (SLLI/SRLI/SRAI), where it's imm[4:0] with the funct7-ish bits above it
// already stripped out by the decoder.
struct DecodedInstr {
    Word raw = 0;
    InstrName name = InstrName::ILLEGAL;
    Format fmt = Format::INVALID;
    uint32_t rd = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    int32_t imm = 0;
    uint32_t shamt = 0;
    bool valid = false;
};
