#pragma once

#include <stdint.h>
#include <stddef.h>
#include <istream>

#include "smallstring.h"
#include "smallvector.h"
#include "types.h"

namespace quokka {
namespace engine {

struct bytecode_architecture {
  bool little;

  uint8_t sizeof_int;
  uint8_t sizeof_sizet;
  uint8_t sizeof_instruction;
  uint8_t sizeof_lua_integer;
  uint8_t sizeof_lua_number;

  static const bytecode_architecture system();
};

struct bytecode_header {
  char signature[4];
  uint8_t version;
  uint8_t format;
  char data[6];

  bytecode_architecture arch;

  lua_integer linteger;
  lua_number lnumber;
};

struct bytecode_upvalue {
  bool instack;
  uint8_t idx;
};

// Prototype is a description of a Lua function, providing its layout in 
// bytecode, but without any of its runtime features
struct bytecode_prototype {
  small_string<16> source;
  int line_defined;
  int last_line_defined;
  uint8_t num_params;
  uint8_t is_var_arg;
  uint8_t max_stack_size;
  /* Code */
  int num_instructions;
  small_vector<lua_instruction, 32> instructions;
  /* Constants */
  int num_constants;
  small_vector<tvalue, 16> constants;
  /* Upvalues */
  int num_upvalues;
  small_vector<bytecode_upvalue, 4> upvalues;
  /* Protos */
  /* Unfortunately this is recursive, so we have to 
     have some heap allocations */
  int num_protos;
  small_vector<bytecode_prototype *, 16> protos;    // TODO: memory leak here - functions are alloc'ed in heap.
  // Debugging information is ignored, but still must be parsed.
  
  /* RUNTIME INFO */
  object_store_ref closure_cache;
};

struct bytecode_chunk {
  bytecode_header header;
  uint8_t num_upvalues;
  bytecode_prototype root_func;
};

class bytecode_reader {
 public:
  bytecode_reader(std::istream &stream);

  void read_chunk(bytecode_chunk &);

  void read_header(bytecode_header &);
  void read_function(bytecode_architecture, bytecode_prototype &);

  uint8_t read_byte();
  void    read_block(uint8_t *out, size_t count);
  int     read_native_int(bytecode_architecture);
  size_t  read_sizet(bytecode_architecture);

  lua_instruction read_lua_instruction(bytecode_architecture);
  lua_integer     read_lua_integer(bytecode_architecture);
  lua_number      read_lua_number(bytecode_architecture);
 private:
  std::istream &_stream;
  const bytecode_architecture _sys_arch = bytecode_architecture::system();
};

#ifndef NO_BYTECODE_WRITER
class bytecode_writer {
 public:
  bytecode_writer(std::ostream &stream, bytecode_architecture target_arch);

  void write_chunk(bytecode_chunk &);

  void write_header(bytecode_header &);
  void write_function(bytecode_prototype &);

  void write_byte(uint8_t);
  void write_block(uint8_t *buf, size_t count);
  void write_native_int(int);
  void write_sizet(size_t);

  void write_lua_instruction(lua_instruction);
  void write_lua_integer(lua_integer);
  void write_lua_number(lua_number);
 private:
  std::ostream &_stream;
  bytecode_architecture _target_arch;
  const bytecode_architecture _sys_arch = bytecode_architecture::system();
};
#endif

} // namespace engine
} // namespace quokka