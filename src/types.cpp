#include "grpl/robotlua/types.h"

#include <new>

using namespace grpl::robotlua;

tvalue::tvalue() : tag_type(construct_tag_type(tag::NIL)) {}

tvalue::tvalue(bool b) : tag_type(construct_tag_type(tag::BOOL)) {
  data.emplace<bool>(b);
}

tvalue::tvalue(lua_integer i) : tag_type(construct_tag_type(tag::NUMBER, variant::NUM_INT)) {
  data.emplace<lua_integer>(i);
}

tvalue::tvalue(lua_number n) : tag_type(construct_tag_type(tag::NUMBER, variant::NUM_FLOAT)) {
  data.emplace<lua_number>(n);
}

tvalue::tvalue(uint8_t tagt) : tag_type(tagt) {
  tag t = get_tag_from_tag_type(tagt);
  variant v = get_variant_from_tag_type(tagt);
  if (t == tag::STRING) {
    data.emplace<string_vec>();
  }
}

tvalue::~tvalue() {
  
}

bool tvalue::operator==(const tvalue &other) const {
  if (tag_type != other.tag_type) return false;
  // nil has no data
  if (get_tag_from_tag_type(tag_type) != tag::NIL) {
    if (data.is<bool>())
      return data.get<bool>() == other.data.get<bool>();
    else if (data.is<lua_number>())
      return data.get<lua_number>() == other.data.get<lua_number>();
    else if (data.is<lua_integer>())
      return data.get<lua_integer>() == other.data.get<lua_integer>();
    else if (data.is<string_vec>()) {
      // Compare string values
      string_vec &a = data.get<string_vec>(), &b = other.data.get<string_vec>();
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); i++)
        if (a[i] != b[i])
          return false;
    } else {
      // Object
      return data.get<size_t>() == other.data.get<size_t>();
    }
  }

  return true;
}