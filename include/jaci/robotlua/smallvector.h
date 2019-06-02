#pragma once

#include <stddef.h>
#include <utility>
#include <stdlib.h>

namespace jaci {
namespace robotlua {

class small_vector_impl {
 public:
  virtual void grow(size_t) = 0;
  virtual void chop(size_t) = 0;
  virtual bool is_stack() const = 0;

  void reserve(size_t size);
  void clear();
  size_t size() const;

 protected:
  size_t _size = 0;
};

template <typename T>
class small_vector_base : public small_vector_impl {
 public:
  virtual T* raw_buffer() const = 0;

  T& operator[](size_t pos) const {
    return raw_buffer()[pos];
  }

  T& last() const {
    return raw_buffer()[_size - 1];
  }

  void chop(size_t top) override {
    for (size_t i = top; i < _size; i++)
      (&raw_buffer()[i])->~T();
    if (top < _size)
      _size = top;
  }

  bool is_stack() const override {
    return _heapvec == nullptr;
  }

 protected:
  T* _heapvec = nullptr;

  void clear_elements() {
    clear();
    if (_heapvec != nullptr)
      free(_heapvec);
  }
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
    this->reserve(other.size());
    for (size_t i = 0; i < other.size(); i++)
      emplace_back(other[i]);
  }

  ~small_vector() {
    this->clear_elements();
  }

  small_vector &operator=(const small_vector &other) {
    this->clear();
    this->reserve(other.size());
    for (int i = 0; i < other.size(); i++)
      emplace_back(other[i]);
    return *this;
  }

  template<class... Args>
  T& emplace(size_t pos, Args&&... args) {
    if (pos < this->_size) {
      (&raw_buffer()[pos])->~T();
    }

    while (pos >= _alloced_size)
      grow(this->_size + GROW_BY);
    
    T* place = &raw_buffer()[pos];
    new(place) T(std::forward<Args>(args)...);
    if (pos >= this->_size)
      this->_size++;
    return *place;
  }

  template<class... Args>
  T& emplace_back(Args&&... args) {
    return emplace(this->_size, std::forward<Args>(args)...);
  }

  T* raw_buffer() const override {
    if (this->_heapvec == nullptr)
      return (T*)_stackvec;
    return this->_heapvec;
  }

 protected:
  void grow(size_t next_size) {
    if (next_size > STACK_SIZE && next_size > _alloced_size) {
      T* new_buf = (T*)malloc(sizeof(T) * next_size);
      for (size_t i = 0; i < this->_size; i++)
        new(&new_buf[i]) T(raw_buffer()[i]);
      
      size_t os = this->_size;
      this->clear_elements();
      this->_size = os;

      this->_heapvec = new_buf;
      _alloced_size = next_size;
    }
  }

 private:
  alignas(alignof(T)) char _stackvec[STACK_SIZE * sizeof(T)];
  size_t _alloced_size = STACK_SIZE;
};

// A reference to a certain element in a small_vector, that is not invalidated
// like a regular iterator when the vector changes internal layout.
template<typename T>
struct continuous_reference {
  small_vector_base<T> *vec;
  size_t idx;

  continuous_reference() : vec(nullptr) {}
  continuous_reference(small_vector_base<T> *v, size_t i) : vec(v), idx(i) {}

  T *operator*() const {
    return &vec->operator[](idx);
  }

  T* get() const {
    return &vec->operator[](idx);
  }

  bool operator==(const continuous_reference &other) const {
    return (vec == other.vec) && (idx == other.idx);
  }

  bool is_valid() {
    return vec != nullptr;
  }
};
} // namespace robotlua
} // namespace jaci