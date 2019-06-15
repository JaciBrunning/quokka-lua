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

void lua_object::on_refcount_zero() {
  unassign(data);
}

bool ::quokka::engine::tonumber(const lua_value &v, lua_number &out) {
  bool ret = true;
  std::visit(overloaded {
    [&](auto&&) { ret = false; },
    [&](lua_integer i) { out = (lua_number) i; },
    [&](lua_number f) { out = f; },
    [&](lua_string &s) {
      char *end;
      lua_number n = strtod(s.c_str(), &end);
      if (*end != '\0')
        ret = false;
      out = n;
    }
  }, v);
  return ret;
}

bool ::quokka::engine::tointeger(const lua_value &v, lua_integer &out) {
  lua_number n;
  if (is<lua_integer>(v)) {
    out = std::get<lua_integer>(v);
    return true;
  } else if (tonumber(v, n)) {
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

bool ::quokka::engine::tostring(const lua_value &v, lua_string &out) {
  char buf[16];
  bool ret = true;
  std::visit(overloaded {
    [&](auto&&) { ret = false; },
    [&](lua_string &s) { out.clear(); out.concat_str(s); },
    [&](std::monostate) { out.clear(); out.concat("nil"); },
    [&](lua_integer i) {
      snprintf(buf, 16, "%d", i);
      out.clear(); out.concat(buf);
    },
    [&](lua_number n) {
      snprintf(buf, 16, "%f", n);
      out.clear(); out.concat(buf);
    },
    [&](bool b) {
      out.clear(); out.concat(b ? "true" : "false");
    },
    [&](object_view &v) {
      out.clear();
      out.concat((*v).is_table() ? "table: <unknown>" : "function: <unknown>");
    }
  }, v);
  return ret;
}

// bool lua_value::operator==(const lua_value &other) const {
//   if (get_tag_type() != other.get_tag_type()) return false;
//   // nil has no data
//   if (get_tag_type() != lua_tag_type::NIL) {
//     if (is<bool>(data))
//       return std::get<bool>(data) == std::get<bool>(other.data);
//     else if (is<lua_number>(data))
//       return std::get<lua_number>(data) == std::get<lua_number>(other.data);
//     else if (is<lua_integer>(data))
//       return std::get<lua_integer>(data) == std::get<lua_integer>(other.data);
//     else if (is<string_t>(data)) {
//       // Compare string values
//       const string_t &a = std::get<string_t>(data), &b = std::get<string_t>(other.data);
//       if (a.length() != b.length()) return false;
//       for (size_t i = 0; i < a.length(); i++)
//         if (a[i] != b[i])
//           return false;
//     } else {
//       // Object
//       return std::get<object_view>(data) == std::get<object_view>(other.data);
//     }
//   }

//   return true;
// }

// bool lua_value::operator<(const lua_value &other) const {
//   if (get_tag_type() == other.get_tag_type()) {
//     lua_number na, nb;
//     if (this->tonumber(na) && other.tonumber(nb)) {
//       return na < nb;
//     } else if (is<string_t>(data)) {
//       int strc = strcmp(std::get<string_t>(data).c_str(), std::get<string_t>(other.data).c_str());
//       return strc < 0;
//     }
//   }
//   return false;
// }

// bool lua_value::operator<=(const lua_value &other) const {
//   if (get_tag_type() == other.get_tag_type()) {
//     lua_number na, nb;
//     if (this->tonumber(na) && other.tonumber(nb)) {
//       return na <= nb;
//     } else if (is<string_t>(data)) {
//       int strc = strcmp(std::get<string_t>(data).c_str(), std::get<string_t>(other.data).c_str());
//       return strc <= 0;
//     }
//   }
//   return false;
// }

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