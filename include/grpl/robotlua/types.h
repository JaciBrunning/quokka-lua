#pragma once

#include <stdint.h>

#include "smallvector.h"

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

  inline uint8_t construct_tag_type(tag t, variant v = variant::NONE) {
    return (uint8_t)t | ((uint8_t)v << 4);
  }

  inline tag get_tag_from_tag_type(uint8_t tag_type) { return (tag)(tag_type & 0x0F); }

  struct tvalue {
    using string_vec = small_vector<char, 32>;

    uint8_t tag_type;
    // TODO: Userdata
    union {
      bool        value_bool;
      lua_number  value_num;
      lua_integer value_int;
      // Is actually of type small_vec, but needs to be a buffer so we can use placement new
      char value_str_buf[sizeof(string_vec)];
    } data;

    tvalue();             // Nil
    tvalue(bool);         // Bool
    tvalue(lua_integer);  // Int
    tvalue(lua_number);   // Float

    tvalue(uint8_t tagt);  // String, Func, Table, Userdata

    ~tvalue();

    small_vector<char, 32> *value_string();
  };
}  // namespace robotlua
}  // namespace grpl