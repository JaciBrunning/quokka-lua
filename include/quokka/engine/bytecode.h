#pragma once

#include <stdint.h>
#include <stddef.h>
#include <istream>

#include "smallstring.h"
#include "smallvector.h"
#include "types.h"
#include "pointer.h"

namespace quokka {
namespace engine {

/**
 * Description of a bytecode architecture. Defines the system the bytecode is compiled
 * for.
 */
struct bytecode_architecture {
  bool little;

  uint8_t sizeof_int;
  uint8_t sizeof_sizet;
  uint8_t sizeof_instruction;
  uint8_t sizeof_lua_integer;
  uint8_t sizeof_lua_number;

  /**
   * Obtain the system architecture.
   */
  static const bytecode_architecture system();
};

/**
 * Header for a bytecode file. Contains information about the bytecode architecture, as well
 * as some error checking information.
 */
struct bytecode_header {
  /**
   * Lua signature: literal "\x1BLua"
   */
  char signature[4];
  uint8_t version;
  uint8_t format;
  /**
   * Bytecode error checking data:  Literal "\x19\x93\r\n\x1A\n"
   */
  char data[6];

  bytecode_architecture arch;

  /**
   * Integer error checking data: 0x5678 (used to check endianness)
   */
  lua_integer linteger;
  /**
   * Double error checking data: 370.5
   */
  lua_number lnumber;
};

/**
 * Description of a prototype upval.
 */
struct bytecode_upvalue {
  bool instack;
  uint8_t idx;
};

/**
 * Prototype is a description of a Lua function (aka closure), providing its
 * layout in bytecode, but without any of its runtime features.
 * 
 * Debug information is not included in Quokka LE.
 */
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
  small_vector<small_shared_ptr<bytecode_prototype>, 16> protos;
  // Debugging information is ignored, but still must be parsed.
  
  /* RUNTIME INFO */
  /* This is used by the runtime to cache the closure. It has no bytecode purpose */
  object_store_ref closure_cache;
};

/**
 * A chunk is a unit of compilation in Lua, representing a file. 
 */
struct bytecode_chunk {
  bytecode_header header;
  uint8_t num_upvalues;
  bytecode_prototype root_func;
};

/**
 * The bytecode reader will read a bytecode structure from an istream,
 * be it from a memory region, or a file.
 */
class bytecode_reader {
 public:
  /**
   * Create a new bytecode reader
   * @param stream The input stream, pointing to either a memory region
   *                or a file (or any other implementation of std::istream).
   */
  bytecode_reader(std::istream &stream);

  /**
   * Read a chunk from the stream.
   * @param c The bytecode chunk to output to
   */
  void read_chunk(bytecode_chunk &c);

  /**
   * Read a chunk from the stream
   * @returns The bytecode chunk
   */
  bytecode_chunk read_chunk() {
    bytecode_chunk c;
    read_chunk(c);
    return c;
  }

  /* INTERNAL */

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

#ifdef WITH_BYTECODE_WRITER
/**
 * The bytecode writer allows writing a bytecode program with a specified architecture,
 * allowing for transpiling.
 */
class bytecode_writer {
 public:
  /**
   * Create a new bytecode writer
   * @param stream The output stream, pointing to a memory region, file, or any other std::ostream.
   * @param target_arch The target architecture. 
   */
  bytecode_writer(std::ostream &stream, bytecode_architecture target_arch);

  /**
   * Write a bytecode chunk to the stream
   * @param c The bytecode chunk to write
   */
  void write_chunk(bytecode_chunk &c);

  /* INTERNAL */

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