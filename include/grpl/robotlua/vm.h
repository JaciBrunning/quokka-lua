#pragma once

#include "smallvector.h"
#include "types.h"
#include "opcodes.h"

namespace grpl {
namespace robotlua {
  class vm {
   public:
    
   private:
    // TODO: Tune this!
    small_vector<tvalue, 256> _stack;
  };
}
}