# rv32i-sim

An instruction-level simulator of a 32-bit RISC-V processor (the RV32I base
integer ISA), built as a small hardware design-verification environment
rather than just an interpreter: a CPU model (the DUT), test programs
(stimulus), a self-checking test harness, an execution trace file, and a
functional coverage model with coverage closure.

## Build

Requires CMake 3.16+ and a C++17 compiler. Verified locally with MSVC
(`/W4`, zero warnings); written to standard, portable C++17 with no
compiler-specific extensions, so GCC/Clang should work too (not locally
verified -- no GCC/Clang toolchain was available on the machine this was
built on).

```
cmake -S . -B build
cmake --build build
```

This builds two executables: `rv32i-sim` (the CLI simulator) and
`tests/run_tests` (the test suite). Opening the repo folder directly in
Visual Studio (File > Open > Folder) picks up `CMakeLists.txt`
automatically via VS's built-in CMake integration.

## Running a program

```
build/rv32i-sim programs/fibonacci_10.hex --dump-regs
```

```
HALT: ecall
cycles: 66  instructions retired: 66
branches: 11 (1 taken, 10 not taken)
loads: 0  stores: 0

registers:
  x10 = 0x0000000a
  x11 = 0x00000037
  ...
```

(`x11 = 0x37 = 55 = fib(10)`.)

Options:

```
--trace PATH          write a per-instruction trace file
--coverage PATH       write a coverage report for this run
--max-cycles N        cycle limit before forcing a timeout halt (default 100000)
--mem-size BYTES      memory size in bytes (default 65536)
--reset-pc ADDR       initial PC / program load address (default 0)
--dump-regs           print all 32 registers after halting
--dump-mem ADDR LEN   print LEN bytes of memory starting at ADDR
```

A `.hex` program file is one 32-bit instruction word per line, in hex
(optionally `0x`-prefixed); blank lines and `#` comments are skipped. See
`programs/` for examples, and `tests/Assembler.h` for how they're built
programmatically (there's no RISC-V toolchain involved anywhere in this
repo).

## Trace file

Every retired instruction gets one line: cycle, PC, raw instruction word,
disassembly, and its effects (register write, memory access, branch
taken/not-taken), finishing with a halt reason and a register dump:

```
cycle   pc          instr       disassembly               effects
0000001 0x00000000  0x40000113  addi x2, x0, 1024         x2 <- 0x00000400
0000004 0x0000000c  0x008000ef  jal x1, 8                 x1 <- 0x00000010
0000006 0x00000018  0x00112023  sw x1, 0(x2)              mem[0x000003fc] <- 0x00000010 (4B)
0000009 0x00000024  0x00012083  lw x1, 0(x2)              x1 <- 0x00000010, mem[0x000003fc] -> 0x00000010 (4B)
0000011 0x0000002c  0x00008067  jalr x0, 0(x1)            x0 write ignored
0000012 0x00000010  0x00000073  ecall                     HALT (ecall)
```

## Running the tests

```
build/tests/run_tests --out-dir out
```

```
Tests: 146/146 passed | Instruction coverage: 100.0% (40/40) | Functional coverage: 100.0%
```

Writes `out/test_results.txt`, `out/coverage_report.txt`, and
`out/coverage.csv`. Options: `--filter NAME` (substring match on test name),
`--seed N` / `--random-count N` (constrained-random tests), `--trace-all`
(write a `.trace` file per test), `--out-dir DIR`. Exits nonzero if any test
failed.

See `DESIGN.md` for the architecture, the verification strategy (directed +
program + constrained-random tests, and the coverage-closure pass that got
this from an initial 62.3% to 100% functional coverage), bugs found along
the way, and possible next steps.
