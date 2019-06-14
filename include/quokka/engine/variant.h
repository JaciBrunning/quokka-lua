#pragma once

#include <algorithm>
#include <new>
#include <typeinfo>
#include <typeindex>
#include <variant>

namespace quokka {
namespace engine {
  /* Template specialization to enforce type safety */
  template <typename...>
  struct type_in {
    static constexpr bool value = false;
  };

  template <typename A, typename B, typename... T>
  struct type_in<A, B, T...> {
    static constexpr bool value = std::is_same<A, B>::value || type_in<A, T...>::value;
  };

  template <typename U, typename... T>
  struct variant_enable {
    static constexpr bool value = std::enable_if<type_in<U, T...>::value>::type;
  };

  template <typename ...T>
  using optional_variant = typename std::variant<std::monostate, T...>;

  // template<typename ...T>
  // struct optional_variant : public std::variant<std::monostate, T...> {
  //   /**
  //    * Get the value of the variant, given the expected type.
  //    * Check with `is<U>()` first, as this does not perform any type checking.
  //    * @param U The type
  //    * @return A reference to the data of the variant, as type U.
  //    */
  //   template <typename U, typename = typename std::enable_if<type_in<U, T...>::value>::type>
  //   inline U &get() const {
  //     return const_cast<U&>(std::get<U>(*this));
  //   }

  //   /**
  //    * Check if this variant is of type U.
  //    * @param U the type to check
  //    * @return True if the variant data is of type U.
  //    */
  //   template <typename U>
  //   inline bool is() const {
  //     return std::holds_alternative<U>(*this);
  //   }

  //   inline bool is_assigned() const {
  //     return !is<std::monostate>();
  //   }

  //   inline void unassign() {
  //     this->template emplace<std::monostate>();
  //   }
  // };

  template <typename U, typename... T, typename = typename variant_enable<U, T...>::value>
  constexpr bool is(const std::variant<T...> &var) {
    return std::holds_alternative<U>(var);
  }

  template <typename... T>
  constexpr bool is_assigned(const optional_variant<T...> &var) {
    return !is<std::monostate>(var);
  }

  template <typename... T>
  constexpr void unassign(optional_variant<T...> &var) {
    var.template emplace<std::monostate>();
  }

  /* Helpers for std::visit */
  template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
  template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace engine
}  // namespace quokka