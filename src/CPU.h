// The CPU model itself: fetch -> decode -> execute -> writeback -> PC update,
// one instruction per step(). Deliberately knows nothing about tracing or
// coverage -- each step() just returns a RetireRecord describing exactly
// what happened, and Tracer/Coverage are free functions of that record. That
// keeps the architectural model (the thing you'd actually compare against a
// golden reference or RTL) independent of anything to do with reporting.
#pragma once

#include <string>
#include <vector>

#include "Memory.h"
#include "RegisterFile.h"
#include "Types.h"

// Everything an observer (trace file, coverage model, test harness) needs to
// know about one retired instruction.
struct RetireRecord {
    uint32_t cycle = 0; // 1-based: the Nth instruction retired
    uint32_t pc = 0;    // address the instruction was fetched from
    DecodedInstr decoded;

    // Values actually read from rs1/rs2 this step (0 if the format doesn't
    // use that field -- see Coverage.cpp's usesRs1/usesRs2 for which do).
    // Kept here so Coverage can classify operand values without needing its
    // own access to the register file.
    uint32_t rs1Value = 0;
    uint32_t rs2Value = 0;

    bool regWriteAttempted = false; // an instruction with a destination register executed at all
    uint32_t regWriteIndex = 0;
    uint32_t regWriteValue = 0;
    bool regWriteWasIgnored = false; // true if the destination was x0

    bool memRead = false;
    bool memWrite = false;
    uint32_t memAddr = 0;
    uint32_t memValue = 0;
    uint32_t memWidth = 0; // bytes: 1, 2, or 4

    bool isBranch = false;   // BEQ/BNE/BLT/BGE/BLTU/BGEU
    bool branchTaken = false;
    uint32_t branchTarget = 0;

    bool halted = false;
    std::string haltReason; // empty unless `halted` is true
};

class CPU {
public:
    // `mem` must outlive the CPU; it's shared with a Memory instance owned
    // by whoever set up the run (main.cpp or a test).
    explicit CPU(Memory& mem, uint32_t resetPC = 0, uint32_t maxCycles = 100000);

    // Retires one instruction and returns its record. If the CPU is already
    // halted, this is a no-op that just returns a record with halted=true
    // and the original halt reason again -- callers should check halted()
    // before looping, but this makes it harmless if they don't.
    RetireRecord step();

    // Runs step() until halted() (illegal instruction, ecall/ebreak, memory
    // fault, or the max-cycle timeout), collecting every retire record.
    // Fine for the small test programs and sample binaries this simulator
    // targets; not meant for million-instruction workloads.
    std::vector<RetireRecord> run();

    bool halted() const { return halted_; }
    const std::string& haltReason() const { return haltReason_; }
    uint32_t cycles() const { return cycle_; }
    uint32_t pc() const { return pc_; }
    const RegisterFile& regs() const { return regs_; }

    // Test-setup backdoor: writes a register directly, bypassing the
    // pipeline entirely. Real programs never do this -- it's here so tests
    // can put a specific value in a register before running a one- or
    // two-instruction program that reads it.
    void pokeRegister(uint32_t index, uint32_t value) { regs_.write(index, value); }

private:
    RetireRecord executeAt(uint32_t instrPC);
    void halt(RetireRecord& rec, const std::string& reason);

    Memory& mem_;
    RegisterFile regs_;
    uint32_t pc_;
    uint32_t cycle_ = 0;
    uint32_t maxCycles_;
    bool halted_ = false;
    std::string haltReason_;
};
