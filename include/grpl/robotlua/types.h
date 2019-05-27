#pragma once

#include <stdint.h>
#include <functional>

#include "smallvector.h"
#include "simplevariant.h"

namespace grpl {
namespace robotlua {
  // We store Instructions and Integers as the value of size_t - 32 on 32-bit platforms and 64 on 64-bit
  // platforms.
  typedef size_t lua_instruction;
  typedef int    lua_integer;
  typedef double lua_number;  // TODO: Add define arg for float?

  enum class tag {
    NIL             = 0,
    BOOL            = 1,
    LIGHT_USER_DATA = 2,
    NUMBER          = 3,
    STRING          = 4,
    TABLE           = 5,
    FUNC            = 6,
    USER_DATA       = 7,
    THREAD          = 8
  };

  enum class variant {
    NONE         = 0,
    /* Number */
    NUM_FLOAT    = 0,
    NUM_INT      = 1,
    /* String */
    STR_SHORT    = 0,
    STR_LONG     = 1,  // NOTE: RobotLua does not differentiate between Long and Short strings.
    /* Function */
    FUNC_LUA     = 0,
    FUNC_LIGHT_C = 1,
    FUNC_C       = 2
  };

  struct lua_lclosure {
    size_t proto_idx;
    small_vector<size_t, 4> upval_refs;
  };

  struct lua_native_closure {
    std::function<void()> func;
  };

  struct lua_closure {
    simple_variant<lua_lclosure, lua_native_closure> impl;
  };

  inline uint8_t construct_tag_type(tag t, variant v = variant::NONE) {
    return (uint8_t)t | ((uint8_t)v << 4);
  }

  inline tag get_tag_from_tag_type(uint8_t tag_type) { return (tag)(tag_type & 0x0F); }
  inline variant get_variant_from_tag_type(uint8_t tag_type) {
    return (variant)(tag_type >> 4);
  }

  // TODO: As keys, integer, number, boolean, and string, all work based
  // on equality
  struct lua_table {
    struct node {
      // Pointer to the global table store
      size_t key_ref;
      // Pointer to the global table store
      size_t value_ref;
    };
    small_vector<node, 16> entries;

    // template<typename STORAGE>
    // void set(STORAGE store, tvalue *key, tvalue *value) {
      
    // }
  };

  /**
   * Lua objects are datatypes that are described by more than just their value. Unline
   * numbers, strings, and booleans, objects can be complex, such as Tables. 
   * 
   * In RobotLua, objects are allocated into one large pool (analogous to the heap),
   * and automatically dealloced when their usages reach zero. Note that objects are distinct
   * to upvalues, as objects do not (on their own) go above their own scope unless they are
   * used in an upvalue. 
   * 
   * A value may hold an object, but an object is not a value.
   */
  struct lua_object {
    uint8_t tag_type;
    simple_variant<lua_table, lua_closure> data;
    size_t refcount;

    lua_object(uint8_t tagt);
  };

  struct tvalue {
    using string_vec = small_vector<char, 32>;

    uint8_t tag_type;
    /**
     * no type: Nil
     * bool: Boolean
     * lua_number: Number (float)
     * lua_integer: Number (integer)
     * string_vec: String
     * size_t: Index in object store
     */
    simple_variant<bool, lua_number, lua_integer, string_vec, size_t> data;

    tvalue();             // Nil
    tvalue(bool);         // Bool
    tvalue(lua_integer);  // Int
    tvalue(lua_number);   // Float

    tvalue(uint8_t tagt);  // String, object

    ~tvalue();

    void set_object(size_t position, lua_object &obj);
    bool operator==(const tvalue &) const;
  };

}  // namespace robotlua
}  // namespace grpl