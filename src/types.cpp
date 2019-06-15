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
  unassign(data);
}

lua_table &lua_object::table() {
  if (is<lua_table>(data))
    return std::get<lua_table>(data);
  return data.emplace<lua_table>();
}

lua_lua_closure &lua_object::lua_func() {
  if (is<lua_lua_closure>(data))
    return std::get<lua_lua_closure>(data);

  return data.emplace<lua_lua_closure>();
}

lua_native_closure &lua_object::native_func() {
  if (is<lua_native_closure>(data))
    return std::get<lua_native_closure>(data);

  return data.emplace<lua_native_closure>();
}

lua_tag_type lua_object::get_tag_type() const {
  if (!is_assigned(data))
    return lua_tag_type::NIL;
  if (is<lua_table>(data))
    return lua_tag_type::TABLE;
  return lua_tag_type::FUNC;
}

void lua_object::on_refcount_zero() {
  unassign(data);
}

lua_value::lua_value() {
  unassign(data);
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

lua_value::lua_value(object_view ref) {
  data.emplace<object_view>(ref);
}

lua_value::lua_value(const char *s) {
  data.emplace<string_t>(s);
}

lua_tag_type lua_value::get_tag_type() const {
  if (!is_assigned(data)) return lua_tag_type::NIL;
  if (is<bool>(data))
    return lua_tag_type::BOOL;
  if (is<lua_number>(data) || is<lua_integer>(data))
    return lua_tag_type::NUMBER;
  if (is<lua_value::string_t>(data))
    return lua_tag_type::STRING;
  // Is object
  return std::get<object_view>(data).get()->get_tag_type();
}

bool lua_value::is_nil() const {
  return !is_assigned(data);
}

bool lua_value::is_falsey() const {
  return is_nil() || (is<bool>(data) && !std::get<bool>(data));
}

object_view lua_value::obj() const {
  return std::get<object_view>(data);
}

bool lua_value::tonumber(lua_number &out) const {
  if (is<lua_number>(data)) {
    out = std::get<lua_number>(data);
    return true;
  } else if (is<lua_integer>(data)) {
    out = (lua_number) std::get<lua_integer>(data);
    return true;
  } else if (is<lua_value::string_t>(data)) {
    // Try to parse string
    char *end;
    lua_number n = strtod(std::get<lua_value::string_t>(data).c_str(), &end);
    if (*end != '\0')
      return false;
    out = n;
    return true;
  }
  return false;
}

bool lua_value::tointeger(lua_integer &out) const {
  lua_number n;
  if (is<lua_integer>(data)) {
    out = std::get<lua_integer>(data);
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
  if (is<lua_value::string_t>(data)) {
    out.concat_str(std::get<lua_value::string_t>(data));
    return true;
  } else if (is<lua_integer>(data)) {
    snprintf(buf, 16, "%d", std::get<lua_integer>(data));
    out.concat(buf);
    return true;
  } else if (is<lua_number>(data)) {
    snprintf(buf, 16, "%f", std::get<lua_number>(data));
    out.concat(buf);
    return true;
  } else if (is<bool>(data)) {
    out.concat(std::get<bool>(data) ? "true" : "false");
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
    if (is<bool>(data))
      return std::get<bool>(data) == std::get<bool>(other.data);
    else if (is<lua_number>(data))
      return std::get<lua_number>(data) == std::get<lua_number>(other.data);
    else if (is<lua_integer>(data))
      return std::get<lua_integer>(data) == std::get<lua_integer>(other.data);
    else if (is<string_t>(data)) {
      // Compare string values
      const string_t &a = std::get<string_t>(data), &b = std::get<string_t>(other.data);
      if (a.length() != b.length()) return false;
      for (size_t i = 0; i < a.length(); i++)
        if (a[i] != b[i])
          return false;
    } else {
      // Object
      return std::get<object_view>(data) == std::get<object_view>(other.data);
    }
  }

  return true;
}

bool lua_value::operator<(const lua_value &other) const {
  if (get_tag_type() == other.get_tag_type()) {
    lua_number na, nb;
    if (this->tonumber(na) && other.tonumber(nb)) {
      return na < nb;
    } else if (is<string_t>(data)) {
      int strc = strcmp(std::get<string_t>(data).c_str(), std::get<string_t>(other.data).c_str());
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
    } else if (is<string_t>(data)) {
      int strc = strcmp(std::get<string_t>(data).c_str(), std::get<string_t>(other.data).c_str());
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
  unassign(value);
}