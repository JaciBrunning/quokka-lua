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

  struct tvalue {
    using string_vec = small_vector<char, 32>;

    uint8_t tag_type;
    simple_variant<bool, lua_number, lua_integer, string_vec, lua_closure> data;

    tvalue();             // Nil
    tvalue(bool);         // Bool
    tvalue(lua_integer);  // Int
    tvalue(lua_number);   // Float

    tvalue(uint8_t tagt);  // String, Func, Table, Userdata
  };

}  // namespace robotlua
}  // namespace grpl