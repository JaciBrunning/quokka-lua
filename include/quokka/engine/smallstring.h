#pragma once

#include "smallvector.h"

namespace quokka {
namespace engine {
  template<size_t STACK_SIZE, size_t GROW_BY=STACK_SIZE>
  class small_string : public small_vector<char, STACK_SIZE, GROW_BY> {
   public:
    small_string() {
      end_str();
    }

    small_string(const char *src) {
      while (*src) {
        concat(*src);
        src++;
      }
    }

    /**
     * Distinct to size() since size() includes the C \0 
     * string termination character (null byte)
     */
    size_t length() const {
      size_t s = this->size();
      return s == 0 ? s : s - 1;
    }

    void end_str() {
      if ((*this)[length()] != '\0')
        this->emplace_back('\0');
    }
    
    char *c_str() {
      return this->raw_buffer();
    }

    void concat(char c) {
      size_t i = this->size();
      if ((*this)[length()] == '\0')
        i = length();
      this->emplace(i, c);
      end_str();
    }

    template<typename STRING_LIKE>
    void concat(const STRING_LIKE &other) {
      for (size_t i = 0; i < other.length(); i++) {
        concat(other[i]);
      }
    }

    void concat(const char *other) {
      while (*other) {
        concat(*other);
        other++;
      }
    }
  };
}
}  // namespace quokka