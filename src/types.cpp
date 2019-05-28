#include "grpl/robotlua/types.h"

#include <new>

using namespace grpl::robotlua;

lua_object::lua_object() : tag_type(construct_tag_type(tag::NIL)) {
  is_free = true;
  refcount = 0;
}

lua_table &lua_object::new_table() {
  tag_type = construct_tag_type(tag::TABLE);
  return data.emplace<lua_table>();
}

lua_lclosure &lua_object::new_lclosure() {
  tag_type = construct_tag_type(tag::FUNC, variant::FUNC_LUA);
  return data.emplace<lua_closure>().impl.emplace<lua_lclosure>();
}

lua_native_closure &lua_object::new_native_closure(bool light) {
  tag_type = construct_tag_type(tag::FUNC, light ? variant::FUNC_LIGHT_C : variant::FUNC_C);
  return data.emplace<lua_closure>().impl.emplace<lua_native_closure>();
}

void lua_object::use() {
  refcount++;
  is_free = false;
}

void lua_object::unuse() {
  refcount--;
  if (refcount == 0) {
    is_free = true;
    if (!data.is_assigned())
      data.unassign();
  }
}

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

tvalue::tvalue(object_store_ref ref) {
  tag_type = (*ref)->tag_type;
  data.emplace<object_store_ref>(ref);
}

tvalue::tvalue(uint8_t tagt) : tag_type(tagt) {
  tag t = get_tag_from_tag_type(tagt);
  variant v = get_variant_from_tag_type(tagt);
  if (t == tag::STRING) {
    data.emplace<string_vec>();
  } else {
    // TODO: Error
  }
}

tvalue::~tvalue() {
  data.~simple_variant();
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
      return data.get<object_store_ref>() == other.data.get<object_store_ref>();
    }
  }

  return true;
}