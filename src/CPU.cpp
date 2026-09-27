#include "CPU.h"

#include "Decoder.h"

CPU::CPU(Memory& mem, uint32_t resetPC, uint32_t maxCycles)
    : mem_(mem), pc_(resetPC), maxCycles_(maxCycles) {}

void CPU::halt(RetireRecord& rec, const std::string& reason) {
    halted_ = true;
    haltReason_ = reason;
    rec.halted = true;
    rec.haltReason = reason;
}

RetireRecord CPU::step() {
    if (halted_) {
        RetireRecord rec;
        rec.cycle = cycle_;
        rec.pc = pc_;
        rec.halted = true;
        rec.haltReason = haltReason_;
        return rec;
    }

    if (cycle_ + 1 > maxCycles_) {
        RetireRecord rec;
        rec.cycle = cycle_ + 1;
        rec.pc = pc_;
        halt(rec, "max-cycle timeout");
        cycle_ = rec.cycle;
        return rec;
    }

    return executeAt(pc_);
}

std::vector<RetireRecord> CPU::run() {
    std::vector<RetireRecord> records;
    while (!halted_) {
        records.push_back(step());
    }
    return records;
}

RetireRecord CPU::executeAt(uint32_t instrPC) {
    RetireRecord rec;
    rec.cycle = cycle_ + 1;
    rec.pc = instrPC;

    if (instrPC % 4 != 0) {
        halt(rec, "misaligned instruction fetch");
        cycle_ = rec.cycle;
        return rec;
    }

    Word raw;
    try {
        raw = mem_.loadWord(instrPC);
    } catch (const MemoryAccessError&) {
        halt(rec, "out-of-bounds memory access");
        cycle_ = rec.cycle;
        return rec;
    }

    const DecodedInstr d = Decoder::decode(raw);
    rec.decoded = d;

    if (!d.valid) {
        halt(rec, "illegal instruction");
        cycle_ = rec.cycle;
        return rec;
    }

    uint32_t nextPC = instrPC + 4;
    const uint32_t rs1v = regs_.read(d.rs1);
    const uint32_t rs2v = regs_.read(d.rs2);
    const uint32_t immU = static_cast<uint32_t>(d.imm);
    rec.rs1Value = rs1v;
    rec.rs2Value = rs2v;

    auto writeRd = [&](uint32_t value) {
        rec.regWriteAttempted = true;
        rec.regWriteIndex = d.rd;
        rec.regWriteValue = value;
        rec.regWriteWasIgnored = RegisterFile::isZeroRegister(d.rd);
        regs_.write(d.rd, value);
    };

    try {
        switch (d.name) {
            case InstrName::LUI:
                writeRd(immU);
                break;
            case InstrName::AUIPC:
                writeRd(instrPC + immU);
                break;

            case InstrName::JAL: {
                uint32_t target = instrPC + immU;
                writeRd(instrPC + 4);
                nextPC = target;
                break;
            }
            case InstrName::JALR: {
                // rs1 is read into rs1v above, before writeRd() touches rd --
                // matters when rd == rs1 (e.g. "jalr x1, 0(x1)").
                uint32_t target = (rs1v + immU) & ~1u;
                writeRd(instrPC + 4);
                nextPC = target;
                break;
            }

            case InstrName::BEQ:
            case InstrName::BNE:
            case InstrName::BLT:
            case InstrName::BGE:
            case InstrName::BLTU:
            case InstrName::BGEU: {
                bool taken = false;
                switch (d.name) {
                    case InstrName::BEQ:  taken = (rs1v == rs2v); break;
                    case InstrName::BNE:  taken = (rs1v != rs2v); break;
                    case InstrName::BLT:  taken = (static_cast<int32_t>(rs1v) < static_cast<int32_t>(rs2v)); break;
                    case InstrName::BGE:  taken = (static_cast<int32_t>(rs1v) >= static_cast<int32_t>(rs2v)); break;
                    case InstrName::BLTU: taken = (rs1v < rs2v); break;
                    case InstrName::BGEU: taken = (rs1v >= rs2v); break;
                    default: break;
                }
                rec.isBranch = true;
                rec.branchTaken = taken;
                rec.branchTarget = instrPC + immU;
                if (taken) nextPC = rec.branchTarget;
                break;
            }

            case InstrName::LB:
            case InstrName::LH:
            case InstrName::LW:
            case InstrName::LBU:
            case InstrName::LHU: {
                uint32_t addr = rs1v + immU;
                rec.memRead = true;
                rec.memAddr = addr;
                uint32_t value = 0;
                switch (d.name) {
                    case InstrName::LB:
                        value = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem_.loadByte(addr))));
                        rec.memWidth = 1;
                        break;
                    case InstrName::LBU:
                        value = mem_.loadByte(addr);
                        rec.memWidth = 1;
                        break;
                    case InstrName::LH:
                        value = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(mem_.loadHalf(addr))));
                        rec.memWidth = 2;
                        break;
                    case InstrName::LHU:
                        value = mem_.loadHalf(addr);
                        rec.memWidth = 2;
                        break;
                    case InstrName::LW:
                        value = mem_.loadWord(addr);
                        rec.memWidth = 4;
                        break;
                    default:
                        break;
                }
                rec.memValue = value;
                writeRd(value);
                break;
            }

            case InstrName::SB:
            case InstrName::SH:
            case InstrName::SW: {
                uint32_t addr = rs1v + immU;
                rec.memWrite = true;
                rec.memAddr = addr;
                rec.memValue = rs2v;
                switch (d.name) {
                    case InstrName::SB: mem_.storeByte(addr, static_cast<uint8_t>(rs2v)); rec.memWidth = 1; break;
                    case InstrName::SH: mem_.storeHalf(addr, static_cast<uint16_t>(rs2v)); rec.memWidth = 2; break;
                    case InstrName::SW: mem_.storeWord(addr, rs2v); rec.memWidth = 4; break;
                    default: break;
                }
                break;
            }

            case InstrName::ADDI:  writeRd(rs1v + immU); break;
            case InstrName::SLTI:  writeRd((static_cast<int32_t>(rs1v) < d.imm) ? 1u : 0u); break;
            // SLTIU: the immediate is sign-extended first, then the compare
            // itself is unsigned -- so e.g. "sltiu x1, x0, -1" compares 0
            // against 0xFFFFFFFF and is true, even though the immediate was
            // written as a small negative number.
            case InstrName::SLTIU: writeRd((rs1v < immU) ? 1u : 0u); break;
            case InstrName::XORI:  writeRd(rs1v ^ immU); break;
            case InstrName::ORI:   writeRd(rs1v | immU); break;
            case InstrName::ANDI:  writeRd(rs1v & immU); break;
            case InstrName::SLLI:  writeRd(rs1v << d.shamt); break;
            case InstrName::SRLI:  writeRd(rs1v >> d.shamt); break;
            case InstrName::SRAI:  writeRd(static_cast<uint32_t>(static_cast<int32_t>(rs1v) >> d.shamt)); break;

            case InstrName::ADD:  writeRd(rs1v + rs2v); break;
            case InstrName::SUB:  writeRd(rs1v - rs2v); break;
            case InstrName::SLL:  writeRd(rs1v << (rs2v & 0x1F)); break;
            case InstrName::SLT:  writeRd((static_cast<int32_t>(rs1v) < static_cast<int32_t>(rs2v)) ? 1u : 0u); break;
            case InstrName::SLTU: writeRd((rs1v < rs2v) ? 1u : 0u); break;
            case InstrName::XOR:  writeRd(rs1v ^ rs2v); break;
            // SRL/SRA (and SRLI/SRAI/SRA above) rely on right-shift of a
            // signed int being arithmetic. That's implementation-defined
            // pre-C++20, but MSVC/GCC/Clang all do the sane thing on x86;
            // documented as a known portability assumption in DESIGN.md.
            case InstrName::SRL:  writeRd(rs1v >> (rs2v & 0x1F)); break;
            case InstrName::SRA:  writeRd(static_cast<uint32_t>(static_cast<int32_t>(rs1v) >> (rs2v & 0x1F))); break;
            case InstrName::OR:   writeRd(rs1v | rs2v); break;
            case InstrName::AND:  writeRd(rs1v & rs2v); break;

            case InstrName::FENCE:
                break; // no-op: single-hart in-order model, nothing to order

            case InstrName::ECALL:
                halt(rec, "ecall");
                break;
            case InstrName::EBREAK:
                halt(rec, "ebreak");
                break;

            case InstrName::ILLEGAL:
                // Unreachable: d.valid was already checked above, and
                // Decoder never sets name = ILLEGAL with valid = true.
                halt(rec, "illegal instruction");
                break;
        }
    } catch (const MemoryAccessError&) {
        halt(rec, "out-of-bounds memory access");
        cycle_ = rec.cycle;
        return rec;
    }

    cycle_ = rec.cycle;
    if (!rec.halted) {
        pc_ = nextPC;
    }
    return rec;
}
