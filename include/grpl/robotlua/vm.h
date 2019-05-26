#pragma once

#include "bytecode.h"
#include "smallvector.h"

#include <functional>

namespace grpl {
namespace robotlua {
  /**
   * The Upval is a construct of Lua that allows for values to exist oustide of their
   * regular scope. Consider the case of the anonymous function:
   * 
   * function createFunc()
   *  local i = 0
   *  local anon = function()
   *    i = i + 1
   *    return i
   *  end
   *  anon()
   *  return anon
   * end
   * 
   * In this case, "i" would usually go out of scope upon the return of createFunc(), 
   * but since it is being used by the anonymous function, we can't allow that to happen.
   * "i" is, in a sense, made global until all instances of the anonymous function are
   * not used anymore. "i" is an upval.
   * 
   * Note that "i", while still an upval, exists in two states at different times during 
   * execution. Before the local "i" goes out of scope (i.e. createFunc returns), the "i"
   * value is shared between createFunc and the anonymous function, hence it is on the stack.
   * In this state, "i" is referred to as an "open upval".
   * 
   * When createFunc returns, the stack entry for createFunc and its variables is popped. In 
   * this case, "i" would go out of scope and would no longer be accessible to the anonymous 
   * function. To solve this, each time a function is returned, its upvals (if still in use)
   * are moved to a global upval table, outside of the stack entirely. In this case, "i" is 
   * classified as a "closed upval" (it holds its own data, instead of pointing to a value
   * already on the stack).
   */
  struct lua_upval {
    /**
     * size_t: Stack offset of upval when open
     * tvalue: Actual value of the upval when closed
     */
    simple_variant<size_t, tvalue> value;
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

  // Our equivilent of lua_state
  class vm {
   public:
    void load(bytecode_chunk &);
   private:
    /**
     * Find the next available 'slot' in a vector that contains single-element
     * variants.
     */
    template<typename T, size_t S>
    size_t first_avail_idx(small_vector<simple_variant<T>, S> &v) {
      for (size_t i = 0; i < v.size(); i++) {
        if (!v[i].is_assigned()) return i;
      }
      // Create a new slot if one doesn't exist.
      // Safe to emplace since we have to call another emplace on the actual 
      // variant.
      size_t slot = v.size();
      v.emplace_back();
      return slot;
    }

    small_vector<tvalue, 128> _registers;
    // Upval storage - used for variables that transcend the normal scope. 
    // e.g. local variables in ownership by an anonymous function
    small_vector<simple_variant<lua_upval>, 16> _upvals;
    // Root Prototype storage - used to store the 'root prototype' (function
    // definition) of loaded files. Referenced by lua_lclosure
    small_vector<simple_variant<bytecode_prototype>, 4> _rootprotos;
    // Store for objects
    small_vector<simple_variant<lua_object>, 32> _objects;

    /**
     * In Lua, all loaded files have a single upvalue - the _ENV (environment).
     * Unless otherwise specified, Lua sets _ENV to the 'distinguished environment'
     * (also called _G in legacy Lua). All variables, e.g. foo, in the loaded file 
     * are actually _ENV.foo.
     * 
     * See http://lua-users.org/lists/lua-l/2014-08/msg00345.html
     * 
     * For simplicity, we choose to always use the distinguished env as _ENV. Cases
     * where different _ENVs are required should be fulfilled with multiple instances
     * of vm.
     */
    tvalue _distinguished_env;
  };
}
}