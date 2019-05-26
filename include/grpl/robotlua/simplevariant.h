#pragma once

#include <algorithm>
#include <new>
#include <typeinfo>

namespace grpl {
namespace robotlua {

  // Simple implementation of std::variant, using C++ compile-time magic
  // in order to provide a union of complex (non-trivial) types.

  /* Template specialization for move, copy, delete - since we need to infer the type */
  template <typename... T>
  struct variant_funcs;

  template <typename U, typename... T>
  struct variant_funcs<U, T...> {
    inline static void copy(size_t id, char *src, char *dst) {
      if (id == typeid(U).hash_code())
        ::new (dst) U(*reinterpret_cast<const U *>(src));  // call copy
      else
        variant_funcs<T...>::copy(id, src, dst);
    }

    inline static void move(size_t id, char *src, char *dst) {
      if (id == typeid(U).hash_code())
        ::new (dst) U(std::move(*reinterpret_cast<U *>(src)));  // call move
      else
        variant_funcs<T...>::move(id, src, dst);  // try the next candidate type
    }

    inline static void destroy(size_t id, char *src) {
      if (id == typeid(U).hash_code())
        reinterpret_cast<U *>(src)->~U();
      else
        variant_funcs<T...>::destroy(id, src);  // try the next candidate type
    }
  };

  /* Template specialization to provide a max sizeof function, statically */
  template <typename A, typename... X>
  struct max_sizeof;

  template <typename A>
  struct max_sizeof<A> {
    static const size_t value = sizeof(A);
  };

  template <typename A, typename B, typename... X>
  struct max_sizeof<A, B, X...> {
    static const size_t value =
        sizeof(A) >= sizeof(B) ? max_sizeof<A, X...>::value : max_sizeof<B, X...>::value;
  };

  /* Template specialization to enforce type safety */
  template <typename...>
  struct type_in {
    static constexpr bool value = false;
  };

  template <typename A, typename B, typename... T>
  struct type_in<A, B, T...> {
    static constexpr bool value = std::is_same<A, B>::value || type_in<A, T...>::value;
  };

  // For when all options are exhaused (i.e. none type)
  template <>
  struct variant_funcs<> {
    inline static void copy(size_t, char *, char *) {}
    inline static void move(size_t, char *, char *) {}
    inline static void destroy(size_t, char *) {}
  };

  /* Variant impl */
  template <typename... T>
  struct simple_variant {
    using variant_func_t = variant_funcs<T...>;

    static const size_t size = max_sizeof<T...>::value;

    size_t _type_id = typeid(void).hash_code();
    char   _data_raw[size];

    simple_variant() {}

    template <typename U, typename... Args, typename std::enable_if<type_in<U, T...>::value>::type>
    simple_variant(Args &&... args) {
      emplace(std::forward<Args>(args)...);
    }

    simple_variant(const simple_variant<T...> &other) {
      // Copy
      _type_id = other._type_id;
      variant_func_t::copy(other._type_id, &other._data_raw[0], &_data_raw[0]);
    }

    simple_variant(simple_variant<T...> &&other) {
      // Move
      _type_id = other._type_id;
      variant_func_t::move(other._type_id, &other._data_raw[0], &_data_raw[0]);
    }

    ~simple_variant() { variant_func_t::destroy(_type_id, &_data_raw[0]); }

    simple_variant<T...> &operator=(const simple_variant<T...> &other) {
      // Copy assignment
      _type_id = other._type_id;
      // Copy raw data
      std::copy(&other._data_raw[0], &other._data_raw[0] + size, &_data_raw[0]);
      return *this;
    }

    simple_variant<T...> &operator=(simple_variant &&other) {
      // Move assignment
      _type_id = other._type_id;
      // Move raw data
      std::move(&other._data_raw[0], &other._data_raw[0] + size, &_data_raw[0]);
      return *this;
    }

    template <typename U, typename... Args, typename = typename std::enable_if<type_in<U, T...>::value>::type>
    U& emplace(Args &&... args) {
      variant_func_t::destroy(_type_id, &_data_raw[0]);
      ::new (&_data_raw[0]) U(std::forward<Args>(args)...);
      _type_id = typeid(U).hash_code();
      return get<U>();
    }

    template <typename U, typename = typename std::enable_if<type_in<U, T...>::value>::type>
    U &get() {
      return *reinterpret_cast<U *>(&_data_raw[0]);
    }

    template <typename U>
    bool is() {
      return (_type_id == typeid(U).hash_code());
    }

    bool is_assigned() { return (_type_id != typeid(void).hash_code()); }
  };

}  // namespace robotlua
}  // namespace grpl