#pragma once

#include "smallvector.h"
#include "refcount.h"
#include "variant.h"

namespace quokka {
namespace engine {
  /* Indexed View */
  template<typename T, typename V>
  class indexed_view {
   public:
    indexed_view() : _vec(nullptr) {}
    indexed_view(V *v, size_t i) : _vec(v), _idx(i) {}

    constexpr T* get() const {
      return _vec ? &_vec->operator[](_idx) : nullptr;
    }

    constexpr T* operator->() const {
      return get();
    }

    constexpr T& operator*() const {
      return _vec->operator[](_idx);
    }

    bool operator==(const indexed_view &other) const {
      return (_vec == other._vec && _idx == other._idx);
    }

    bool is_valid() {
      return _vec != nullptr;
    }
   protected:
    V *_vec = nullptr;
    size_t _idx;
  };

  template <typename T>
  using small_vector_view = indexed_view<T, small_vector_base<T>>;

  /* Refcount View */
  template <typename T, typename V>
  class refcount_indexed_view : public indexed_view<T, V> {
   public:
    using indexed_t = indexed_view<T, V>;
    using indexed_t::get;
    using indexed_t::operator*;
    using indexed_t::is_valid;

    refcount_indexed_view() : indexed_view<T, V>() { }
    refcount_indexed_view(V *v, size_t i) : indexed_view<T, V>(v, i) {
      if (is_valid()) 
        get()->use();
    }

    refcount_indexed_view(const refcount_indexed_view &other) 
        : refcount_indexed_view(other._vec, other._idx) { }
    refcount_indexed_view(refcount_indexed_view &&other)
        : refcount_indexed_view(other._vec, other._idx) { }

    constexpr refcount_indexed_view &operator=(const refcount_indexed_view &other) {
      if (is_valid())
        get()->unuse();
      
      this->_vec = other._vec;
      this->_idx = other._idx;

      if (is_valid())
        get()->use();

      return *this;
    }

    ~refcount_indexed_view() {
      if (is_valid())
        get()->unuse();
    }
  };

  template <typename T>
  using small_vector_refcount_view = refcount_indexed_view<T, small_vector_base<T>>;
}
}
