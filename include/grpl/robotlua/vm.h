#pragma once

#include "bytecode.h"
#include "smallvector.h"

#include <functional>

namespace grpl {
namespace robotlua {
  struct lua_upval {
    // The actual value of the upval
    tvalue val;
    // The stack offset of the creator of the upvalue (the parent to the user),
    // used to index which upval we're actually referring to.
    size_t level;
    // Refcount for the upval, so we can dealloc when it's been used for the last time
    size_t refcount;
  };

  struct lua_call {
    // Index of the function in the stack
    size_t func_idx;
    // Index of the top of this function in the stack
    size_t top;
    // Function type specific info
    union {
      struct {
        size_t base;
        const lua_instruction *pc;
      } lua;
      struct { } native;
    } info;
    // How many results (return vals) from this function?
    int numresults;
  };

  class globalenv {

  };

  // Our equivilent of lua_state
  class vm {
   public:
    void load(bytecode_chunk &);
   private:
    small_vector<tvalue, 128> _registers;
    // Upval storage - used for variables that transcend the normal scope. 
    // e.g. local variables in ownership by an anonymous function
    small_vector<slot<lua_upval>, 16> _upvals;
    // Root Prototype storage - used to store the 'root prototype' (function
    // definition) of loaded files. Referenced by lua_lclosure
    small_vector<slot<bytecode_prototype>, 4> _rootprotos;
  };
}
}