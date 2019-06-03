#pragma once

#include <stdint.h>
#include <functional>

#include "smallvector.h"
#include "smallstring.h"
#include "simplevariant.h"

namespace jaci {
namespace robotlua {
  using lua_instruction = size_t;
  using lua_integer     = int;
  using lua_number      = double;

  enum class lua_tag_type {
    NIL = 0,
    BOOL = 1, 
    // light_user_data ignored
    NUMBER = 3, // Note: internally, NUMBER can be either a float or an integer internally. See tvalue for info.
    STRING = 4,
    TABLE = 5,
    FUNC = 6    // Note: internally, FUNC can be either a Lua closure or a Native closure. See lua_object for info.
    // user_data and thread ignored
  };

  // Tag types in bytecode have variant information. We don't actually care about that, since we use simplevariant,
  // so we ignore it.
  inline lua_tag_type trunc_tag_type(uint8_t bc_tagtype) {
    return (lua_tag_type)(bc_tagtype & 0x0F);
  }

  struct lua_object;
  struct lua_upval;

  class refcountable {
   public:
    bool is_free = true;
    int refcount = 0;

    void use();
    void unuse();

    virtual void on_refcount_zero() {};
  };

  // Store object reference with refcount.
  template<typename T>
  struct store_refcountable : public continuous_reference<T> {
    store_refcountable() : continuous_reference<T>(nullptr, 0) {}

    store_refcountable(small_vector_base<T> *v, size_t id) : continuous_reference<T>(v, id) {
      if (continuous_reference<T>::is_valid())
        continuous_reference<T>::get()->use();
    }

    store_refcountable(const store_refcountable &other) : store_refcountable(other.vec, other.idx) {}
    store_refcountable(store_refcountable &&other) : store_refcountable(other.vec, other.idx) {}

    store_refcountable &operator=(const store_refcountable &other) {
      if (continuous_reference<T>::is_valid())
        continuous_reference<T>::get()->unuse();
      
      this->vec = other.vec;
      this->idx = other.idx;

      if (continuous_reference<T>::is_valid())
        continuous_reference<T>::get()->use();
      
      return *this;
    }

    ~store_refcountable() {
      if (continuous_reference<T>::is_valid())
        continuous_reference<T>::get()->unuse();
    }
  };

  using object_store_ref = store_refcountable<lua_object>;
  using upval_ref = store_refcountable<lua_upval>;

  // Note: fwd declaration of bytecode_prototype in bytecode.h
  struct bytecode_prototype;

  // Note: fwd declaration of vm in vm.h (needed by lua_native_closure funcdec)
  class vm;

  struct tvalue {
    using string_t = small_string<32>;

    /**
     * no type: Nil
     * bool: Boolean
     * lua_number: Number (float)
     * lua_integer: Number (integer)
     * string_t: String
     * object_store_ref: Ref to lua_object in object store
     */
    simple_variant<bool, lua_number, lua_integer, string_t, object_store_ref> data;

    tvalue();             // Nil
    tvalue(bool);         // Bool
    tvalue(lua_integer);  // Int
    tvalue(lua_number);   // Float
    tvalue(object_store_ref);   // Obj (table, function)
    tvalue(const char *);   // String

    lua_tag_type get_tag_type() const;
    bool is_nil() const;
    bool is_falsey() const;

    object_store_ref obj() const;

    bool tonumber(lua_number &out) const;
    lua_number tonumber() const;
    bool tointeger(lua_integer &out) const;
    lua_integer tointeger() const;

    bool operator==(const tvalue &) const;
    bool operator<(const tvalue &) const;
    bool operator<=(const tvalue &) const;
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

  struct lua_lclosure {
    bytecode_prototype *proto;
    small_vector<upval_ref, 4> upval_refs;
  };

  struct lua_native_closure {
    using func_t = std::function<int(vm &)>;
    func_t func;
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
  struct lua_object : public refcountable {
    simple_variant<lua_table, lua_lclosure, lua_native_closure> data;

    lua_object();

    lua_table &table();
    lua_lclosure &lclosure();
    lua_native_closure &native_closure();

    lua_tag_type get_tag_type() const;

    void on_refcount_zero() override;
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
  struct lua_upval : public refcountable {
    /**
     * size_t: Stack offset of upval when open
     * tvalue: Actual value of the upval when closed
     */
    simple_variant<size_t, tvalue> value;

    void on_refcount_zero() override;
  };
}  // namespace robotlua
}  // namespace jaci