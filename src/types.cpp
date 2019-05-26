#include "grpl/robotlua/types.h"

#include <new>

using namespace grpl::robotlua;

tvalue::tvalue() : tag_type(construct_tag_type(tag::NIL)) {}

tvalue::tvalue(bool b) : tag_type(construct_tag_type(tag::BOOL)) {
  // data.value_bool = b;
  data.emplace<bool>(b);
}

tvalue::tvalue(lua_integer i) : tag_type(construct_tag_type(tag::NUMBER, variant::NUM_INT)) {
  // data.value_int = i;
  data.emplace<lua_integer>(i);
}

tvalue::tvalue(lua_number n) : tag_type(construct_tag_type(tag::NUMBER, variant::NUM_FLOAT)) {
  // data.value_num = n;
  data.emplace<lua_number>(n);
}

tvalue::tvalue(uint8_t tagt) : tag_type(tagt) {
  tag t = get_tag_from_tag_type(tagt);
  variant v = get_variant_from_tag_type(tagt);
  if (t == tag::STRING) {
    data.emplace<string_vec>();
  } else if (t == tag::FUNC) {
    lua_closure &cl = data.emplace<lua_closure>();
    if (v == variant::FUNC_LUA) {
      // Lua Func
      cl.impl.emplace<lua_lclosure>();
    } else {
      // Native Func
      cl.impl.emplace<lua_native_closure>();
    }
  }
}