// Turns a DecodedInstr into the text shown in trace files and CLI dumps,
// e.g. "addi  x5, x0, -12" or "sw    x1, 0(x0)".
#pragma once

#include <string>

#include "Types.h"

std::string disassemble(const DecodedInstr& d);
