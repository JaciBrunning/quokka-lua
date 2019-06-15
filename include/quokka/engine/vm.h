#pragma once

#include "bytecode.h"
#include "smallvector.h"
#include "opcodes.h"

#include <functional>

namespace quokka {
namespace engine {

  #define CALL_STATUS_LUA (1 << 1)
  #define CALL_STATUS_FRESH (1 << 3)
  #define CALL_STATUS_TAIL (1 << 5)

  /**
   * Magic value for a nreturn with variable returns.
   */
  const int MULTIRET = -1;

  /**
   * Structure defining a call (stack frame) of a closure.
   */
  struct lua_call {
    // Index of the function in the stack
    size_t func_idx;
    // Function type specific info
    union {
      struct {
        size_t base;
        const lua_instruction *pc;
      } lua;
      struct { } native;
    } info;
    // How many results (return vals) from this function?
    int numresults = 0;
    unsigned callstatus = 0;
  };

  /**
   * The Quokka VM is the runtime of the Quokka Lua Engine. It is responsible
   * for interpreting bytecode instructions, and storing all data related to the
   * runtime. 
   */
  class quokka_vm {
   public:
    /**
     * Construct a Quokka VM without loading any bytecode.
     */
    quokka_vm();

    /**
     * Construct a Quokka VM and load a bytecode chunk (root prototype).
     * @param bc The bytecode to load
     */
    quokka_vm(bytecode_chunk &bc) : quokka_vm() {
      load(bc);
    }
    
    /**
     * Load some bytecode into the VM. This should only be done if using the default
     * constructor, or if a call to the root function has been completed.
     * @param bc The bytecode to load
     */
    void load(bytecode_chunk &bc);

    object_view alloc_object();
    upval_view alloc_upval();

    /**
     * Call a function on the stack.
     * 
     * In preparation for calling the function, the closure value shall
     * be pushed onto the stack (unless preceeded by load()).
     * Any required arguments should be pushed onto the stack using push().
     * After the function is called, its return values can be retrieved using pop().
     * 
     * @param nargs The number of arguments to the function. Default 0
     * @param nreturn The number of return values from the function. Default 0
     */
    void call(size_t nargs = 0, int nreturn = 0);

    /**
     * Gets an argument given to a native function.
     * @param id The id of the argument, indexed from 0
     * @return The value of the argument
     */
    lua_value &argument(int id);

    /**
     * Get the number of arguments provided to a native function.
     * @return The number of arguments (params)
     */
    int num_arguments();

    /**
     * Push a value onto the stack, in preparation for calling a function.
     * @param v The value to push onto the stack.
     */
    void push(const lua_value &v);

    /**
     * Push a value from the global environment onto the stack. Shorthand for
     * push(env().get(key)).
     * @param key The name of the global variable.
     */
    void push_global(const lua_value &key) {
      push(env().get(key));
    }

    /**
     * Pop a value off of the top of the stack, retrieving it's value. Most
     * commonly used for fetching return values. 
     * 
     * Note that, if used for getting return values, return values are popped
     * in reverse order.
     * 
     * @return The value at the top of the stack.
     */
    lua_value &pop();

    /**
     * Pop a number of arguments from the stack, disregarding their value, primarily
     * used for ignoring return values.
     * 
     * @param num The number of values to pop.
     */
    void pop(size_t num);

    /**
     * Get the 'distinguished environment', also known as the Global Table, where all
     * global variables are stored. The distinguished env is created automatically when
     * the VM is created, and this can be used for assigning or retrieving global variables.
     */
    lua_table &env();

    /**
     * Allocate a native function, ready to be put into the global env or other lua_value.
     * This is a quick way to define a new native function.
     */
    object_view alloc_native_function(lua_native_closure::func_t f);

    /**
     * Define a native function, placed into the global env. This is a shortcut
     * for env().set(key, alloc_native_function(f)).
     * 
     * @param key The key of the native function in the global env (see env())
     * @param f The native function, as a lambda, C function, or other std::function type.
     */
    void define_native_function(const lua_value &key, lua_native_closure::func_t f);

    inline void define_native_function(const char *key, lua_native_closure::func_t f) {
      define_native_function(lua_string{key}, f);
    }
    
   private:
    using call_ref = continuous_reference<lua_call>;
    using reg_ref = continuous_reference<lua_value>;

    // Return true if C function
    bool precall(size_t func_stack_idx, int nreturn);
    void execute();
    // Return false if multi results (variable number)
    bool postcall(size_t first_result_idx, int nreturn);

    void close_upvals(size_t level);
    object_view lclosure_cache(bytecode_prototype &proto, size_t func_base, object_view parent_cl);
    object_view lclosure_new(bytecode_prototype &proto, size_t func_base, object_view parent_cl);

    small_vector<lua_value, 48> _registers;
    small_vector<lua_call, 16> _callinfo;
    // Upval storage - used for variables that transcend the normal scope. 
    // e.g. local variables in ownership by an anonymous function
    small_vector<lua_upval, 2, 4> _upvals;
    // Store for objects
    small_vector<lua_object, 8> _objects;

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
    lua_value _distinguished_env;

    /* Instruction and Call info */
    lua_instruction _instruction;
    opcode _code;
    uint8_t _arg_a;
    unsigned int _arg_b, _arg_c;
    size_t _ra;

    call_ref _ci_ref;
    object_view _cl_ref;
    size_t _base;
  };
}
}