#pragma once

#include <algorithm>
#include <new>
#include <typeinfo>
#include <typeindex>

namespace quokka {
namespace engine {
  // Simple implementation of std::variant, using C++ compile-time magic
  // in order to provide a union of complex (non-trivial) types.

  using variant_type_id = std::type_index;

  template<typename T>
  constexpr variant_type_id variant_typeid_func() {
    return variant_type_id(typeid(T));
  }

  /* Template specialization for move, copy, delete - since we need to infer the type */
  template <typename... T>
  struct variant_funcs;

  template <typename U, typename... T>
  struct variant_funcs<U, T...> {
    inline static void copy(const variant_type_id &id, const char *src, char *dst) {
      if (id == variant_typeid_func<U>())
        ::new (dst) U(*reinterpret_cast<const U *>(src));  // call copy
      else
        variant_funcs<T...>::copy(id, src, dst);
    }

    inline static void move(const variant_type_id &id, char *src, char *dst) {
      if (id == variant_typeid_func<U>())
        ::new (dst) U(std::move(*reinterpret_cast<U *>(src)));  // call move
      else
        variant_funcs<T...>::move(id, src, dst);  // try the next candidate type
    }

    inline static void destroy(const variant_type_id &id, char *src) {
      if (id == variant_typeid_func<U>())
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
    inline static void copy(const variant_type_id &, const char *, char *) {}
    inline static void move(const variant_type_id &, char *, char *) {}
    inline static void destroy(const variant_type_id &, char *) {}
  };

  /**
   * simple_variant is a simple implementation of std::variant to provide a union of complex
   * types, as std::variant is introduced in C++17.
   */
  template <typename... T>
  struct simple_variant {
    using variant_func_t = variant_funcs<T...>;

    static const size_t size = max_sizeof<T...>::value;

    variant_type_id _type_id = variant_typeid_func<void>();
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
      // std::copy(&other._data_raw[0], &other._data_raw[0] + size, &_data_raw[0]);
      variant_func_t::copy(other._type_id, &other._data_raw[0], &_data_raw[0]);
      return *this;
    }

    simple_variant<T...> &operator=(simple_variant &&other) {
      // Move assignment
      _type_id = other._type_id;
      // Move raw data
      // std::move(&other._data_raw[0], &other._data_raw[0] + size, &_data_raw[0]);
      variant_func_t::move(other._type_id, &other._data_raw[0], &_data_raw[0]);
      return *this;
    }

    /**
     * Set the value of the variant, forwarding arguments to the constructor.
     */
    template <typename U, typename... Args, typename = typename std::enable_if<type_in<U, T...>::value>::type>
    U& emplace(Args &&... args) {
      variant_func_t::destroy(_type_id, &_data_raw[0]);
      ::new (&_data_raw[0]) U(std::forward<Args>(args)...);
      _type_id = variant_typeid_func<U>();
      return get<U>();
    }

    /**
     * Clear the value of the variant.
     */
    void unassign() {
      variant_func_t::destroy(_type_id, &_data_raw[0]);
      _type_id = variant_typeid_func<void>();
    }

    /**
     * Get the value of the variant, given the expected type.
     * Check with `is<U>()` first, as this does not perform any type checking.
     * @param U The type
     * @return A reference to the data of the variant, as type U.
     */
    template <typename U, typename = typename std::enable_if<type_in<U, T...>::value>::type>
    U &get() const {
      return *reinterpret_cast<U *>((char *)&_data_raw[0]);
    }

    /**
     * Check if this variant is of type U.
     * @param U the type to check
     * @return True if the variant data is of type U.
     */
    template <typename U>
    bool is() const {
      return (_type_id == variant_typeid_func<U>());
    }

    /**
     * Check if this variant is assigned.
     * @return True if this variant is assigned (has data).
     */
    bool is_assigned() const { return (_type_id != variant_typeid_func<void>()); }
  };
}  // namespace engine
}  // namespace quokka