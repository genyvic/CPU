// Small hand-assembled multi-instruction programs -- the kind of thing
// you'd actually run through a real CPU model to convince yourself control
// flow, memory addressing, and the calling convention all work together,
// not just each instruction in isolation.
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

void registerProgramTests(TestHarness& h) {
    // --- sum 1..N via a counted loop --------------------------------------
    // x10=N x11=sum x12=i. "BLT N,i,exit" branches once i > N.
    {
        auto t = base("program_loop_sum_1_to_5");
        t.program = {
            ADDI(10, 0, 5),   // 0:  N = 5
            ADDI(11, 0, 0),   // 4:  sum = 0
            ADDI(12, 0, 1),   // 8:  i = 1
            BLT(10, 12, 16),  // 12: loop: if N < i, goto exit (28)
            ADD(11, 11, 12),  // 16: sum += i
            ADDI(12, 12, 1),  // 20: i++
            JAL(0, -12),      // 24: goto loop (12)
            ECALL(),          // 28: exit
        };
        t.expectedRegs = {{11, 15}}; // 1+2+3+4+5
        h.addTest(t);
    }

    // --- iterative Fibonacci ------------------------------------------------
    // Classic a,b iteration: after k steps a=fib(k), b=fib(k+1).
    {
        auto t = base("program_fibonacci_10");
        t.program = {
            ADDI(10, 0, 10),   // 0:  N = 10
            ADDI(11, 0, 0),    // 4:  a = fib(0) = 0
            ADDI(12, 0, 1),    // 8:  b = fib(1) = 1
            ADDI(13, 0, 0),    // 12: i = 0
            BGE(13, 10, 24),   // 16: loop: if i >= N, goto exit (40)
            ADD(14, 11, 12),   // 20: tmp = a + b
            ADDI(11, 12, 0),   // 24: a = b
            ADDI(12, 14, 0),   // 28: b = tmp
            ADDI(13, 13, 1),   // 32: i++
            JAL(0, -20),       // 36: goto loop (16)
            ECALL(),           // 40: exit
        };
        t.expectedRegs = {{11, 55}}; // fib(10)
        h.addTest(t);
    }

    // --- sum an array in memory ---------------------------------------------
    {
        auto t = base("program_array_sum");
        t.initialMemWords = {{0x200, 1}, {0x204, 2}, {0x208, 3}, {0x20C, 4}, {0x210, 5}};
        t.program = {
            ADDI(10, 0, 0x200),  // 0:  base
            ADDI(11, 0, 5),      // 4:  N
            ADDI(12, 0, 0),      // 8:  i = 0
            ADDI(13, 0, 0),      // 12: sum = 0
            BGE(12, 11, 28),     // 16: loop: if i >= N, goto exit (44)
            SLLI(14, 12, 2),     // 20: byte offset = i * 4
            ADD(14, 14, 10),     // 24: addr = base + offset
            LW(15, 0, 14),       // 28: val = mem[addr]
            ADD(13, 13, 15),     // 32: sum += val
            ADDI(12, 12, 1),     // 36: i++
            JAL(0, -24),         // 40: goto loop (16)
            ECALL(),             // 44: exit
        };
        t.expectedRegs = {{13, 15}}; // 1+2+3+4+5
        h.addTest(t);
    }

    // --- memcpy: copy N words from src[] to dst[] ---------------------------
    {
        auto t = base("program_memcpy");
        t.initialMemWords = {{0x300, 10}, {0x304, 20}, {0x308, 30}};
        t.program = {
            ADDI(10, 0, 0x300),  // 0:  src
            ADDI(11, 0, 0x400),  // 4:  dst
            ADDI(12, 0, 3),      // 8:  N
            ADDI(13, 0, 0),      // 12: i = 0
            BGE(13, 12, 36),     // 16: loop: if i >= N, goto exit (52)
            SLLI(14, 13, 2),     // 20: offset = i * 4
            ADD(14, 14, 10),     // 24: addr_s = src + offset
            LW(16, 0, 14),       // 28: val = mem[addr_s]
            SLLI(15, 13, 2),     // 32: offset again (addr_d recomputed to avoid clobbering x14)
            ADD(15, 15, 11),     // 36: addr_d = dst + offset
            SW(16, 0, 15),       // 40: mem[addr_d] = val
            ADDI(13, 13, 1),     // 44: i++
            JAL(0, -32),         // 48: goto loop (16)
            ECALL(),             // 52: exit
        };
        t.expectedMem = {{0x400, 10, 4}, {0x404, 20, 4}, {0x408, 30, 4}};
        h.addTest(t);
    }

    // --- bubble sort, 4 elements in place ------------------------------------
    {
        auto t = base("program_bubble_sort_4_elements");
        t.initialMemWords = {{0x100, 4}, {0x104, 2}, {0x108, 3}, {0x10C, 1}};
        t.program = {
            ADDI(10, 0, 0x100),   // 0:  base
            ADDI(11, 0, 4),       // 4:  n
            ADDI(20, 11, -1),     // 8:  nm1 = n - 1
            ADDI(12, 0, 0),       // 12: i = 0
            BGE(12, 20, 64),      // 16: OUTER: if i >= nm1, goto DONE (80)
            SUB(18, 20, 12),      // 20: bound = nm1 - i
            ADDI(13, 0, 0),       // 24: j = 0
            BGE(13, 18, 44),      // 28: INNER: if j >= bound, goto INNER_DONE (72)
            SLLI(14, 13, 2),      // 32: addr_j = base + j*4  (offset...)
            ADD(14, 14, 10),      // 36: ...
            ADDI(15, 14, 4),      // 40: addr_j1 = addr_j + 4
            LW(16, 0, 14),        // 44: val_j
            LW(17, 0, 15),        // 48: val_j1
            BGE(17, 16, 12),      // 52: if val_j1 >= val_j, goto SKIP_SWAP (64)
            SW(17, 0, 14),        // 56: mem[addr_j] = val_j1
            SW(16, 0, 15),        // 60: mem[addr_j1] = val_j
            ADDI(13, 13, 1),      // 64: SKIP_SWAP: j++
            JAL(0, -40),          // 68: goto INNER (28)
            ADDI(12, 12, 1),      // 72: INNER_DONE: i++
            JAL(0, -60),          // 76: goto OUTER (16)
            ECALL(),              // 80: DONE
        };
        t.expectedMem = {{0x100, 1, 4}, {0x104, 2, 4}, {0x108, 3, 4}, {0x10C, 4, 4}};
        h.addTest(t);
    }

    // --- function call through JAL/JALR, using the stack ---------------------
    // FUNC computes (a0 + a1) * 2, spilling/restoring ra (x1) via the stack
    // pointer (x2) even though it doesn't strictly need to -- that's the
    // point of the test.
    {
        auto t = base("program_function_call_with_stack");
        t.program = {
            ADDI(2, 0, 1024),   // 0:  sp = 1024
            ADDI(10, 0, 7),     // 4:  a0 = 7
            ADDI(11, 0, 5),     // 8:  a1 = 5
            JAL(1, 8),          // 12: call FUNC (20), ra = x1 = 16
            ECALL(),            // 16: back here with the result in a0

            // FUNC:
            ADDI(2, 2, -4),     // 20: sp -= 4
            SW(1, 0, 2),        // 24: mem[sp] = ra
            ADD(10, 10, 11),    // 28: a0 = a0 + a1
            SLLI(10, 10, 1),    // 32: a0 = a0 * 2
            LW(1, 0, 2),        // 36: ra = mem[sp]
            ADDI(2, 2, 4),      // 40: sp += 4
            JALR(0, 1, 0),      // 44: return to ra
        };
        t.expectedRegs = {{10, 24}, {1, 16}, {2, 1024}}; // (7+5)*2, ra/sp both restored
        h.addTest(t);
    }
}
