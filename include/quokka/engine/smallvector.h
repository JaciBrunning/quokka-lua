#pragma once

#include <stddef.h>
#include <utility>
#include <stdlib.h>

namespace quokka {
namespace engine {

class small_vector_impl {
 public:
  /**
   * Chop this vector to a certain size, deallocating all elements
   * after the chopped size.
   */
  virtual void chop(size_t) = 0;
  /**
   * Is this vector on the stack?
   */
  virtual bool is_stack() const = 0;

  /**
   * Reserve this vector to a certain size.
   */
  void reserve(size_t size);
  /**
   * Clear this vector's elements
   */
  void clear();
  /**
   * Get the size of the vector
   */
  size_t size() const;

 protected:
  /**
   * Grow this vector up to a certain size.
   */
  virtual void grow(size_t) = 0;
  
  size_t _size = 0;
};

template <typename T>
class small_vector_base : public small_vector_impl {
 public:
  /**
   * Get the raw buffer for this vector. Note that this is invalidated
   * when the vector is grown.
   */
  virtual T* raw_buffer() const = 0;

  /**
   * Get a value at an index of the vector.
   * Note that this does not perform bounds-checking.
   */
  T& operator[](size_t pos) const {
    return raw_buffer()[pos];
  }

  /**
   * Get the last value of the vector.
   */
  T& last() const {
    return raw_buffer()[_size - 1];
  }

  void chop(size_t top) override {
    for (size_t i = top; i < _size; i++)
      (raw_buffer()[i]).~T();
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
 * A small_vector is a vector that is stack-allocated up to a certain size, and is realloced
 * onto the heap if it becomes larger than the stack size. 
 * 
 * This is particularly useful for avoiding memory fragmentation on small-memory
 * platforms (like embedded systems).
 * 
 * @param T The storage type of the vector
 * @param STACK_SIZE The maximum size of the vector on the stack
 * @param GROW_BY The size to grow the vector by when new elements are added
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
    for (size_t i = 0; i < other.size(); i++)
      emplace_back(other[i]);
    return *this;
  }

  /**
   * Emplace an element into this vector, at a certain index.
   * This will grow the vector if necessary, and destruct any elements already
   * present at the index.
   * Forwards arguments to the constructor of the type T.
   */
  template<class... Args>
  T& emplace(size_t pos, Args&&... args) {
    if (pos < this->_size) {
      (raw_buffer()[pos]).~T();
    } else {
      while (pos >= _alloced_size)
        grow(this->_size + GROW_BY);
    }
    
    T* place = &raw_buffer()[pos];
    new(place) T(std::forward<Args>(args)...);

    if (pos >= this->_size)
      this->_size = pos + 1;
    
    return *place;
  }

  /**
   * Emplace an element into the end of this vector.
   * This will grow the vector if necessary.
   * Forwards arguments to the constructor of type T.
   */
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
  void grow(size_t next_size) override {
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
} // namespace engine
} // namespace quokka