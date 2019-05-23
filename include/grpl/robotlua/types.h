#pragma once

#include <stdint.h>

#include "smallvector.h"

namespace grpl {
namespace robotlua {
// We store Instructions and Integers as the value of size_t - 32 on 32-bit platforms and 64 on 64-bit platforms.
typedef size_t lua_instruction;
typedef int lua_integer;
typedef double lua_number;   // TODO: Add define arg for float?

enum class tag {
  NIL = 0,
  BOOL = 1,
  LIGHT_USER_DATA = 2,
  NUMBER = 3,
  STRING = 4,
  TABLE = 5,
  FUNC = 6,
  USER_DATA = 7,
  THREAD = 8
};

enum class variant {
  NONE = 0,
  NUM_FLOAT = 0, NUM_INT = 1,
  STR_SHORT = 0, STR_LONG = 1,
  FUNC_LUA = 0, FUNC_LIGHT_C = 1, FUNC_C = 2
};

uint8_t construct_tag_type(tag t, variant v) {
  return (uint8_t)t | ((uint8_t)v << 4);
}

tag get_tag_from_tag_type(uint8_t tag_type) {
  return (tag)(tag_type & 0x0F);
}

struct tvalue {
  uint8_t tag_type;
  union {
    bool value_bool;
    lua_number value_num;
    lua_integer value_int;
    small_vector<char, 32> value_str;  // TODO: small_string
  } data = {false};
};
}
}