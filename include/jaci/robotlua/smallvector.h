#pragma once

#include <stddef.h>
#include <utility>
#include <stdlib.h>

namespace jaci {
namespace robotlua {

template <typename T>
class small_vector_base {
 public:
  // A reference to a certain element in a small_vector, that is not invalidated
  // like a regular iterator when the vector changes internal layout.
  struct continuous_reference {
    small_vector_base *vec;
    size_t idx;

    continuous_reference() {}
    continuous_reference(small_vector_base *v, size_t i) : vec(v), idx(i) {}

    T *operator*() const {
      return &vec->operator[](idx);
    }

    T* get() const {
      return &vec->operator[](idx);
    }

    bool operator==(const continuous_reference &other) const {
      return (vec == other.vec) && (idx == other.idx);
    }
  };

  virtual T& operator[](size_t) const = 0;
  virtual void reserve(size_t) = 0;
  virtual size_t size() const = 0;
};

/**
 * A vector that is stack-allocated up to a certain size, and is realloced
 * onto the heap if it becomes larger than the stack size. 
 * 
 * This is particularly useful for avoiding memory fragmentation on small-memory
 * platforms (like embedded systems).
 */
template <typename T, size_t STACK_SIZE, size_t GROW_BY=STACK_SIZE>
class small_vector : public small_vector_base<T> {
 public:
  small_vector() { }

  small_vector(const small_vector &other) {
    reserve(other.size());
    for (size_t i = 0; i < other.size(); i++)
      emplace_back(other[i]);
  }

  ~small_vector() {
    clear_elements();
  }

  small_vector &operator=(const small_vector &other) {
    clear();
    reserve(other.size());
    for (int i = 0; i < other.size(); i++)
      emplace_back(other[i]);
    return *this;
  }

  T& operator[](size_t pos) const override {
    return active_buffer()[pos];
  }

  T& last() const {
    return active_buffer()[size() - 1];
  }

  void reserve(size_t size) override {
    if (size > _size) {
      grow(size);
    }
  }

  template<class... Args>
  T& emplace(size_t pos, Args&&... args) {
    if (pos < _size) {
      (&active_buffer()[pos])->~T();
    }

    while (pos >= _alloced_size)
      grow(size() + GROW_BY);
    
    T* place = &active_buffer()[pos];
    new(place) T(std::forward<Args>(args)...);
    if (pos >= _size)
      _size++;
    return *place;
  }

  template<class... Args>
  T& emplace_back(Args&&... args) {
    return emplace(size(), std::forward<Args>(args)...);
  }

  void chop(size_t top) {
    for (size_t i = top; i < size(); i++) {
      (&active_buffer()[i])->~T();
    }
    if (top < _size)
      _size = top;
  }

  size_t size() const override {
    return _size;
  }

  bool is_stack() const {
    return _heapvec == nullptr;
  }

  void clear() {
    for (size_t i = 0; i < _size; i++) {
      (&active_buffer()[i])->~T();
    }
    _size = 0;
  }

  T* raw_buffer() const {
    return active_buffer();
  }

 protected:
  void grow(size_t next_size) {
    if (next_size > STACK_SIZE && next_size > _alloced_size) {
      T* new_buf = (T*)malloc(sizeof(T) * next_size);
      for (size_t i = 0; i < _size; i++)
        new(&new_buf[i]) T(active_buffer()[i]);
      
      clear_elements();

      _heapvec = new_buf;
      _alloced_size = next_size;
    }
  }

  T* active_buffer() const {
    if (is_stack())
      return (T*)_stackvec;
    return _heapvec;
  }

  void clear_elements() {
    clear();

    if (!is_stack()) {
      free(_heapvec);
    }
  }

 private:
  alignas(alignof(T)) char _stackvec[STACK_SIZE * sizeof(T)];
  T* _heapvec = nullptr;
  size_t _size = 0;
  size_t _alloced_size = STACK_SIZE;
};
} // namespace robotlua
} // namespace jaci