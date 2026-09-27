// A tiny hand-rolled RV32I encoder. There's no RISC-V toolchain on this
// machine, so tests build programs by calling these functions directly
// instead of assembling text. Reuses the Opcode enum from Types.h so the
// encoders and the decoder can't silently drift apart.
#pragma once

#include <cstdint>

#include "Types.h"

namespace asmb {

inline uint32_t opc(Opcode o) { return static_cast<uint32_t>(o); }

// --- Format encoders -------------------------------------------------

inline uint32_t R(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return (funct7 << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
           ((funct3 & 0x7) << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}

inline uint32_t I(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    uint32_t imm12 = static_cast<uint32_t>(imm) & 0xFFF;
    return (imm12 << 20) | ((rs1 & 0x1F) << 15) | ((funct3 & 0x7) << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}

inline uint32_t S(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    uint32_t immU = static_cast<uint32_t>(imm) & 0xFFF;
    uint32_t hi = (immU >> 5) & 0x7F;
    uint32_t lo = immU & 0x1F;
    return (hi << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) | ((funct3 & 0x7) << 12) | (lo << 7) | (opcode & 0x7F);
}

// `imm` is the signed byte offset (must be even -- bit 0 is always 0).
inline uint32_t B(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    uint32_t u = static_cast<uint32_t>(imm);
    uint32_t bit12 = (u >> 12) & 0x1, bit11 = (u >> 11) & 0x1;
    uint32_t bits10_5 = (u >> 5) & 0x3F, bits4_1 = (u >> 1) & 0xF;
    return (bit12 << 31) | (bits10_5 << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
           ((funct3 & 0x7) << 12) | (bits4_1 << 8) | (bit11 << 7) | (opcode & 0x7F);
}

// `imm20` is the raw 20-bit upper immediate, unshifted (as in "lui x5, 100").
inline uint32_t U(uint32_t imm20, uint32_t rd, uint32_t opcode) {
    return ((imm20 & 0xFFFFF) << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}

// `imm` is the signed byte offset (must be even).
inline uint32_t J(int32_t imm, uint32_t rd, uint32_t opcode) {
    uint32_t u = static_cast<uint32_t>(imm);
    uint32_t bit20 = (u >> 20) & 0x1;
    uint32_t bits10_1 = (u >> 1) & 0x3FF;
    uint32_t bit11 = (u >> 11) & 0x1;
    uint32_t bits19_12 = (u >> 12) & 0xFF;
    return (bit20 << 31) | (bits10_1 << 21) | (bit11 << 20) | (bits19_12 << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}

// --- Per-instruction convenience encoders -----------------------------

inline uint32_t LUI(uint32_t rd, uint32_t imm20)   { return U(imm20, rd, opc(Opcode::LUI_OP)); }
inline uint32_t AUIPC(uint32_t rd, uint32_t imm20) { return U(imm20, rd, opc(Opcode::AUIPC_OP)); }
inline uint32_t JAL(uint32_t rd, int32_t offset)   { return J(offset, rd, opc(Opcode::JAL_OP)); }
inline uint32_t JALR(uint32_t rd, uint32_t rs1, int32_t imm) { return I(imm, rs1, 0b000, rd, opc(Opcode::JALR_OP)); }

inline uint32_t BEQ(uint32_t rs1, uint32_t rs2, int32_t off)  { return B(off, rs2, rs1, 0b000, opc(Opcode::BRANCH)); }
inline uint32_t BNE(uint32_t rs1, uint32_t rs2, int32_t off)  { return B(off, rs2, rs1, 0b001, opc(Opcode::BRANCH)); }
inline uint32_t BLT(uint32_t rs1, uint32_t rs2, int32_t off)  { return B(off, rs2, rs1, 0b100, opc(Opcode::BRANCH)); }
inline uint32_t BGE(uint32_t rs1, uint32_t rs2, int32_t off)  { return B(off, rs2, rs1, 0b101, opc(Opcode::BRANCH)); }
inline uint32_t BLTU(uint32_t rs1, uint32_t rs2, int32_t off) { return B(off, rs2, rs1, 0b110, opc(Opcode::BRANCH)); }
inline uint32_t BGEU(uint32_t rs1, uint32_t rs2, int32_t off) { return B(off, rs2, rs1, 0b111, opc(Opcode::BRANCH)); }

inline uint32_t LB(uint32_t rd, int32_t off, uint32_t rs1)  { return I(off, rs1, 0b000, rd, opc(Opcode::LOAD)); }
inline uint32_t LH(uint32_t rd, int32_t off, uint32_t rs1)  { return I(off, rs1, 0b001, rd, opc(Opcode::LOAD)); }
inline uint32_t LW(uint32_t rd, int32_t off, uint32_t rs1)  { return I(off, rs1, 0b010, rd, opc(Opcode::LOAD)); }
inline uint32_t LBU(uint32_t rd, int32_t off, uint32_t rs1) { return I(off, rs1, 0b100, rd, opc(Opcode::LOAD)); }
inline uint32_t LHU(uint32_t rd, int32_t off, uint32_t rs1) { return I(off, rs1, 0b101, rd, opc(Opcode::LOAD)); }

inline uint32_t SB(uint32_t rs2, int32_t off, uint32_t rs1) { return S(off, rs2, rs1, 0b000, opc(Opcode::STORE)); }
inline uint32_t SH(uint32_t rs2, int32_t off, uint32_t rs1) { return S(off, rs2, rs1, 0b001, opc(Opcode::STORE)); }
inline uint32_t SW(uint32_t rs2, int32_t off, uint32_t rs1) { return S(off, rs2, rs1, 0b010, opc(Opcode::STORE)); }

inline uint32_t ADDI(uint32_t rd, uint32_t rs1, int32_t imm)  { return I(imm, rs1, 0b000, rd, opc(Opcode::OP_IMM)); }
inline uint32_t SLTI(uint32_t rd, uint32_t rs1, int32_t imm)  { return I(imm, rs1, 0b010, rd, opc(Opcode::OP_IMM)); }
inline uint32_t SLTIU(uint32_t rd, uint32_t rs1, int32_t imm) { return I(imm, rs1, 0b011, rd, opc(Opcode::OP_IMM)); }
inline uint32_t XORI(uint32_t rd, uint32_t rs1, int32_t imm)  { return I(imm, rs1, 0b100, rd, opc(Opcode::OP_IMM)); }
inline uint32_t ORI(uint32_t rd, uint32_t rs1, int32_t imm)   { return I(imm, rs1, 0b110, rd, opc(Opcode::OP_IMM)); }
inline uint32_t ANDI(uint32_t rd, uint32_t rs1, int32_t imm)  { return I(imm, rs1, 0b111, rd, opc(Opcode::OP_IMM)); }
inline uint32_t SLLI(uint32_t rd, uint32_t rs1, uint32_t shamt) { return I(static_cast<int32_t>(shamt & 0x1F), rs1, 0b001, rd, opc(Opcode::OP_IMM)); }
inline uint32_t SRLI(uint32_t rd, uint32_t rs1, uint32_t shamt) { return I(static_cast<int32_t>(shamt & 0x1F), rs1, 0b101, rd, opc(Opcode::OP_IMM)); }
inline uint32_t SRAI(uint32_t rd, uint32_t rs1, uint32_t shamt) { return I(static_cast<int32_t>((0b0100000u << 5) | (shamt & 0x1F)), rs1, 0b101, rd, opc(Opcode::OP_IMM)); }

inline uint32_t ADD(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0000000, rs2, rs1, 0b000, rd, opc(Opcode::OP)); }
inline uint32_t SUB(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0100000, rs2, rs1, 0b000, rd, opc(Opcode::OP)); }
inline uint32_t SLL(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0000000, rs2, rs1, 0b001, rd, opc(Opcode::OP)); }
inline uint32_t SLT(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0000000, rs2, rs1, 0b010, rd, opc(Opcode::OP)); }
inline uint32_t SLTU(uint32_t rd, uint32_t rs1, uint32_t rs2) { return R(0b0000000, rs2, rs1, 0b011, rd, opc(Opcode::OP)); }
inline uint32_t XOR(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0000000, rs2, rs1, 0b100, rd, opc(Opcode::OP)); }
inline uint32_t SRL(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0000000, rs2, rs1, 0b101, rd, opc(Opcode::OP)); }
inline uint32_t SRA(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0100000, rs2, rs1, 0b101, rd, opc(Opcode::OP)); }
inline uint32_t OR(uint32_t rd, uint32_t rs1, uint32_t rs2)   { return R(0b0000000, rs2, rs1, 0b110, rd, opc(Opcode::OP)); }
inline uint32_t AND(uint32_t rd, uint32_t rs1, uint32_t rs2)  { return R(0b0000000, rs2, rs1, 0b111, rd, opc(Opcode::OP)); }

inline uint32_t FENCE()  { return I(0, 0, 0b000, 0, opc(Opcode::MISC_MEM)); }
inline uint32_t ECALL()  { return I(0, 0, 0b000, 0, opc(Opcode::SYSTEM)); }
inline uint32_t EBREAK() { return I(1, 0, 0b000, 0, opc(Opcode::SYSTEM)); }

} // namespace asmb
