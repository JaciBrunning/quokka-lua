#pragma once

namespace quokka {
namespace engine {
  // std::shared_ptr enlarges out binary by 10KiB, which is a bit too much, so we
  // implement out own implementation

  /**
   * Implementation of std::shared_ptr.
   */
  template <class T>
  class small_shared_ptr {
   public:
    small_shared_ptr() {}

    explicit small_shared_ptr(T* p) : _ptr(p), _cnt(new unsigned int(1)) { }
    small_shared_ptr(const small_shared_ptr &other) : _ptr(other._ptr), _cnt(other._cnt) {
      if (_cnt)
        (*_cnt)++;
    }

    small_shared_ptr &operator=(const small_shared_ptr &other) {
      if (this != &other) {
        clear();
        _ptr = other._ptr;
        _cnt = other._cnt;
        if (_cnt)
          (*_cnt)++;
      }

      return *this;
    }
    
    ~small_shared_ptr() {
      clear();
    }

    void clear() {
      if (_cnt) {
        // If this is the last one - delete the ptr
        if (*_cnt == 1)
          delete _ptr;
        // If this is the last instance, delete count.
        if (!--*_cnt)
          delete _cnt;
      }
      _cnt = 0;
      _ptr = 0;
    }

    T* get() const {
      return (_cnt) ? _ptr : 0;
    }

    T* operator->() const {
      return get();
    }

    T& operator*() const {
      return *get();
    }

   private:
    T* _ptr;
    unsigned int *_cnt;
  };
}  // namespace engine
}  // namespace quokka