#pragma once

#include <optional>

#include "variant.h"

namespace quokka {
namespace engine {

  /* Refcount Object  */
  template <typename T, typename Enable=void>
  class refcount;

  template <typename ...T>
  class refcount<std::variant<T...>,  variant_enable<std::monostate, T...>> : public std::variant<T...> {
   public:
    using variant_t = std::variant<T...>;
    static constexpr bool is_optional = false;

    constexpr operator std::variant<T...>() const { return *this; }
    constexpr const std::variant<T...> &value() const { return *this; }
    constexpr std::variant<T...> &value() { return *this; }

    constexpr bool is_free() const { return _refcount == 0; }
    constexpr bool is_assigned() const { return _refcount > 0; }

    inline void use() { _refcount++; }
    inline void unuse() {
      if (_refcount > 0) {
        _refcount--;
        if (_refcount == 0)
          unassign(*this);
      }
    }
   private:
    int _refcount = 0;
  };
}
}