#pragma once

#include <algorithm>
#include <new>
#include <typeinfo>
#include <typeindex>
#include <variant>

namespace quokka {
namespace engine {
  template <typename...>
  struct type_in {
    static constexpr bool value = false;
  };

  template <typename A, typename B, typename... T>
  struct type_in<A, B, T...> {
    static constexpr bool value = std::is_same<A, B>::value || type_in<A, T...>::value;
  };

  template<typename U, typename ...T>
  using variant_enable = typename std::enable_if<type_in<U, T...>::value>::type;

  template <typename ...T>
  using optional_variant = typename std::variant<std::monostate, T...>;

  template <typename U, typename... T, typename = variant_enable<U, T...>>
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