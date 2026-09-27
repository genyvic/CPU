# Design notes

## Architecture

The model is split the way a DV environment usually is: a DUT, stimulus,
and a checker/monitor layer that's kept independent of the DUT itself.

```
src/Types.h          shared enums/structs (Opcode, Format, InstrName, DecodedInstr)
src/RegisterFile     32 x 32-bit regs, x0 hardwired to zero
src/Memory           byte-addressable, little-endian, bounds-checked
src/Decoder          raw word -> DecodedInstr (pure function, no CPU state)
src/Disassembler     DecodedInstr -> text, for traces/CLI dumps
src/CPU              fetch -> decode -> execute -> writeback -> PC update
src/Tracer           DecodedInstr/RetireRecord -> trace file lines
src/Coverage         RetireRecord -> coverpoint/bin hit counts
src/ProgramLoader    .hex file -> Memory
src/main.cpp         CLI: load a program, run it, optionally trace/report coverage

tests/Assembler.h    hand-rolled RV32I encoder (no toolchain available)
tests/TestHarness    run a program, compare final reg/mem state, collect coverage
tests/tests_*.cpp    the actual test cases, grouped by what they exercise
tests/run_tests.cpp  pulls every tests_*.cpp file together, writes reports
```

The one design decision that shapes everything else: **`CPU::step()`
doesn't know that tracing or coverage exist.** It just returns a
`RetireRecord` -- PC, decoded instruction, what got written where, whether a
branch was taken, whether it halted and why. `Tracer` and `Coverage` are
both just functions of that record. This is the same shape a real
verification environment has (DUT emits a retire/commit record, a monitor
and a scoreboard both consume it independently) and it's what let the
coverage model change twice during development (see below) without ever
touching `CPU.cpp`.

`Decoder` (raw word -> fields) and `tests/Assembler.h` (fields -> raw word)
are also intentionally two separate, independently-written pieces of code
that happen to be inverses of each other, both keyed off the same `Opcode`
enum from `Types.h` so they can't disagree about *which* bit pattern means
*which* opcode -- but the actual field-packing/unpacking arithmetic in each
is written from scratch, not shared. Every directed test that encodes a
program and then checks the CPU's resulting register/memory state is
implicitly cross-checking that these two independent implementations agree.

## Verification strategy

**Directed tests** (`tests_alu.cpp`, `tests_loadstore.cpp`,
`tests_branch_jump.cpp`): one or more tests per instruction, covering the
specific edge cases that are easy to get subtly wrong -- the SLTIU
sign-extend-then-unsigned-compare quirk, the JALR read-rs1-before-write-rd
hazard when `rd == rs1`, the `& ~1` target masking, signed overflow
wraparound on ADD/SUB, shift amounts being masked to 5 bits, sign vs zero
extension on narrow loads.

**Program tests** (`tests_programs.cpp`): hand-assembled multi-instruction
programs (counted loop, iterative Fibonacci, array sum, memcpy, bubble sort,
a function call through JAL/JALR using the stack) -- these catch bugs that
only show up when several instructions interact, not just each one in
isolation. Two of these (`program_array_sum`, `program_memcpy`) initially
had branch-offset arithmetic errors from hand-computing addresses; see
"Bugs found" below.

**Constrained-random tests** (`tests_random.cpp`): seeded sequences of ALU
and load/store instructions (deliberately no control flow, so there's never
an open question of what "should" happen), checked **instruction-by-
instruction** against `RefMachine`, a second RV32I implementation written
from scratch with no shared code path through `CPU.cpp`/`Decoder.cpp`. Both
the real CPU and the reference model execute the same generated instruction
and get compared -- all 32 registers plus the touched memory region -- after
every single step, not just at the end, so a divergence gets caught at the
exact instruction that caused it. Failures print the seed for exact
reproduction.

**Decoder unit tests** (`tests_decoder.cpp`): direct checks of
`Decoder::decode()` on hand-built words, independent of running anything on
a CPU -- field extraction and sign-extension for every format (R/I/S/B/U/J),
including negative immediates at both ends of each format's range.

## Coverage and coverage closure

Coverage is a generic `coverpoint -> bin -> hit count` table
(`src/Coverage.cpp`) with every expected bin pre-declared, so an unhit bin
shows up as a real, visible hole instead of just being silently absent.
Coverpoints: instruction, format (R/I/S/B/U/J), branch (taken/not-taken x
forward/backward, per branch), operand/value (rd/rs1/rs2 == x0, rs1==rs2,
rd==rs1, operand value extremes, immediate extremes, shift-amount buckets,
result zero/negative/overflow, per ALU instruction), memory (per load/store
op, byte offset within word, sign vs zero extension), register read/write
(x0..x31), and a small instruction x operand-sign cross-coverpoint.

The first full run, right after the directed/program/edge-case tests were
all passing, looked like this:

```
Tests: 124/125 passed | Instruction coverage: 97.5% (39/40) | Functional coverage: 62.3%
192 coverage holes
```

Reading `out/coverage_report.txt`'s holes section showed three different
things going on, not just "need more tests":

1. **One test was actually failing setup**, not producing a coverage gap on
   its own merits -- `edge_auipc_uses_actual_pc` loaded its program at
   0x1000 with the default 4096-byte memory, which doesn't fit. Fixing the
   test's `memSize` closed the `instruction: auipc` hole immediately.
2. **Two bins in the coverage model itself were unfillable by
   construction**, which pure test-writing can never close: `SLLI`/`SRLI`/
   `SRAI`'s "immediate" field is really a shift amount, not a signed value,
   so their `imm_*` bins could never hit; and `SLT`/`SLTU`/`SLTI`/`SLTIU`
   only ever produce 0 or 1, so their `_result_negative` bin could never
   hit either. Both were fixed in `Coverage.cpp` by excluding those
   instructions from the bins that don't apply to them.
3. **Genuine gaps**: extreme operand values (`INT32_MAX`/`INT32_MIN`/all-
   ones) on operands the basic directed tests hadn't happened to use,
   sign-cross combinations, and registers x19/x21-x31 (nothing had touched
   them -- the directed tests mostly used low register numbers, and the
   random tests were originally restricted to x0-x10 to keep addressing
   predictable).

That last category is what `tests_coverage_closure.cpp` exists for: dense,
correctness-agnostic probe programs (one per ALU instruction, plus a
register sweep) that exist purely to exercise specific value/register
combinations, layered on top of the correctness-focused tests rather than
replacing them. The random tests' register range was also widened from
x0-x10 to the full x0-x31 (redirecting any accidental write to the
load/store base-pointer register to x0, so addressing stays safe).

After that closure pass:

```
Tests: 146/146 passed | Instruction coverage: 100.0% (40/40) | Functional coverage: 100.0%
0 coverage holes
```

## Bugs found during development

- `Decoder.cpp` referenced `Opcode::LUI` instead of `Opcode::LUI_OP` --
  caught immediately at compile time (undeclared identifier), never reached
  a running program.
- `edge_auipc_uses_actual_pc`'s test setup didn't allocate enough memory
  for its own base address (see above) -- a test bug, not a CPU bug, but it
  masqueraded as a coverage gap until traced back.
- Two coverage bins (shift-immediate `imm_*`, compare-op `result_negative`)
  were unfillable by design, not by missing tests -- see above.
- `program_array_sum` and `program_memcpy`'s loop-exit branch offsets were
  each off by one instruction (4 bytes) in the first draft -- caught by
  re-deriving every address from the actual instruction list rather than
  trusting the first pass of by-hand arithmetic, before ever running them.
- `main.cpp`'s `--dump-regs` set the output stream to left-justify (for the
  register-index column) and never reset it back to right-justify before
  printing the hex value, so `std::setw(8)` padded on the *right* instead of
  the left (`0x37000000` printed for a register actually holding `0x37`).
  This one is the most interesting of the bunch: the automated test suite
  never caught it, because `TestHarness` reads register values directly
  through `CPU::regs()` rather than by parsing the CLI's printed output --
  it only surfaced by manually running `rv32i-sim` on a real program and
  reading the register dump. A reminder that "the tests pass" and "the tool
  works" aren't quite the same claim when part of the tool is its
  human-facing output formatting.

## Possible next steps

- **A pipelined model.** A classic 5-stage IF/ID/EX/MEM/WB pipeline with
  hazard detection and forwarding, using this single-cycle model as the
  golden architectural reference -- run the same program through both, and
  the two commit/retire streams (this simulator's `RetireRecord`s vs. the
  pipeline's) should match exactly modulo timing.
- **A SystemVerilog RTL core**, verified against this simulator as the
  golden reference model in a small testbench (Verilator-based, or a
  minimal UVM environment) -- `RetireRecord` is already close to the shape
  a golden-model compare scoreboard wants (PC, instruction, register/memory
  writes, branch outcome).
- **riscv-tests / riscv-arch-test.** Running the community RISC-V
  compliance suite against this model would need either a small ELF loader
  or a converter from those tests' binaries into this repo's `.hex` word
  format, and would cross-check this project's own hand-written tests
  against an independently maintained reference suite.
- **RV32M (multiply/divide)** and/or **Zicsr**, to grow this from "RV32I
  base integer" into something closer to a real minimal application-class
  core.
