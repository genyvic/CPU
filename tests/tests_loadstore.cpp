// Directed tests for LB/LH/LW/LBU/LHU and SB/SH/SW: sign extension vs zero
// extension, byte-offset-within-word coverage, negative-offset addressing,
// and truncation on narrow stores.
#include "TestRegistry.h"

#include "Assembler.h"

namespace {

using namespace asmb;

TestCase base(std::string name) {
    TestCase t;
    t.name = std::move(name);
    t.expectedHaltReason = "ecall";
    return t;
}

} // namespace

void registerLoadStoreTests(TestHarness& h) {
    // Word 0xAABBCCDD at 0x100 -> bytes (LE): [0x100]=DD [0x101]=CC [0x102]=BB [0x103]=AA
    // gives one test per byte-offset-within-word bin, and offset 3 (0xAA)
    // doubles as an LBU high-bit / zero-extension case.
    {
        auto t = base("load_lbu_offset0");
        t.initialMemWords = {{0x100, 0xAABBCCDD}};
        t.program = {LBU(3, 0x100, 0), ECALL()};
        t.expectedRegs = {{3, 0xDD}};
        h.addTest(t);
    }
    {
        auto t = base("load_lbu_offset1");
        t.initialMemWords = {{0x100, 0xAABBCCDD}};
        t.program = {LBU(3, 0x101, 0), ECALL()};
        t.expectedRegs = {{3, 0xCC}};
        h.addTest(t);
    }
    {
        auto t = base("load_lbu_offset2");
        t.initialMemWords = {{0x100, 0xAABBCCDD}};
        t.program = {LBU(3, 0x102, 0), ECALL()};
        t.expectedRegs = {{3, 0xBB}};
        h.addTest(t);
    }
    {
        auto t = base("load_lbu_offset3_highbit_zero_extends");
        t.initialMemWords = {{0x100, 0xAABBCCDD}};
        t.program = {LBU(3, 0x103, 0), ECALL()};
        t.expectedRegs = {{3, 0xAA}}; // not sign-extended, even though bit 7 is set
        h.addTest(t);
    }

    {
        auto t = base("load_lb_sign_extends_negative");
        t.initialMemWords = {{0x104, 0x000000FF}};
        t.program = {LB(3, 0x104, 0), ECALL()};
        t.expectedRegs = {{3, 0xFFFFFFFF}};
        h.addTest(t);
    }
    {
        auto t = base("load_lb_sign_extends_nonneg");
        t.initialMemWords = {{0x108, 0x0000007F}};
        t.program = {LB(3, 0x108, 0), ECALL()};
        t.expectedRegs = {{3, 0x0000007F}};
        h.addTest(t);
    }
    {
        auto t = base("load_lh_sign_extends_negative");
        t.initialMemWords = {{0x10C, 0x00008000}};
        t.program = {LH(3, 0x10C, 0), ECALL()};
        t.expectedRegs = {{3, 0xFFFF8000}};
        h.addTest(t);
    }
    {
        auto t = base("load_lh_sign_extends_nonneg");
        t.initialMemWords = {{0x110, 0x00007FFF}};
        t.program = {LH(3, 0x110, 0), ECALL()};
        t.expectedRegs = {{3, 0x00007FFF}};
        h.addTest(t);
    }
    {
        auto t = base("load_lhu_highbit_zero_extends");
        t.initialMemWords = {{0x10C, 0x00008000}};
        t.program = {LHU(3, 0x10C, 0), ECALL()};
        t.expectedRegs = {{3, 0x00008000}};
        h.addTest(t);
    }
    {
        auto t = base("load_lw_basic");
        t.initialMemWords = {{0x100, 0x12345678}};
        t.program = {LW(3, 0x100, 0), ECALL()};
        t.expectedRegs = {{3, 0x12345678}};
        h.addTest(t);
    }
    {
        // Base register is nonzero and the offset is negative -- exercises
        // the general rs1 + sign-extended-imm address computation, not
        // just the x0-base special case above.
        auto t = base("load_lw_negative_offset_from_nonzero_base");
        t.initialRegs = {{1, 0x300}};
        t.initialMemWords = {{0x2FC, 0x11223344}};
        t.program = {LW(3, -4, 1), ECALL()};
        t.expectedRegs = {{3, 0x11223344}};
        h.addTest(t);
    }

    {
        auto t = base("store_sb_truncates_to_low_byte");
        t.initialRegs = {{1, 0xFFFFFFAB}};
        t.program = {SB(1, 0x200, 0), ECALL()};
        t.expectedMem = {{0x200, 0xAB, 1}};
        h.addTest(t);
    }
    {
        auto t = base("store_sh_truncates_to_low_halfword");
        t.initialRegs = {{1, 0xFFFF1234}};
        t.program = {SH(1, 0x204, 0), ECALL()};
        t.expectedMem = {{0x204, 0x1234, 2}};
        h.addTest(t);
    }
    {
        auto t = base("store_sw_basic");
        t.initialRegs = {{1, 0xDEADBEEF}};
        t.program = {SW(1, 0x208, 0), ECALL()};
        t.expectedMem = {{0x208, 0xDEADBEEF, 4}};
        h.addTest(t);
    }
    {
        auto t = base("store_then_load_round_trip");
        t.initialRegs = {{1, 0xCAFEBABE}};
        t.program = {SW(1, 0x20C, 0), LW(2, 0x20C, 0), ECALL()};
        t.expectedRegs = {{2, 0xCAFEBABE}};
        t.expectedMem = {{0x20C, 0xCAFEBABE, 4}};
        h.addTest(t);
    }
    {
        // Store through a nonzero base register with a nonzero offset.
        auto t = base("store_sb_nonzero_base_and_offset");
        t.initialRegs = {{1, 0xAB}, {2, 0x310}};
        t.program = {SB(1, 4, 2), ECALL()};
        t.expectedMem = {{0x314, 0xAB, 1}};
        h.addTest(t);
    }
}
