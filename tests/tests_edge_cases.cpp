// Edge cases and negative tests: the exception paths (illegal instruction,
// misaligned fetch, out-of-bounds memory, max-cycle timeout) plus a couple
// of extra int-extreme/x0 checks that don't fit neatly under ALU or
// load/store.
#include "TestRegistry.h"

#include "Assembler.h"

namespace {

using namespace asmb;

TestCase base(std::string name) {
    TestCase t;
    t.name = std::move(name);
    return t;
}

} // namespace

void registerEdgeCaseTests(TestHarness& h) {
    {
        // 0xFFFFFFFF's low 7 bits (the opcode field) don't match any
        // RV32I opcode, so the decoder must reject it outright.
        auto t = base("edge_illegal_instruction_halts_cleanly");
        t.program = {0xFFFFFFFFu};
        t.expectedHaltReason = "illegal instruction";
        h.addTest(t);
    }
    {
        // JALR x1, 2(x0): target = (0 + 2) & ~1 = 2, which clears bit 0 but
        // is still not 4-byte aligned -- the fetch of the instruction at
        // PC=2 must be rejected as misaligned, even though bit 0 alone
        // would have been fine.
        auto t = base("edge_misaligned_instruction_fetch");
        t.program = {JALR(1, 0, 2)};
        t.expectedHaltReason = "misaligned instruction fetch";
        h.addTest(t);
    }
    {
        // memSize=4 holds exactly one instruction word and nothing more;
        // running off the end of it (no ECALL) must fault, not read
        // garbage or wrap around.
        auto t = base("edge_out_of_bounds_instruction_fetch");
        t.memSize = 4;
        t.program = {ADDI(1, 0, 1)};
        t.expectedHaltReason = "out-of-bounds memory access";
        h.addTest(t);
    }
    {
        // A self-branch (BEQ x0, x0, 0) never advances the PC, so with a
        // small max-cycle budget the only way out is the timeout halt.
        auto t = base("edge_max_cycle_timeout");
        t.maxCycles = 5;
        t.program = {BEQ(0, 0, 0)};
        t.expectedHaltReason = "max-cycle timeout";
        h.addTest(t);
    }
    {
        auto t = base("edge_lui_to_x0_is_ignored");
        t.program = {LUI(0, 5), ECALL()};
        t.expectedRegs = {{0, 0}};
        t.expectedHaltReason = "ecall";
        h.addTest(t);
    }
    {
        // INT32_MIN < INT32_MAX under signed comparison -- the two most
        // extreme 32-bit values directly against each other.
        auto t = base("edge_int32_extremes_signed_compare");
        t.initialRegs = {{1, 0x80000000}, {2, 0x7FFFFFFF}};
        t.program = {SLT(3, 1, 2), ECALL()};
        t.expectedRegs = {{3, 1}};
        t.expectedHaltReason = "ecall";
        h.addTest(t);
    }
    {
        // AUIPC's result depends on where it's loaded, not just its
        // immediate -- run it from a nonzero base to make sure PC, not 0,
        // is what gets added.
        auto t = base("edge_auipc_uses_actual_pc");
        t.baseAddress = 0x1000;
        t.memSize = 0x1000 + 4096;
        t.program = {AUIPC(3, 1), ECALL()}; // x3 = pc(0x1000) + (1 << 12) = 0x2000
        t.expectedRegs = {{3, 0x2000}};
        t.expectedHaltReason = "ecall";
        h.addTest(t);
    }
    {
        auto t = base("edge_ebreak_halts");
        t.program = {EBREAK()};
        t.expectedHaltReason = "ebreak";
        h.addTest(t);
    }
    {
        auto t = base("edge_fence_is_a_noop");
        t.program = {FENCE(), ADDI(1, 0, 42), ECALL()};
        t.expectedRegs = {{1, 42}};
        t.expectedHaltReason = "ecall";
        h.addTest(t);
    }
}
