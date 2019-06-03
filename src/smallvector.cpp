#include "quokka/engine/smallvector.h"

using namespace quokka::engine;

void small_vector_impl::reserve(size_t size) {
  if (size > _size)
    grow(size);
}

void small_vector_impl::clear() {
  chop(0);
}

size_t small_vector_impl::size() const {
  return _size;
}