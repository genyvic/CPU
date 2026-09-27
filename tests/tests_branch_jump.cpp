// Directed tests for the six branches (taken, not-taken, forward, backward)
// and for JAL/JALR, including the classic JALR hazard: rs1 must be read
// before rd is written, since they can alias (e.g. "jalr x1, 0(x1)").
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

// A 3-instruction skeleton: `branchInstr` either skips the marker write
// (taken -> lands directly on ECALL, x3 stays 0) or falls into it
// (not-taken -> x3 becomes 1, then falls through to ECALL). The offset is
// always +8 here, so this also covers the "_forward" bin for every branch.
TestCase forwardBranchTest(std::string name, uint32_t branchInstr, uint32_t expectedX3) {
    auto t = base(std::move(name));
    t.program = {branchInstr, ADDI(3, 0, 1), ECALL()};
    t.expectedRegs = {{3, expectedX3}};
    return t;
}

// A branch that is NOT taken, with a negative (backward) offset. Since it's
// never actually taken, the underflowed target address is never fetched --
// this purely exists to hit each branch's "_backward" coverage bin cheaply.
TestCase backwardNotTakenTest(std::string name, uint32_t branchInstr) {
    auto t = base(std::move(name));
    t.program = {branchInstr, ECALL()};
    return t;
}

} // namespace

void registerBranchJumpTests(TestHarness& h) {
    // --- taken / not-taken / forward, one pair per branch ---------------
    {
        auto t = forwardBranchTest("branch_beq_taken", BEQ(1, 2, 8), 0);
        t.initialRegs = {{1, 5}, {2, 5}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_beq_not_taken", BEQ(1, 2, 8), 1);
        t.initialRegs = {{1, 5}, {2, 6}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bne_taken", BNE(1, 2, 8), 0);
        t.initialRegs = {{1, 5}, {2, 6}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bne_not_taken", BNE(1, 2, 8), 1);
        t.initialRegs = {{1, 5}, {2, 5}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_blt_taken", BLT(1, 2, 8), 0);
        t.initialRegs = {{1, 0xFFFFFFFF}, {2, 1}}; // -1 < 1
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_blt_not_taken", BLT(1, 2, 8), 1);
        t.initialRegs = {{1, 5}, {2, 3}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bge_taken", BGE(1, 2, 8), 0);
        t.initialRegs = {{1, 5}, {2, 3}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bge_not_taken", BGE(1, 2, 8), 1);
        t.initialRegs = {{1, 0xFFFFFFFF}, {2, 1}}; // -1 < 1, so BGE not taken
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bltu_taken", BLTU(1, 2, 8), 0);
        t.initialRegs = {{1, 1}, {2, 0xFFFFFFFF}}; // 1 < huge unsigned
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bltu_not_taken", BLTU(1, 2, 8), 1);
        t.initialRegs = {{1, 0xFFFFFFFF}, {2, 1}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bgeu_taken", BGEU(1, 2, 8), 0);
        t.initialRegs = {{1, 0xFFFFFFFF}, {2, 1}};
        h.addTest(t);
    }
    {
        auto t = forwardBranchTest("branch_bgeu_not_taken", BGEU(1, 2, 8), 1);
        t.initialRegs = {{1, 1}, {2, 0xFFFFFFFF}};
        h.addTest(t);
    }

    // --- backward bin, one per branch (never actually taken) -------------
    {
        auto t = backwardNotTakenTest("branch_beq_backward", BEQ(1, 2, -4));
        t.initialRegs = {{1, 1}, {2, 2}};
        h.addTest(t);
    }
    {
        auto t = backwardNotTakenTest("branch_bne_backward", BNE(1, 2, -4));
        t.initialRegs = {{1, 2}, {2, 2}};
        h.addTest(t);
    }
    {
        auto t = backwardNotTakenTest("branch_blt_backward", BLT(1, 2, -4));
        t.initialRegs = {{1, 5}, {2, 3}};
        h.addTest(t);
    }
    {
        auto t = backwardNotTakenTest("branch_bge_backward", BGE(1, 2, -4));
        t.initialRegs = {{1, 3}, {2, 5}};
        h.addTest(t);
    }
    {
        auto t = backwardNotTakenTest("branch_bltu_backward", BLTU(1, 2, -4));
        t.initialRegs = {{1, 5}, {2, 3}};
        h.addTest(t);
    }
    {
        auto t = backwardNotTakenTest("branch_bgeu_backward", BGEU(1, 2, -4));
        t.initialRegs = {{1, 3}, {2, 5}};
        h.addTest(t);
    }

    // --- JAL --------------------------------------------------------------
    {
        // JAL x1, +8: jumps over the marker instruction at +4, and rd gets
        // the return address (pc + 4), not the jump target.
        auto t = base("jal_forward_sets_link_register");
        t.program = {JAL(1, 8), ADDI(3, 0, 999), ECALL()};
        t.expectedRegs = {{1, 4}, {3, 0}};
        h.addTest(t);
    }
    {
        // A small backward loop: JAL with rd = x0 (plain jump, no link
        // needed) back to the top, counted down by x1 via a BNE that then
        // breaks out. Covers a backward J-format jump plus reuses BNE.
        auto t = base("jal_backward_loop");
        t.initialRegs = {{1, 3}};
        t.program = {
            ADDI(1, 1, -1),      // 0: loop: x1--
            BNE(1, 0, -4),       // 4: if x1 != 0, branch back to 0 (backward branch)
            JAL(0, 4),           // 8: once the loop exits, jump straight to ECALL
            ECALL(),             // 12
        };
        // BNE does the actual looping (and covers the backward-branch bin
        // again); the trailing JAL(0, 4) just keeps a plain rd=x0 jump in
        // the mix for JAL's own coverage.
        t.expectedRegs = {{1, 0}};
        h.addTest(t);
    }

    // --- JALR ---------------------------------------------------------------
    {
        // rd == rs1 == x1: the classic hazard. Old x1 (16) must be used to
        // compute the jump target BEFORE x1 is overwritten with the link
        // address (pc + 4 = 8). If the implementation writes rd first,
        // this test falls through into the x3=999 marker and fails.
        auto t = base("jalr_rd_equals_rs1_hazard");
        t.program = {
            ADDI(1, 0, 16),  // 0: x1 = 16 (points at the ECALL below)
            JALR(1, 1, 0),   // 4: target = old x1 (16); x1 <- pc+4 = 8
            ADDI(3, 0, 999), // 8: must be skipped
            ADDI(3, 0, 888), // 12: must be skipped
            ECALL(),         // 16
        };
        t.expectedRegs = {{1, 8}, {3, 0}};
        h.addTest(t);
    }
    {
        // Also exercises the "(rs1 + imm) & ~1" bit-clearing: 13 + 4 = 17,
        // which must be masked down to 16, the ECALL's address.
        auto t = base("jalr_target_low_bit_cleared");
        t.initialRegs = {{2, 13}};
        t.program = {
            JALR(1, 2, 4),   // 0: target = (13 + 4) & ~1 = 16; x1 <- 4
            ADDI(3, 0, 999), // 4: skipped
            ADDI(3, 0, 888), // 8
            ADDI(3, 0, 777), // 12
            ECALL(),         // 16
        };
        t.expectedRegs = {{1, 4}, {3, 0}};
        h.addTest(t);
    }
}
