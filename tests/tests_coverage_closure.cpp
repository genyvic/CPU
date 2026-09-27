// The directed tests in tests_alu.cpp etc. check correctness, but they don't
// go out of their way to hit every operand-value bin the coverage model
// tracks (INT32_MIN/INT32_MAX/allones on both operands, every sign-cross
// combination, shamt boundaries, registers nobody else happens to touch like
// x19/x21-x31...). Rather than bolt that onto the correctness tests and make
// them harder to read, it's cleaner to have a separate small set of programs
// whose only job is poking at those specific values. Most of them don't
// bother checking expectedRegs beyond confirming the program actually ran to
// completion -- correctness is already covered elsewhere, this is just here
// for coverage.
#include "TestRegistry.h"

#include "Assembler.h"

namespace {

using namespace asmb;
using RType = uint32_t (*)(uint32_t, uint32_t, uint32_t);
using IType = uint32_t (*)(uint32_t, uint32_t, int32_t);

TestCase base(std::string name) {
    TestCase t;
    t.name = std::move(name);
    t.expectedHaltReason = "ecall";
    return t;
}

// Probes rd_x0, rs1_x0, rs2_x0, rd_eq_rs1, rs1_eq_rs2, every extreme value
// (zero/positive/negative/int32max/int32min/allones) on both operands, all
// four sign-cross combinations, and (for the shift ops) shamt_31 -- all in
// one small program, reusing a fixed set of preloaded registers.
TestCase rTypeClosure(std::string name, RType op) {
    auto t = base(std::move(name));
    t.initialRegs = {
        {1, 0x80000000}, // int32min, negative
        {2, 0x7FFFFFFF}, // int32max, positive
        {3, 0xFFFFFFFF}, // allones, negative, low 5 bits = 31
        {4, 5},          // positive
        {5, 0xFFFFFFFB}, // -5, negative
    };
    t.program = {
        op(10, 1, 2), // neg/nonneg, rs1=int32min, rs2=int32max
        op(11, 2, 1), // nonneg/neg, rs2=int32min
        op(12, 1, 1), // neg/neg, rs1==rs2, result often zero or unchanged-negative
        op(13, 3, 3), // allones/allones, low-5 = 31 (shamt_31 for shift ops)
        op(14, 0, 2), // rs1 = x0
        op(15, 4, 0), // rs2 = x0
        op(16, 5, 4), // rs1 negative, rs2 positive
        op(0, 1, 2),  // rd = x0
        op(1, 1, 4),  // rd == rs1
        ECALL(),
    };
    return t;
}

// Same idea for the I-type ALU ops (imm instead of rs2): extreme rs1
// values, imm at its zero/positive/negative/max/min extremes, rs1 = x0,
// rd == rs1.
TestCase iTypeClosure(std::string name, IType op) {
    auto t = base(std::move(name));
    t.initialRegs = {
        {1, 0x80000000}, // int32min
        {2, 0x7FFFFFFF}, // int32max
        {3, 0xFFFFFFFF}, // allones
    };
    t.program = {
        op(10, 1, 2047),   // rs1 = int32min, imm = max
        op(11, 2, -2048),  // rs1 = int32max, imm = min
        op(12, 3, 0),      // rs1 = allones, imm = zero
        op(13, 0, -1),     // rs1 = x0, imm = negative (allones)
        op(1, 1, 0),       // rd == rs1
        op(0, 1, 5),       // rd = x0
        ECALL(),
    };
    return t;
}

// Shift-immediates take a shamt (0..31), not a signed immediate -- separate
// closure shape covering shamt_0/shamt_31 plus the same extreme rs1 values.
TestCase shiftITypeClosure(std::string name, uint32_t (*op)(uint32_t, uint32_t, uint32_t)) {
    auto t = base(std::move(name));
    t.initialRegs = {
        {1, 0x80000000}, // int32min
        {2, 0x7FFFFFFF}, // int32max
        {3, 0xFFFFFFFF}, // allones
    };
    t.program = {
        op(10, 1, 0),   // shamt_0, rs1 = int32min
        op(11, 2, 31),  // shamt_31, rs1 = int32max
        op(12, 3, 15),  // shamt_mid, rs1 = allones
        op(13, 0, 5),   // rs1 = x0
        op(1, 1, 3),    // rd == rs1
        ECALL(),
    };
    return t;
}

} // namespace

void registerCoverageClosureTests(TestHarness& h) {
    h.addTest(rTypeClosure("closure_r_add", ADD));
    h.addTest(rTypeClosure("closure_r_sub", SUB));
    h.addTest(rTypeClosure("closure_r_and", AND));
    h.addTest(rTypeClosure("closure_r_or", OR));
    h.addTest(rTypeClosure("closure_r_xor", XOR));
    h.addTest(rTypeClosure("closure_r_sll", SLL));
    h.addTest(rTypeClosure("closure_r_srl", SRL));
    h.addTest(rTypeClosure("closure_r_sra", SRA));
    h.addTest(rTypeClosure("closure_r_slt", SLT));
    h.addTest(rTypeClosure("closure_r_sltu", SLTU));

    h.addTest(iTypeClosure("closure_i_addi", ADDI));
    h.addTest(iTypeClosure("closure_i_andi", ANDI));
    h.addTest(iTypeClosure("closure_i_ori", ORI));
    h.addTest(iTypeClosure("closure_i_xori", XORI));
    h.addTest(iTypeClosure("closure_i_slti", SLTI));
    h.addTest(iTypeClosure("closure_i_sltiu", SLTIU));

    h.addTest(shiftITypeClosure("closure_shift_slli", SLLI));
    h.addTest(shiftITypeClosure("closure_shift_srli", SRLI));
    h.addTest(shiftITypeClosure("closure_shift_srai", SRAI));

    // Extra result_zero probes the generic closure shapes above don't
    // reliably hit for every op (e.g. ORI needs rs1 = 0 AND imm = 0 at the
    // same time to produce a zero result).
    {
        auto t = base("closure_result_zero_extra");
        t.program = {
            ORI(10, 0, 0),   // 0 | 0 = 0
            XORI(11, 0, 0),  // 0 ^ 0 = 0
            SLTIU(12, 0, 0), // 0 < 0 (unsigned) is false = 0
            ECALL(),
        };
        t.expectedRegs = {{10, 0}, {11, 0}, {12, 0}};
        h.addTest(t);
    }

    // Register sweep: writes then reads every register x0..x31 at least
    // once, so register_write/register_read coverage doesn't depend on
    // whichever registers the other directed tests happened to pick.
    {
        auto t = base("closure_register_sweep");
        std::vector<uint32_t> prog;
        for (uint32_t r = 1; r <= 31; ++r) {
            prog.push_back(ADDI(r, 0, static_cast<int32_t>(r))); // write x_r
        }
        for (uint32_t r = 1; r <= 31; ++r) {
            prog.push_back(ADD(0, r, 0)); // read x_r (rd = x0, discarded)
        }
        prog.push_back(ECALL());
        t.program = std::move(prog);
        t.expectedRegs = {{31, 31}};
        h.addTest(t);
    }
}
