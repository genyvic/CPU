// Directed tests for the R-type and I-type ALU instructions: basic
// correctness plus the edge cases that actually tend to break naive
// implementations (rd/rs1/rs2 == x0, rs1 == rs2, rd == rs1, signed overflow
// wraparound, shift-amount boundaries, and the SLTIU
// sign-extend-then-compare-unsigned quirk).
#include "TestRegistry.h"

#include "Assembler.h"

namespace {

using namespace asmb;

TestCase makeTest(std::string name, std::vector<RegExpectation> initRegs,
                   std::vector<uint32_t> program, std::vector<RegExpectation> expectedRegs) {
    TestCase t;
    t.name = std::move(name);
    t.initialRegs = std::move(initRegs);
    t.program = std::move(program);
    t.expectedRegs = std::move(expectedRegs);
    t.expectedHaltReason = "ecall";
    return t;
}

} // namespace

void registerAluTests(TestHarness& h) {
    // --- R-type ---------------------------------------------------------
    h.addTest(makeTest("alu_add_basic", {{1, 5}, {2, 7}},
        {ADD(3, 1, 2), ECALL()}, {{3, 12}}));

    h.addTest(makeTest("alu_add_overflow_wrap", {{1, 0x7FFFFFFF}, {2, 1}},
        {ADD(3, 1, 2), ECALL()}, {{3, 0x80000000}}));

    h.addTest(makeTest("alu_add_rd_x0_ignored", {{1, 5}, {2, 7}},
        {ADD(0, 1, 2), ECALL()}, {{0, 0}}));

    h.addTest(makeTest("alu_add_rs1_eq_rs2", {{1, 9}},
        {ADD(3, 1, 1), ECALL()}, {{3, 18}}));

    h.addTest(makeTest("alu_add_rd_eq_rs1", {{1, 5}, {2, 3}},
        {ADD(1, 1, 2), ECALL()}, {{1, 8}}));

    h.addTest(makeTest("alu_sub_basic", {{1, 10}, {2, 3}},
        {SUB(3, 1, 2), ECALL()}, {{3, 7}}));

    h.addTest(makeTest("alu_sub_negative_result", {{1, 3}, {2, 10}},
        {SUB(3, 1, 2), ECALL()}, {{3, 0xFFFFFFF9}}));

    h.addTest(makeTest("alu_sub_overflow_wrap", {{1, 0x80000000}, {2, 1}},
        {SUB(3, 1, 2), ECALL()}, {{3, 0x7FFFFFFF}}));

    h.addTest(makeTest("alu_sub_rs1_x0", {{2, 5}},
        {SUB(3, 0, 2), ECALL()}, {{3, 0xFFFFFFFB}}));

    h.addTest(makeTest("alu_sll_basic", {{1, 1}, {2, 4}},
        {SLL(3, 1, 2), ECALL()}, {{3, 16}}));

    h.addTest(makeTest("alu_sll_shamt_masked_to_5_bits", {{1, 1}, {2, 0x23}}, // 35 -> shamt 3
        {SLL(3, 1, 2), ECALL()}, {{3, 8}}));

    h.addTest(makeTest("alu_slt_signed", {{1, 0xFFFFFFFF}, {2, 1}}, // -1 < 1
        {SLT(3, 1, 2), ECALL()}, {{3, 1}}));

    h.addTest(makeTest("alu_slt_rs2_x0", {{1, 0xFFFFFFFF}},
        {SLT(3, 1, 0), ECALL()}, {{3, 1}}));

    h.addTest(makeTest("alu_sltu_unsigned", {{1, 0xFFFFFFFF}, {2, 1}}, // huge unsigned, not < 1
        {SLTU(3, 1, 2), ECALL()}, {{3, 0}}));

    h.addTest(makeTest("alu_xor_basic", {{1, 0xF0F0F0F0}, {2, 0x0F0F0F0F}},
        {XOR(3, 1, 2), ECALL()}, {{3, 0xFFFFFFFF}}));

    h.addTest(makeTest("alu_srl_logical", {{1, 0x80000000}, {2, 4}},
        {SRL(3, 1, 2), ECALL()}, {{3, 0x08000000}}));

    h.addTest(makeTest("alu_sra_arithmetic", {{1, 0x80000000}, {2, 4}},
        {SRA(3, 1, 2), ECALL()}, {{3, 0xF8000000}}));

    h.addTest(makeTest("alu_or_basic", {{1, 0x0F0F0F0F}, {2, 0xF0F0F0F0}},
        {OR(3, 1, 2), ECALL()}, {{3, 0xFFFFFFFF}}));

    h.addTest(makeTest("alu_and_basic", {{1, 0xFF00FF00}, {2, 0x00FF00FF}},
        {AND(3, 1, 2), ECALL()}, {{3, 0}}));

    // --- I-type -----------------------------------------------------------
    h.addTest(makeTest("alu_addi_negative_imm", {{1, 10}},
        {ADDI(3, 1, -3), ECALL()}, {{3, 7}}));

    h.addTest(makeTest("alu_addi_imm_min", {{1, 0}},
        {ADDI(3, 1, -2048), ECALL()}, {{3, 0xFFFFF800}}));

    h.addTest(makeTest("alu_addi_imm_max", {{1, 0}},
        {ADDI(3, 1, 2047), ECALL()}, {{3, 2047}}));

    h.addTest(makeTest("alu_addi_overflow_wrap", {{1, 0x7FFFFFFF}},
        {ADDI(3, 1, 1), ECALL()}, {{3, 0x80000000}}));

    h.addTest(makeTest("alu_slti_signed", {{1, 0xFFFFFFFB}}, // -5
        {SLTI(3, 1, 0), ECALL()}, {{3, 1}}));

    // The documented quirk: imm is sign-extended first (-1 -> 0xFFFFFFFF),
    // *then* the comparison is unsigned, so 0 < 0xFFFFFFFF is true.
    h.addTest(makeTest("alu_sltiu_signextend_then_unsigned_compare", {{1, 0}},
        {SLTIU(3, 1, -1), ECALL()}, {{3, 1}}));

    h.addTest(makeTest("alu_xori_imm_zero", {{1, 0xFFFFFFFF}},
        {XORI(3, 1, 0), ECALL()}, {{3, 0xFFFFFFFF}}));

    h.addTest(makeTest("alu_ori_basic", {{1, 0}},
        {ORI(3, 1, 5), ECALL()}, {{3, 5}}));

    h.addTest(makeTest("alu_andi_negative_imm_is_allones", {{1, 0xFFFFFFFF}},
        {ANDI(3, 1, -1), ECALL()}, {{3, 0xFFFFFFFF}}));

    h.addTest(makeTest("alu_slli_shamt_0", {{1, 123}},
        {SLLI(3, 1, 0), ECALL()}, {{3, 123}}));

    h.addTest(makeTest("alu_slli_shamt_31", {{1, 1}},
        {SLLI(3, 1, 31), ECALL()}, {{3, 0x80000000}}));

    h.addTest(makeTest("alu_slli_rd_eq_rs1", {{1, 3}},
        {SLLI(1, 1, 1), ECALL()}, {{1, 6}}));

    h.addTest(makeTest("alu_srli_shamt_mid", {{1, 0x80000000}},
        {SRLI(3, 1, 1), ECALL()}, {{3, 0x40000000}}));

    h.addTest(makeTest("alu_srai_shamt_31_all_sign_bits", {{1, 0x80000000}},
        {SRAI(3, 1, 31), ECALL()}, {{3, 0xFFFFFFFF}}));

    h.addTest(makeTest("alu_srai_rs1_x0", {},
        {SRAI(3, 0, 5), ECALL()}, {{3, 0}}));
}
