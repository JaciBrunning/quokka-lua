#pragma once

#include <stdint.h>
#include <functional>

#include "smallvector.h"
#include "smallstring.h"
#include "variant.h"

namespace quokka {
namespace engine {
  using lua_instruction = size_t;
  using lua_integer     = int;
  using lua_number      = double;

  /**
   * The Quokka Lua Tag Type is a simplified version of the PUC-RIO Tag Type.
   * The original version has two sections: The Tag and the Variant, with the
   * variant describing subtypes (e.g. float/integer numbers, lua/native functions).
   * We do not require variant, as we can derive that from `simple_variant`.
   * 
   * The Tag Type simply gives the 'overall type' of a value (see: lua_value).
   */
  enum class lua_tag_type {
    NIL = 0,
    BOOL = 1, 
    // light_user_data ignored
    NUMBER = 3, // Note: internally, NUMBER can be either a float or an integer internally. See lua_value for info.
    STRING = 4,
    TABLE = 5,
    FUNC = 6    // Note: internally, FUNC can be either a Lua closure or a Native closure. See lua_object for info.
    // user_data and thread ignored
  };

  /**
   * Tag types in bytecode have variant information. We don't actually care about that, since we use simplevariant,
   * so we ignore it.
   */
  inline lua_tag_type trunc_tag_type(uint8_t bc_tagtype) {
    return (lua_tag_type)(bc_tagtype & 0x0F);
  }

  /**
   * Refcountable describes a type that is deallocated after its refcount reaches zero, similar to 
   * a shared_ptr, but without the necessary heap allocation.
   */
  class refcountable {
   public:
    bool is_free = true;
    int refcount = 0;

    void use();
    void unuse();

    virtual void on_refcount_zero() {};
  };

  // Forward decls
  struct lua_object;
  struct lua_upval;

  /**
   * store_refcountable is a generic form of iterator that points to a store that may change
   * size, and thus invalidate any typical iterator types (e.g. T*). See continuous_reference<T> 
   * for more details.
   */
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

  /**
   * Storage reference for objects
   */
  using object_store_ref = store_refcountable<lua_object>;
  /**
   * Storage reference for upvals
   */
  using upval_ref = store_refcountable<lua_upval>;
  /**
   * String type for values
   */
  using value_string_t = small_string<16>;

  // Fwd decls
  struct bytecode_prototype;
  class quokka_vm;

  /**
   * lua_value is the main container for data in Lua, containing the value of any variables
   * used in the program.
   * 
   * lua_value is polymorphic, in similar representation to a C-style union due to the use of
   * simple_variant. Because of this, all lua_values are the same size, regardless of the 
   * data they hold. 
   * 
   * The type of the lua_value can be determined using one of the following methods:
   *    - is_* functions
   *    - data.is<type>()
   * For example, `value.is_numeric()` or `value.data.is<bool>()`
   * 
   * The value of the lua_value can be obtained through one of the following methods.
   *    - data.get<type>()    Note this does NOT check the data type, you should check the type prior to calling
   *    - obj()->get()        Shortcut for accessing an object, through which table and function types can be accessed.
   *                          Note that this does not check data type.
   *    - to* conversion functions  Convert types if applicable. These do perform type checking and coercion where necessary
   */
  struct lua_value {
    using string_t = value_string_t;

    /**
     * Data storage for the lua_value. 
     * 
     * <unassigned>: Nil
     * bool: Boolean
     * lua_number: Number (float)
     * lua_integer: Number (integer)
     * string_t: String
     * object_store_ref: Ref to lua_object in object store
     */
    optional_variant<bool, lua_number, lua_integer, string_t, object_store_ref> data;

    /**
     * Create a new lua_value of type Nil
     */
    lua_value();             // Nil
    /**
     * Create a new lua_value of type Bool
     */
    lua_value(bool);         // Bool
    /**
     * Create a new lua_value of type Integer
     */
    lua_value(lua_integer);  // Int
    /**
     * Create a new lua_value of type Number (decimal)
     */
    lua_value(lua_number);   // Float/Double
    /**
     * Create a new lua_value of type Object
     */
    lua_value(object_store_ref);   // Obj (table, function)
    /**
     * Create a new lua_value of type String
     */
    lua_value(const char *);   // String

    /**
     * Get the tag type of the value
     */
    lua_tag_type get_tag_type() const;

    /**
     * Is this value nil?
     */
    bool is_nil() const;

    /**
     * Is this value falsey? (Nil or False)
     */
    bool is_falsey() const;

    /**
     * Is this value numeric? (Integer or Decimal Number)
     */
    inline bool is_numeric() const {
      return is<lua_number>(data) || is<lua_integer>(data);
    }

    /**
     * Is this value an integer?
     */
    inline bool is_integer() const { return data.is<lua_integer>(); }
    /**
     * Is this value a decimal? (Number)
     */
    inline bool is_decimal() const { return data.is<lua_number>(); }
    
    /**
     * Is this value a boolean?
     */
    inline bool is_bool() const { return data.is<bool>(); }
    /**
     * Is this value a string?
     */
    inline bool is_string() const { return data.is<string_t>(); }

    /**
     * Is this value an object? (Table or Function)
     */
    inline bool is_object() const { return data.is<object_store_ref>(); }

    /**
     * Get the value of this lua_value as an object.
     */
    object_store_ref obj() const;

    /**
     * Convert this value to a number. Can convert a number, integer, or
     * string.
     * @param out The output number
     * @return True if the value was successfully converted
     */
    bool tonumber(lua_number &out) const;
    /**
     * Convert this value to an integer. Can convert a number, integer, or
     * string.
     * @param out The output integer
     * @return True if the value was successfully converted
     */
    bool tointeger(lua_integer &out) const;
    /**
     * Convert this value to a string. Can convert any lua type
     * @param out The output string
     * @return True if the value was successfully converted
     */
    bool tostring(string_t &out) const;

    /**
     * Convert this value to a number, or 0 if the conversion was unsuccessful
     * @return The number, or 0 if the conversion failed.
     */
    inline lua_number tonumber() const {
      lua_number n = 0;
      tonumber(n);
      return n;
    }

    /**
     * Convert this value to an integer, or 0 if the conversion was unsuccessful
     * @return The integer, or 0 if the conversion failed.
     */
    inline lua_integer tointeger() const {
      lua_integer i = 0;
      tointeger(i);
      return i;
    }

    /**
     * Convert this value to a string, or "" (empty string) if the conversion was unsuccessful
     * @return The number, or "" (empty string) if the conversion failed.
     */
    inline string_t tostring() const {
      string_t s("");
      tostring(s);
      return s;
    }
    
    bool operator==(const lua_value &) const;
    bool operator<(const lua_value &) const;
    bool operator<=(const lua_value &) const;
  };

  /**
   * A lua_table is the implementation of the Lua table datatype, allowing for a key-value store.
   * Note that in Quokka, we implement this as an array of pairs, to save on memory.
   * 
   * Table keys are based on equality. For bool, integer, number, and string, this is based on the
   * equality of the value. For objects (table, func), this is based on the instance of the value
   * (the object itself).
   */
  struct lua_table {
    struct node {
      lua_value key;
      lua_value value;

      node(const lua_value &k, const lua_value &v) : key(k), value(v) {}
    };
    small_vector<node, 8> entries;

    /**
     * Get a value from the table by key.
     * 
     * @param k The key of the entry
     * @return The value of the entry
     */
    lua_value get(const lua_value &k) const;

    /**
     * Set a value in the table by key.
     * 
     * @param k The key of the entry
     * @param v The value of the entry
     */
    void set(const lua_value &k, const lua_value &v);
  };

  /**
   * lua_lua_closure is the implementation of a closure (function) implemented in Lua, including
   * references to its upvals and bytecode prototype.
   */
  struct lua_lua_closure {
    bytecode_prototype *proto;
    small_vector<upval_ref, 4> upval_refs;
  };

  /**
   * lua_native_closure is the implementation of a closure (function) implemented in C++ (native).
   * It is simply a function reference, either a C function or C++ function / lambda.
   */
  struct lua_native_closure {
    using func_t = std::function<int(quokka_vm &)>;
    func_t func;
  };

  /**
   * Lua objects are datatypes that are described by more than just their value. Unlike
   * numbers, strings, and booleans, objects can be complex, such as tables. 
   * 
   * In Quokka LE, objects are allocated into one large pool (analogous to the heap),
   * and automatically dealloced when their usages reach zero. Note that objects are distinct
   * to upvalues, as objects do not (on their own) go above their own scope unless they are
   * used in an upvalue. 
   * 
   * A value may hold an object (or rather, a reference to an object), but an object is not a value.
   */
  struct lua_object : public refcountable {
    optional_variant<lua_table, lua_lua_closure, lua_native_closure> data;

    lua_object();

    lua_table &table();
    lua_lua_closure &lua_func();
    lua_native_closure &native_func();

    lua_tag_type get_tag_type() const;

    inline bool is_table() const { return data.is<lua_table>(); }
    inline bool is_function() const { return !is_table(); };

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
     * lua_value: Actual value of the upval when closed
     */
    optional_variant<size_t, lua_value> value;

    void on_refcount_zero() override;
  };
}  // namespace engine
}  // namespace quokka