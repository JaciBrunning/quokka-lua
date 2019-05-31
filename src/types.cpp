#include "jaci/robotlua/types.h"

#include <new>

using namespace jaci::robotlua;

void refcountable::use() {
  refcount++;
  is_free = false;
}

void refcountable::unuse() {
  refcount--;
  if (refcount == 0) {
    is_free = true;
    on_refcount_zero();
  }
}

lua_object::lua_object() : tag_type(construct_tag_type(tag::NIL)) { }

lua_table &lua_object::table() {
  if (data.is<lua_table>())
    return data.get<lua_table>();
  tag_type = construct_tag_type(tag::TABLE);
  return data.emplace<lua_table>();
}

lua_closure &lua_object::closure() {
  if (data.is<lua_closure>())
    return data.get<lua_closure>();
  return data.emplace<lua_closure>();
}

lua_lclosure &lua_object::lclosure() {
  lua_closure &parent = closure();
  if (parent.impl.is<lua_lclosure>())
    return parent.impl.get<lua_lclosure>();

  tag_type = construct_tag_type(tag::FUNC, variant::FUNC_LUA);
  return parent.impl.emplace<lua_lclosure>();
}

lua_native_closure &lua_object::native_closure() {
  lua_closure &parent = closure();
  if (parent.impl.is<lua_native_closure>())
    return parent.impl.get<lua_native_closure>();

  tag_type = construct_tag_type(tag::FUNC, variant::FUNC_C);
  return parent.impl.emplace<lua_native_closure>();
}

void lua_object::on_refcount_zero() {
  data.unassign();
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

tvalue::tvalue(const char *s) : tag_type(construct_tag_type(tag::STRING)) {
  data.emplace<string_vec>(s);
}

bool tvalue::is_nil() {
  return get_tag_from_tag_type(tag_type) == tag::NIL;
}

bool tvalue::is_falsey() {
  return is_nil() || (data.is<bool>() && !data.get<bool>());
}

object_store_ref tvalue::obj() {
  return data.get<object_store_ref>();
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

tvalue lua_table::get(const tvalue &key) const {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].key == key)
      return entries[i].value;
  }
  return tvalue();  // nil
}

void lua_table::set(const tvalue &k, const tvalue &v) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].key == k) {
      entries[i].value = v;
      return;
    }
  }

  entries.emplace_back(k, v);
}

void lua_upval::on_refcount_zero() {
  value.unassign();
}