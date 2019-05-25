#pragma once

#include "smallvector.h"
#include "types.h"
#include "opcodes.h"
#include "bytecode.h"

namespace grpl {
namespace robotlua {
  using stack_value = tvalue;
  using stack_offset = size_t;

  // A single entry on the call stack - a function call
  struct call_info {
    
  };

  class vm {
   public:
    vm();

    void load(bytecode_chunk &bytecode);
    
    void execute(lua_instruction instruction);
   private:
    // TODO: Tune this!
    small_vector<stack_value, 128> _registers;
  };
}
}