// Turns a raw 32-bit instruction word into a DecodedInstr: which format it
// is, which fields it uses, and the sign-extended immediate. Pure function
// of the instruction bits -- no simulator state involved.
#pragma once

#include "Types.h"

class Decoder {
public:
    static DecodedInstr decode(Word instr);
};
