#pragma once

#include <stdint.h>
#include <functional>

#include "smallvector.h"
#include "simplevariant.h"

namespace grpl {
namespace robotlua {
  using lua_instruction = size_t;
  using lua_integer     = int;
  using lua_number      = double;

  enum class tag {
    NIL             = 0,
    BOOL            = 1,
    LIGHT_USER_DATA = 2,
    NUMBER          = 3,
    STRING          = 4,
    TABLE           = 5,
    FUNC            = 6,
    USER_DATA       = 7,
    THREAD          = 8
  };

  enum class variant {
    NONE         = 0,
    /* Number */
    NUM_FLOAT    = 0,
    NUM_INT      = 1,
    /* String */
    STR_SHORT    = 0,
    STR_LONG     = 1,  // NOTE: RobotLua does not differentiate between Long and Short strings.
    /* Function */
    FUNC_LUA     = 0,
    FUNC_LIGHT_C = 1,
    FUNC_C       = 2
  };

  struct lua_lclosure {
    size_t proto_idx;
    small_vector<size_t, 4> upval_refs;
  };

  struct lua_native_closure {
    std::function<void()> func;
  };

  struct lua_closure {
    simple_variant<lua_lclosure, lua_native_closure> impl;
  };

  inline uint8_t construct_tag_type(tag t, variant v = variant::NONE) {
    return (uint8_t)t | ((uint8_t)v << 4);
  }

  inline tag get_tag_from_tag_type(uint8_t tag_type) { return (tag)(tag_type & 0x0F); }
  inline variant get_variant_from_tag_type(uint8_t tag_type) {
    return (variant)(tag_type >> 4);
  }

  struct lua_object;

  // Store object reference with refcount.
  struct object_store_ref : small_vector_base<lua_object>::continuous_reference {
    object_store_ref() = delete;

    object_store_ref(small_vector_base<lua_object> *v, size_t id) {
      vec = v;
      idx = idx;
      get()->use();
    }

    object_store_ref(const object_store_ref &other) : object_store_ref(other.vec, other.idx) { }
    object_store_ref(object_store_ref &&other) : object_store_ref(other.vec, other.idx) { }

    ~object_store_ref() {
      get()->unuse();
    }
  };

  struct tvalue {
    using string_vec = small_vector<char, 32>;

    uint8_t tag_type;
    /**
     * no type: Nil
     * bool: Boolean
     * lua_number: Number (float)
     * lua_integer: Number (integer)
     * string_vec: String
     * object_store_ref: Ref to lua_object in object store
     */
    simple_variant<bool, lua_number, lua_integer, string_vec, object_store_ref> data;

    tvalue();             // Nil
    tvalue(bool);         // Bool
    tvalue(lua_integer);  // Int
    tvalue(lua_number);   // Float
    tvalue(object_store_ref);   // Obj (table, function)

    tvalue(uint8_t tagt);  // String

    ~tvalue();

    void set_object(size_t position, lua_object &obj);
    bool operator==(const tvalue &) const;
  };

  struct lua_table {
    struct node {
      // Pointer to the global table store
      tvalue key_ref;
      // Pointer to the global table store
      tvalue value_ref;
    };
    small_vector<node, 16> entries;
  };

  /**
   * Lua objects are datatypes that are described by more than just their value. Unline
   * numbers, strings, and booleans, objects can be complex, such as Tables. 
   * 
   * In RobotLua, objects are allocated into one large pool (analogous to the heap),
   * and automatically dealloced when their usages reach zero. Note that objects are distinct
   * to upvalues, as objects do not (on their own) go above their own scope unless they are
   * used in an upvalue. 
   * 
   * A value may hold an object (or rather, a reference to an object), but an object is not a value.
   */
  struct lua_object {
    bool is_free;
    uint8_t tag_type;
    simple_variant<lua_table, lua_closure> data;

    lua_object();

    lua_table &new_table();
    lua_lclosure &new_lclosure();
    lua_native_closure &new_native_closure(bool light=false);

    void use();
    void unuse();

   private:
    size_t refcount;
  };

}  // namespace robotlua
}  // namespace grpl