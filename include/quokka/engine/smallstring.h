#pragma once

#include "smallvector.h"

namespace quokka {
namespace engine {

  /**
   * small_string is a simple string implementation that, up to a specific size, will
   * exist on the stack. After the string reaches a certain size, specified by STACK_SIZE,
   * it will grow and move to the heap, where it acts as std::vector<char>, with extra 
   * methods for strings.
   * 
   * Note that small_string contains one extra character, for the C string null byte 
   * termination character (\0).
   */
  template<size_t STACK_SIZE, size_t GROW_BY=STACK_SIZE>
  class small_string : public small_vector<char, STACK_SIZE, GROW_BY> {
   public:
    /**
     * Construct a small_string with no data (empty string). 
     */
    small_string() {
      end_str();
    }

    /**
     * Construct a small_string and fill it with the given C string.
     */
    small_string(const char *src) {
      while (*src) {
        concat(*src);
        src++;
      }
    }

    /**
     * Get the length of the string, minus the null termination char.
     */
    size_t length() const {
      size_t s = this->size();
      return s == 0 ? s : s - 1;
    }

    /**
     * Concat a null termination char if one does not already exist.
     */
    void end_str() {
      if ((*this)[length()] != '\0')
        this->emplace_back('\0');
    }
    
    /**
     * Obtain the contents of the string as a C string. Note that, if 
     * this string grows, the pointer becomes invalid.
     * 
     * @return A pointer to the content as a C string, terminated by 
     * a null byte (\0).
     */
    char *c_str() {
      return this->raw_buffer();
    }

    /**
     * Concatenate a character to the string. Respects the null byte.
     */
    void concat(char c) {
      size_t i = this->size();
      if ((*this)[length()] == '\0')
        i = length();
      this->emplace(i, c);
      end_str();
    }

    /**
     * Concatenate another small_string (or std::string) to the string.
     * Concatenates in-place.
     * 
     * @param other The string to concatenate to this one.
     */
    template<typename STRING_LIKE>
    void concat_str(const STRING_LIKE &other) {
      for (size_t i = 0; i < other.length(); i++) {
        concat(other[i]);
      }
    }

    /**
     * Concatenate a C string to this string. Concatenates in-place.
     * 
     * @param other The C string to concatenate to this one.
     */
    void concat(const char *other) {
      while (*other) {
        concat(*other);
        other++;
      }
    }
  };
}
}  // namespace quokka