#pragma once

#include <stddef.h>
#include <utility>
#include <stdlib.h>

namespace grpl {
namespace robotlua {
/**
 * A vector that is stack-allocated up to a certain size, and is realloced
 * onto the heap if it becomes larger than the stack size. 
 * 
 * This is particularly useful for avoiding memory fragmentation on small-memory
 * platforms (like embedded systems).
 */
template <typename T, size_t STACK_SIZE, size_t GROW_BY=STACK_SIZE>
class small_vector {
 public:
  small_vector() { }

  small_vector(const small_vector &other) {
    reserve(other.size());
    for (int i = 0; i < other.size(); i++)
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

  T& operator[](size_t pos) const {
    return active_buffer()[pos];
  }

  void reserve(size_t size) {
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

  size_t size() const {
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
        new_buf[i] = active_buffer()[i];
      
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
} // namespace grpl