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

  inline uint8_t construct_tag_type(tag t, variant v = variant::NONE) {
    return (uint8_t)t | ((uint8_t)v << 4);
  }

  inline tag get_tag_from_tag_type(uint8_t tag_type) { return (tag)(tag_type & 0x0F); }
  inline variant get_variant_from_tag_type(uint8_t tag_type) {
    return (variant)(tag_type >> 4);
  }

  struct lua_object;
  struct lua_upval;

  // TODO: Generify these

  // Store object reference with refcount.
  struct object_store_ref : small_vector_base<lua_object>::continuous_reference {
    object_store_ref() = delete;

    object_store_ref(small_vector_base<lua_object> *v, size_t id);
    object_store_ref(const object_store_ref &other) : object_store_ref(other.vec, other.idx) { }
    object_store_ref(object_store_ref &&other) : object_store_ref(other.vec, other.idx) { }

    ~object_store_ref();
  };

  // Store upval reference with refcount
  struct upval_ref : small_vector_base<lua_upval>::continuous_reference {
    upval_ref() = delete;

    upval_ref(small_vector_base<lua_upval> *v, size_t id);
    upval_ref(const upval_ref &other) : upval_ref(other.vec, other.idx) { }
    upval_ref(upval_ref &&other) : upval_ref(other.vec, other.idx) { }

    ~upval_ref();
  };

  // Note: fwd declaration of bytecode_prototype in bytecode.h
  struct bytecode_prototype;

  // Note: fwd declaration of vm in vm.h
  class vm;

  struct lua_lclosure {
    bytecode_prototype *proto;
    small_vector<upval_ref, 4> upval_refs;
  };

  struct lua_native_closure {
    std::function<int(vm &)> func;
  };

  struct lua_closure {
    simple_variant<lua_lclosure, lua_native_closure> impl;
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

    bool is_nil();
    bool is_falsey();
    object_store_ref obj();
    bool operator==(const tvalue &) const;
  };

  struct lua_table {
    struct node {
      tvalue key;
      tvalue value;

      node(const tvalue &k, const tvalue &v) : key(k), value(v) {}
    };
    small_vector<node, 16> entries;

    tvalue get(const tvalue &) const;
    void set(const tvalue &, const tvalue &);
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

    lua_table &table();
    lua_closure &closure();
    lua_lclosure &lclosure();
    lua_native_closure &native_closure(bool light=false);

    void use();
    void unuse();

   private:
    size_t refcount;
  };
  
  /**
   * The Upval is a construct of Lua that allows for values to exist oustide of their
   * regular scope. Consider the case of the anonymous function:
   * 
   * function createFunc()
   *  local i = 0
   *  local anon = function()
   *    i = i + 1
   *    return i
   *  end
   *  anon()
   *  return anon
   * end
   * 
   * In this case, "i" would usually go out of scope upon the return of createFunc(), 
   * but since it is being used by the anonymous function, we can't allow that to happen.
   * "i" is, in a sense, made global until all instances of the anonymous function are
   * not used anymore. "i" is an upval.
   * 
   * Note that "i", while still an upval, exists in two states at different times during 
   * execution. Before the local "i" goes out of scope (i.e. createFunc returns), the "i"
   * value is shared between createFunc and the anonymous function, hence it is on the stack.
   * In this state, "i" is referred to as an "open upval".
   * 
   * When createFunc returns, the stack entry for createFunc and its variables is popped. In 
   * this case, "i" would go out of scope and would no longer be accessible to the anonymous 
   * function. To solve this, each time a function is returned, its upvals (if still in use)
   * are moved to a global upval table, outside of the stack entirely. In this case, "i" is 
   * classified as a "closed upval" (it holds its own data, instead of pointing to a value
   * already on the stack).
   */
  struct lua_upval {
    /**
     * size_t: Stack offset of upval when open
     * tvalue: Actual value of the upval when closed
     */
    simple_variant<size_t, tvalue> value;
    bool is_free;

    lua_upval();

    void use();
    void unuse();
   private:
    size_t refcount;
  };

  namespace conv {
    inline bool tonumber(tvalue &src, lua_number &out) {
      if (src.data.is<lua_number>()) {
        out = src.data.get<lua_number>();
        return true;
      } else if (src.data.is<lua_integer>()) {
        out = (lua_number) src.data.get<lua_integer>();
        return true;
      } else if (src.data.is<tvalue::string_vec>()) {
        // Try to parse string
        return false;
      }
      return false;
    }

    inline bool tointeger(tvalue &src, lua_integer &out) {
      if (src.data.is<lua_integer>()) {
        out = src.data.get<lua_integer>();
        return true;
      } else if (src.data.is<lua_number>()) {
        out = (lua_integer) src.data.get<lua_number>();
        return true;
      } else if (src.data.is<tvalue::string_vec>()) {
        // Try to parse string
        return false;
      }
      return false;
    }
  }
}  // namespace robotlua
}  // namespace grpl