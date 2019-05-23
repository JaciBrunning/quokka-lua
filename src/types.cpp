#include "grpl/robotlua/types.h"

#include <new>

using namespace grpl::robotlua;

tvalue::tvalue() : tag_type(construct_tag_type(tag::NIL)) {}

tvalue::tvalue(bool b) : tag_type(construct_tag_type(tag::BOOL)) {
  data.value_bool = b;
}

tvalue::tvalue(lua_integer i) : tag_type(construct_tag_type(tag::NUMBER, variant::NUM_INT)) {
  data.value_int = i;
}

tvalue::tvalue(lua_number n) : tag_type(construct_tag_type(tag::NUMBER, variant::NUM_FLOAT)) {
  data.value_num = n;
}

tvalue::tvalue(uint8_t tagt) : tag_type(tagt) {
  tag t = get_tag_from_tag_type(tagt);
  if (t == tag::STRING) {
    ::new (value_string()) tvalue::string_vec();
  }
  // TODO:
}

tvalue::~tvalue() {
  tag t = get_tag_from_tag_type(tag_type);
  if (t == tag::STRING) {
    value_string()->~small_vector();
  }
}

small_vector<char, 32> *tvalue::value_string() {
  return reinterpret_cast<tvalue::string_vec *>(data.value_str_buf);
}