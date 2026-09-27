// Constrained-random testing: generate short random sequences of ALU and
// load/store instructions (deliberately no control flow, so there's no
// question of what "should" happen next) and check the real CPU against a
// second, independently written reference model after every single
// instruction. If they ever disagree, the seed is printed so the failure
// reproduces exactly.
//
// The reference model is *not* allowed to reuse CPU.cpp's execute logic --
// the whole point is that a bug in one implementation's understanding of
// the ISA has to also appear, independently, in the other to slip through.
#include "TestRegistry.h"

#include <array>
#include <random>
#include <sstream>
#include <string>

#include "Assembler.h"
#include "CPU.h"
#include "Memory.h"

namespace {

enum class RKind {
    ADD, SUB, AND, OR, XOR, SLT, SLTU, SLL, SRL, SRA,
    ADDI, ANDI, ORI, XORI, SLTI, SLTIU, SLLI, SRLI, SRAI,
    LW, LB, LH, LBU, LHU,
    SB, SH, SW,
};

const char* kindName(RKind k) {
    switch (k) {
        case RKind::ADD: return "add";     case RKind::SUB: return "sub";
        case RKind::AND: return "and";     case RKind::OR: return "or";
        case RKind::XOR: return "xor";     case RKind::SLT: return "slt";
        case RKind::SLTU: return "sltu";   case RKind::SLL: return "sll";
        case RKind::SRL: return "srl";     case RKind::SRA: return "sra";
        case RKind::ADDI: return "addi";   case RKind::ANDI: return "andi";
        case RKind::ORI: return "ori";     case RKind::XORI: return "xori";
        case RKind::SLTI: return "slti";   case RKind::SLTIU: return "sltiu";
        case RKind::SLLI: return "slli";   case RKind::SRLI: return "srli";
        case RKind::SRAI: return "srai";   case RKind::LW: return "lw";
        case RKind::LB: return "lb";       case RKind::LH: return "lh";
        case RKind::LBU: return "lbu";     case RKind::LHU: return "lhu";
        case RKind::SB: return "sb";       case RKind::SH: return "sh";
        case RKind::SW: return "sw";
    }
    return "?";
}

struct GenInstr {
    RKind kind{};
    uint32_t rd = 0, rs1 = 0, rs2 = 0;
    int32_t imm = 0; // doubles as shamt (shift-immediate) or offset (load/store)
};

uint32_t encode(const GenInstr& g) {
    using namespace asmb;
    switch (g.kind) {
        case RKind::ADD:  return ADD(g.rd, g.rs1, g.rs2);
        case RKind::SUB:  return SUB(g.rd, g.rs1, g.rs2);
        case RKind::AND:  return AND(g.rd, g.rs1, g.rs2);
        case RKind::OR:   return OR(g.rd, g.rs1, g.rs2);
        case RKind::XOR:  return XOR(g.rd, g.rs1, g.rs2);
        case RKind::SLT:  return SLT(g.rd, g.rs1, g.rs2);
        case RKind::SLTU: return SLTU(g.rd, g.rs1, g.rs2);
        case RKind::SLL:  return SLL(g.rd, g.rs1, g.rs2);
        case RKind::SRL:  return SRL(g.rd, g.rs1, g.rs2);
        case RKind::SRA:  return SRA(g.rd, g.rs1, g.rs2);
        case RKind::ADDI:  return ADDI(g.rd, g.rs1, g.imm);
        case RKind::ANDI:  return ANDI(g.rd, g.rs1, g.imm);
        case RKind::ORI:   return ORI(g.rd, g.rs1, g.imm);
        case RKind::XORI:  return XORI(g.rd, g.rs1, g.imm);
        case RKind::SLTI:  return SLTI(g.rd, g.rs1, g.imm);
        case RKind::SLTIU: return SLTIU(g.rd, g.rs1, g.imm);
        case RKind::SLLI:  return SLLI(g.rd, g.rs1, static_cast<uint32_t>(g.imm));
        case RKind::SRLI:  return SRLI(g.rd, g.rs1, static_cast<uint32_t>(g.imm));
        case RKind::SRAI:  return SRAI(g.rd, g.rs1, static_cast<uint32_t>(g.imm));
        case RKind::LW:  return LW(g.rd, g.imm, g.rs1);
        case RKind::LB:  return LB(g.rd, g.imm, g.rs1);
        case RKind::LH:  return LH(g.rd, g.imm, g.rs1);
        case RKind::LBU: return LBU(g.rd, g.imm, g.rs1);
        case RKind::LHU: return LHU(g.rd, g.imm, g.rs1);
        case RKind::SB: return SB(g.rs2, g.imm, g.rs1);
        case RKind::SH: return SH(g.rs2, g.imm, g.rs1);
        case RKind::SW: return SW(g.rs2, g.imm, g.rs1);
    }
    return 0;
}

// Independent reference model: same ISA semantics, written from scratch,
// operating on its own register/memory arrays with no shared code path
// through CPU.cpp or Decoder.cpp.
class RefMachine {
public:
    explicit RefMachine(size_t memSize) : mem_(memSize, 0) {}

    uint32_t reg(uint32_t i) const { return regs_[i & 31]; }
    uint8_t byte(uint32_t addr) const { return mem_[addr]; }

    void pokeReg(uint32_t i, uint32_t v) { regs_[i & 31] = v; }

    void exec(const GenInstr& g) {
        uint32_t a = reg(g.rs1);
        uint32_t b = reg(g.rs2);
        uint32_t imm = static_cast<uint32_t>(g.imm);

        switch (g.kind) {
            case RKind::ADD:  setReg(g.rd, a + b); break;
            case RKind::SUB:  setReg(g.rd, a - b); break;
            case RKind::AND:  setReg(g.rd, a & b); break;
            case RKind::OR:   setReg(g.rd, a | b); break;
            case RKind::XOR:  setReg(g.rd, a ^ b); break;
            case RKind::SLT:  setReg(g.rd, (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1u : 0u); break;
            case RKind::SLTU: setReg(g.rd, (a < b) ? 1u : 0u); break;
            case RKind::SLL:  setReg(g.rd, a << (b & 0x1F)); break;
            case RKind::SRL:  setReg(g.rd, a >> (b & 0x1F)); break;
            case RKind::SRA:  setReg(g.rd, static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1F))); break;

            case RKind::ADDI:  setReg(g.rd, a + imm); break;
            case RKind::ANDI:  setReg(g.rd, a & imm); break;
            case RKind::ORI:   setReg(g.rd, a | imm); break;
            case RKind::XORI:  setReg(g.rd, a ^ imm); break;
            case RKind::SLTI:  setReg(g.rd, (static_cast<int32_t>(a) < g.imm) ? 1u : 0u); break;
            case RKind::SLTIU: setReg(g.rd, (a < imm) ? 1u : 0u); break;
            case RKind::SLLI:  setReg(g.rd, a << (imm & 0x1F)); break;
            case RKind::SRLI:  setReg(g.rd, a >> (imm & 0x1F)); break;
            case RKind::SRAI:  setReg(g.rd, static_cast<uint32_t>(static_cast<int32_t>(a) >> (imm & 0x1F))); break;

            case RKind::LW: setReg(g.rd, loadWord(a + imm)); break;
            case RKind::LB: setReg(g.rd, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem_[a + imm])))); break;
            case RKind::LBU: setReg(g.rd, mem_[a + imm]); break;
            case RKind::LH: setReg(g.rd, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(loadHalf(a + imm))))); break;
            case RKind::LHU: setReg(g.rd, loadHalf(a + imm)); break;

            case RKind::SB: mem_[a + imm] = static_cast<uint8_t>(b); break;
            case RKind::SH: storeHalf(a + imm, static_cast<uint16_t>(b)); break;
            case RKind::SW: storeWord(a + imm, b); break;
        }
    }

private:
    void setReg(uint32_t i, uint32_t v) { if ((i & 31) != 0) regs_[i & 31] = v; }

    uint16_t loadHalf(uint32_t addr) const {
        return static_cast<uint16_t>(mem_[addr]) | (static_cast<uint16_t>(mem_[addr + 1]) << 8);
    }
    uint32_t loadWord(uint32_t addr) const {
        return static_cast<uint32_t>(mem_[addr]) | (static_cast<uint32_t>(mem_[addr + 1]) << 8) |
               (static_cast<uint32_t>(mem_[addr + 2]) << 16) | (static_cast<uint32_t>(mem_[addr + 3]) << 24);
    }
    void storeHalf(uint32_t addr, uint16_t v) {
        mem_[addr] = static_cast<uint8_t>(v & 0xFF);
        mem_[addr + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }
    void storeWord(uint32_t addr, uint32_t v) {
        mem_[addr] = static_cast<uint8_t>(v & 0xFF);
        mem_[addr + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        mem_[addr + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        mem_[addr + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }

    std::array<uint32_t, 32> regs_{};
    std::vector<uint8_t> mem_;
};

// Registers 0..10 are free for the generator to use as ALU operands/results
// (x0 included, on purpose -- it exercises the read-zero/write-ignored
// paths). Register 20 is reserved as the load/store base pointer and is
// never touched by generated ALU instructions, so addresses stay
// predictable no matter what the ALU ops do.
constexpr uint32_t kPtrReg = 20;
constexpr uint32_t kPtrBase = 0x1000;
constexpr uint32_t kMemRegionBytes = 64; // [kPtrBase, kPtrBase + 64) is fair game
constexpr uint32_t kMemSize = kPtrBase + 256;

GenInstr generateOne(std::mt19937& rng) {
    static const RKind aluR[] = {RKind::ADD, RKind::SUB, RKind::AND, RKind::OR, RKind::XOR,
                                 RKind::SLT, RKind::SLTU, RKind::SLL, RKind::SRL, RKind::SRA};
    static const RKind aluI[] = {RKind::ADDI, RKind::ANDI, RKind::ORI, RKind::XORI, RKind::SLTI, RKind::SLTIU};
    static const RKind shiftI[] = {RKind::SLLI, RKind::SRLI, RKind::SRAI};
    static const RKind loads[] = {RKind::LW, RKind::LB, RKind::LH, RKind::LBU, RKind::LHU};
    static const RKind stores[] = {RKind::SB, RKind::SH, RKind::SW};

    std::uniform_int_distribution<uint32_t> reg(0, 31); // full register file, for read-side use
    std::uniform_int_distribution<int> category(0, 4);
    std::uniform_int_distribution<int32_t> imm12(-2048, 2047);
    std::uniform_int_distribution<uint32_t> shamt(0, 31);
    std::uniform_int_distribution<int32_t> offset(0, 60); // leaves room for a 4-byte access

    // Any register is fair game to *read*, including the pointer register
    // (kPtrReg) -- that just means an ALU op reads the current base
    // address, which is harmless. Writing to it would corrupt every load/
    // store address for the rest of the case, though, so destination
    // registers get redirected to x0 if they land on kPtrReg.
    auto destReg = [&]() {
        uint32_t r = reg(rng);
        return (r == kPtrReg) ? 0u : r;
    };

    GenInstr g;
    switch (category(rng)) {
        case 0:
            g.kind = aluR[std::uniform_int_distribution<size_t>(0, 9)(rng)];
            g.rd = destReg(); g.rs1 = reg(rng); g.rs2 = reg(rng);
            break;
        case 1:
            g.kind = aluI[std::uniform_int_distribution<size_t>(0, 5)(rng)];
            g.rd = destReg(); g.rs1 = reg(rng); g.imm = imm12(rng);
            break;
        case 2:
            g.kind = shiftI[std::uniform_int_distribution<size_t>(0, 2)(rng)];
            g.rd = destReg(); g.rs1 = reg(rng); g.imm = static_cast<int32_t>(shamt(rng));
            break;
        case 3:
            g.kind = loads[std::uniform_int_distribution<size_t>(0, 4)(rng)];
            g.rd = destReg(); g.rs1 = kPtrReg; g.imm = offset(rng);
            break;
        default:
            g.kind = stores[std::uniform_int_distribution<size_t>(0, 2)(rng)];
            g.rs2 = reg(rng); g.rs1 = kPtrReg; g.imm = offset(rng);
            break;
    }
    return g;
}

std::string hex32(uint32_t v) {
    std::ostringstream out;
    out << "0x" << std::hex << v;
    return out.str();
}

constexpr int kInstrPerCase = 25;

TestResult runOneRandomCase(uint32_t seed, uint32_t caseIdx, Coverage& coverage) {
    TestResult result;
    result.name = "random_" + std::to_string(caseIdx) + "_seed" + std::to_string(seed);

    std::mt19937 rng(seed);
    std::vector<GenInstr> instrs;
    instrs.reserve(kInstrPerCase);
    for (int i = 0; i < kInstrPerCase; ++i) instrs.push_back(generateOne(rng));

    RefMachine ref(kMemSize);
    ref.pokeReg(kPtrReg, kPtrBase);

    Memory mem(kMemSize);
    std::vector<uint32_t> program;
    program.reserve(instrs.size() + 1);
    for (const auto& g : instrs) program.push_back(encode(g));
    program.push_back(asmb::ECALL());
    for (size_t i = 0; i < program.size(); ++i) {
        mem.storeWord(static_cast<uint32_t>(i * 4), program[i]);
    }

    CPU cpu(mem, 0, 10000);
    cpu.pokeRegister(kPtrReg, kPtrBase);

    bool ok = true;
    for (size_t i = 0; i < instrs.size() && ok; ++i) {
        RetireRecord rec = cpu.step();
        coverage.record(rec);
        if (rec.decoded.valid) result.instructionsExecuted.insert(rec.decoded.name);
        ref.exec(instrs[i]);

        for (uint32_t r = 0; r < 32 && ok; ++r) {
            uint32_t actual = cpu.regs().read(r);
            uint32_t expected = ref.reg(r);
            if (actual != expected) {
                ok = false;
                result.failures.push_back("instr #" + std::to_string(i) + " (" + kindName(instrs[i].kind) +
                                           "): x" + std::to_string(r) + " expected " + hex32(expected) +
                                           " got " + hex32(actual) + " [seed=" + std::to_string(seed) + "]");
            }
        }
        for (uint32_t off = 0; off < kMemRegionBytes && ok; ++off) {
            uint32_t addr = kPtrBase + off;
            uint8_t actual = mem.loadByte(addr);
            uint8_t expected = ref.byte(addr);
            if (actual != expected) {
                ok = false;
                result.failures.push_back("instr #" + std::to_string(i) + " (" + kindName(instrs[i].kind) +
                                           "): mem[" + hex32(addr) + "] expected " + hex32(expected) +
                                           " got " + hex32(actual) + " [seed=" + std::to_string(seed) + "]");
            }
        }
    }

    // Drain the trailing ECALL so the CPU ends in a clean halted state.
    while (!cpu.halted()) {
        RetireRecord rec = cpu.step();
        coverage.record(rec);
    }

    result.cycles = cpu.cycles();
    result.passed = ok;
    return result;
}

} // namespace

std::vector<TestResult> runRandomTests(Coverage& coverage, uint32_t seed, uint32_t count) {
    std::vector<TestResult> results;
    results.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        results.push_back(runOneRandomCase(seed + i, i, coverage));
    }
    return results;
}
