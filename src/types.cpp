#include "jaci/robotlua/types.h"

#include <new>
#include <cstring>

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

lua_object::lua_object() {
  data.unassign();
}

lua_table &lua_object::table() {
  if (data.is<lua_table>())
    return data.get<lua_table>();
  return data.emplace<lua_table>();
}

lua_lclosure &lua_object::lclosure() {
  if (data.is<lua_lclosure>())
    return data.get<lua_lclosure>();

  return data.emplace<lua_lclosure>();
}

lua_native_closure &lua_object::native_closure() {
  if (data.is<lua_native_closure>())
    return data.get<lua_native_closure>();

  return data.emplace<lua_native_closure>();
}

lua_tag_type lua_object::get_tag_type() const {
  if (!data.is_assigned())
    return lua_tag_type::NIL;
  if (data.is<lua_table>())
    return lua_tag_type::TABLE;
  return lua_tag_type::FUNC;
}

void lua_object::on_refcount_zero() {
  data.unassign();
}

tvalue::tvalue() {
  data.unassign();
}

tvalue::tvalue(bool b) {
  data.emplace<bool>(b);
}

tvalue::tvalue(lua_integer i) {
  data.emplace<lua_integer>(i);
}

tvalue::tvalue(lua_number n) {
  data.emplace<lua_number>(n);
}

tvalue::tvalue(object_store_ref ref) {
  data.emplace<object_store_ref>(ref);
}

tvalue::tvalue(const char *s) {
  data.emplace<string_vec>(s);
}

lua_tag_type tvalue::get_tag_type() const {
  if (!data.is_assigned()) return lua_tag_type::NIL;
  if (data.is<bool>())
    return lua_tag_type::BOOL;
  if (data.is<lua_number>() || data.is<lua_integer>())
    return lua_tag_type::NUMBER;
  if (data.is<tvalue::string_vec>())
    return lua_tag_type::STRING;
  // Is object
  return data.get<object_store_ref>().get()->get_tag_type();
}

bool tvalue::is_nil() {
  return !data.is_assigned();
}

bool tvalue::is_falsey() {
  return is_nil() || (data.is<bool>() && !data.get<bool>());
}

object_store_ref tvalue::obj() {
  return data.get<object_store_ref>();
}

tvalue::string_vec &tvalue::str() {
  return data.get<string_vec>();
}

bool tvalue::operator==(const tvalue &other) const {
  if (get_tag_type() != other.get_tag_type()) return false;
  // nil has no data
  if (get_tag_type() != lua_tag_type::NIL) {
    if (data.is<bool>())
      return data.get<bool>() == other.data.get<bool>();
    else if (data.is<lua_number>())
      return data.get<lua_number>() == other.data.get<lua_number>();
    else if (data.is<lua_integer>())
      return data.get<lua_integer>() == other.data.get<lua_integer>();
    else if (data.is<string_vec>()) {
      // Compare string values
      string_vec &a = data.get<string_vec>(), &b = other.data.get<string_vec>();
      if (a.length() != b.length()) return false;
      for (size_t i = 0; i < a.length(); i++)
        if (a[i] != b[i])
          return false;
    } else {
      // Object
      return data.get<object_store_ref>() == other.data.get<object_store_ref>();
    }
  }

  return true;
}

bool tvalue::operator<(const tvalue &other) const {
  if (get_tag_type() == other.get_tag_type()) {
    lua_number na, nb;
    if (conv::tonumber((tvalue &)*this, na) && conv::tonumber((tvalue &)other, nb)) {
      return na < nb;
    } else if (data.is<string_vec>()) {
      int strc = strcmp(data.get<string_vec>().c_str(), other.data.get<string_vec>().c_str());
      return strc < 0;
    }
  }
  return false;
}

bool tvalue::operator<=(const tvalue &other) const {
  if (get_tag_type() == other.get_tag_type()) {
    lua_number na, nb;
    if (conv::tonumber((tvalue &)*this, na) && conv::tonumber((tvalue &)other, nb)) {
      return na <= nb;
    } else if (data.is<string_vec>()) {
      int strc = strcmp(data.get<string_vec>().c_str(), other.data.get<string_vec>().c_str());
      return strc <= 0;
    }
  }
  return false;
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