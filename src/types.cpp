#include "quokka/engine/types.h"

#include <new>
#include <cstring>

using namespace quokka::engine;

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

lua_value::lua_value() {
  data.unassign();
}

lua_value::lua_value(bool b) {
  data.emplace<bool>(b);
}

lua_value::lua_value(lua_integer i) {
  data.emplace<lua_integer>(i);
}

lua_value::lua_value(lua_number n) {
  data.emplace<lua_number>(n);
}

lua_value::lua_value(object_store_ref ref) {
  data.emplace<object_store_ref>(ref);
}

lua_value::lua_value(const char *s) {
  data.emplace<string_t>(s);
}

lua_tag_type lua_value::get_tag_type() const {
  if (!data.is_assigned()) return lua_tag_type::NIL;
  if (data.is<bool>())
    return lua_tag_type::BOOL;
  if (data.is<lua_number>() || data.is<lua_integer>())
    return lua_tag_type::NUMBER;
  if (data.is<lua_value::string_t>())
    return lua_tag_type::STRING;
  // Is object
  return data.get<object_store_ref>().get()->get_tag_type();
}

bool lua_value::is_nil() const {
  return !data.is_assigned();
}

bool lua_value::is_falsey() const {
  return is_nil() || (data.is<bool>() && !data.get<bool>());
}

object_store_ref lua_value::obj() const {
  return data.get<object_store_ref>();
}

bool lua_value::tonumber(lua_number &out) const {
  if (data.is<lua_number>()) {
    out = data.get<lua_number>();
    return true;
  } else if (data.is<lua_integer>()) {
    out = (lua_number) data.get<lua_integer>();
    return true;
  } else if (data.is<lua_value::string_t>()) {
    // Try to parse string
    char *end;
    lua_number n = strtod(data.get<lua_value::string_t>().c_str(), &end);
    if (*end != '\0')
      return false;
    out = n;
    return true;
  }
  return false;
}

bool lua_value::tointeger(lua_integer &out) const {
  lua_number n;
  if (data.is<lua_integer>()) {
    out = data.get<lua_integer>();
    return true;
  } else if (tonumber(n)) {
    // Safe for doubles to be converted to int
    if (n < std::numeric_limits<lua_integer>::lowest())
      out = std::numeric_limits<lua_integer>::lowest();
    else if (n > std::numeric_limits<lua_integer>::max())
      out = std::numeric_limits<lua_integer>::max();
    else
      out = (lua_integer) n;
    return true;
  }
  return false;
}

bool lua_value::tostring(string_t &out) const {
  char buf[16];
  out.clear();
  if (data.is<lua_value::string_t>()) {
    out.concat_str(data.get<lua_value::string_t>());
    return true;
  } else if (data.is<lua_integer>()) {
    snprintf(buf, 16, "%d", data.get<lua_integer>());
    out.concat(buf);
    return true;
  } else if (data.is<lua_number>()) {
    snprintf(buf, 16, "%f", data.get<lua_number>());
    out.concat(buf);
    return true;
  } else if (data.is<bool>()) {
    out.concat(data.get<bool>() ? "true" : "false");
    return true;
  } else if (is_nil()) {
    out.concat("nil");
    return true;
  } else if (is_object()) {
    out.concat(obj().get()->is_table() ? "table: <unknown>" : "function: <unknown>");
    return true;
  }
  return false;
}

bool lua_value::operator==(const lua_value &other) const {
  if (get_tag_type() != other.get_tag_type()) return false;
  // nil has no data
  if (get_tag_type() != lua_tag_type::NIL) {
    if (data.is<bool>())
      return data.get<bool>() == other.data.get<bool>();
    else if (data.is<lua_number>())
      return data.get<lua_number>() == other.data.get<lua_number>();
    else if (data.is<lua_integer>())
      return data.get<lua_integer>() == other.data.get<lua_integer>();
    else if (data.is<string_t>()) {
      // Compare string values
      string_t &a = data.get<string_t>(), &b = other.data.get<string_t>();
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

bool lua_value::operator<(const lua_value &other) const {
  if (get_tag_type() == other.get_tag_type()) {
    lua_number na, nb;
    if (this->tonumber(na) && other.tonumber(nb)) {
      return na < nb;
    } else if (data.is<string_t>()) {
      int strc = strcmp(data.get<string_t>().c_str(), other.data.get<string_t>().c_str());
      return strc < 0;
    }
  }
  return false;
}

bool lua_value::operator<=(const lua_value &other) const {
  if (get_tag_type() == other.get_tag_type()) {
    lua_number na, nb;
    if (this->tonumber(na) && other.tonumber(nb)) {
      return na <= nb;
    } else if (data.is<string_t>()) {
      int strc = strcmp(data.get<string_t>().c_str(), other.data.get<string_t>().c_str());
      return strc <= 0;
    }
  }
  return false;
}

lua_value lua_table::get(const lua_value &key) const {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].key == key)
      return entries[i].value;
  }
  return lua_value();  // nil
}

void lua_table::set(const lua_value &k, const lua_value &v) {
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