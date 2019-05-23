#include "grpl/robotlua/bytecode.h"

#include <byteswap.h>

#ifdef ROBOTLUA_DEBUG
  #include <stdio.h>
  #define PDEBUG(...) printf(__VA_ARGS__);
#else
  #define PDEBUG(...)
#endif

using namespace grpl::robotlua;

const bytecode_architecture bytecode_architecture::system() {
  union {
    uint16_t i;
    char c[2];
  } e = { 0x0100 };

  bytecode_architecture arch;
  arch.little = !e.c[0];

  arch.sizeof_int = (uint8_t)sizeof(int);
  arch.sizeof_sizet = (uint8_t)sizeof(size_t);
  arch.sizeof_instruction = (uint8_t)sizeof(lua_instruction);
  arch.sizeof_lua_integer = (uint8_t)sizeof(lua_integer);
  arch.sizeof_lua_number = (uint8_t)sizeof(lua_number);
  return arch;
}

/* READER */

template<typename VT>
static void read_lua_string(bytecode_reader &reader, bytecode_architecture arch, VT &vec) {
  uint8_t b_size = reader.read_byte();
  if (b_size == 0)
    return;
  if (b_size < 0xFF) {
    // Small string
    for (size_t i = 0; i < (size_t)b_size - 1; i++) 
      vec.emplace_back((char)reader.read_byte());
  } else {
    // Long string
    size_t size = reader.read_sizet(arch);
    for (size_t i = 0; i < (size_t)size - 1; i++)
      vec.emplace_back((char)reader.read_byte());
  }
}

template<typename T>
static T read_size(std::istream &stream) {
  T t;
  stream.read((char *)&t, sizeof(T));
  return t;
}

template<typename T>
static T read_numeric(std::istream &stream, bool little, uint8_t size_target, bool sys_little) {
  uint8_t size_sys = (uint8_t)sizeof(T);
  bool endian_match = little == sys_little;
  if (endian_match && size_target == size_sys) {
    return read_size<T>(stream);
  } else if (size_target == sizeof(int16_t)) {
    int16_t v = read_size<int16_t>(stream);
    if (!endian_match)
      v = __bswap_16(v);
    return (T) v;
  } else if (size_target == sizeof(int32_t)) {
    int32_t v = read_size<int32_t>(stream);
    if (!endian_match)
      v = __bswap_32(v);
    return (T) v;
  } else if (size_target == sizeof(int64_t)) {
    int64_t v = read_size<int64_t>(stream);
    if (!endian_match)
      v = __bswap_64(v);
    return (T) v;
  }
  return 0;
}

bytecode_reader::bytecode_reader(std::istream &s) : _stream(s) { }

void bytecode_reader::read_chunk(bytecode_chunk &chunk) {
  read_header(chunk.header);
  chunk.num_upvalues = read_byte();
  read_function(chunk.header.arch, chunk.root_func);
}

void bytecode_reader::read_header(bytecode_header &data) {
  read_block((uint8_t *)data.signature, 4);
  data.version = read_byte();
  data.format = read_byte();
  read_block((uint8_t *)data.data, 6);
  data.arch.sizeof_int = read_byte();
  data.arch.sizeof_sizet = read_byte();
  data.arch.sizeof_instruction = read_byte();
  data.arch.sizeof_lua_integer = read_byte();
  data.arch.sizeof_lua_number = read_byte();
  // Read integer
  data.arch.little = _stream.peek() == 0x78;  // Number is 0x5678, we can check endianness this way
  data.linteger = read_lua_integer(data.arch);
  // Read number
  data.lnumber = read_lua_number(data.arch);
}

void bytecode_reader::read_function(bytecode_architecture arch, bytecode_function &func) {
  read_lua_string(*this, arch, func.source);
  func.line_defined = read_native_int(arch);
  func.last_line_defined = read_native_int(arch);
  func.num_params = read_byte();
  func.is_var_arg = read_byte();
  func.max_stack_size = read_byte();

  func.num_instructions = read_native_int(arch);
  for (int i = 0; i < func.num_instructions; i++) {
    func.instructions.emplace_back(read_lua_instruction(arch));
  }

  // Read constants directly since we need to change the emplace args.
  func.num_constants = read_native_int(arch);
  for (int i = 0; i < func.num_constants; i++) {
    uint8_t type_tag = read_byte();
    tag t = get_tag_from_tag_type(type_tag);
    if (t == tag::BOOL) {
      func.constants.emplace_back();
    } else if (type_tag == construct_tag_type(tag::NUMBER, variant::NUM_FLOAT)) {
      func.constants.emplace_back(read_lua_number(arch));
    } else if (type_tag == construct_tag_type(tag::NUMBER, variant::NUM_INT)) {
      func.constants.emplace_back(read_lua_integer(arch));
    } else if (t == tag::STRING) {
      tvalue &val = func.constants.emplace_back(type_tag);
      read_lua_string(*this, arch, *val.value_string());
    }
  }

  func.num_upvalues = read_native_int(arch);
  for (int i = 0; i < func.num_upvalues; i++) {
    func.upvalues.emplace_back(bytecode_upvalue{(uint8_t)read_byte(), (uint8_t)read_byte()});
  }

  func.num_protos = read_native_int(arch);
  for (int i = 0; i < func.num_protos; i++) {
    bytecode_function *f = new bytecode_function();
    read_function(arch, *f);
    func.protos.emplace_back(f);
  }

  /* Read debugging information */
  /* We ignore the debugging info, but we have to advance the 
     stream anyway */
  // TODO: Do we require this for C funcs to load info?

  small_vector<char, 32> tmp_vec;
  int num_opcode_map = read_native_int(arch);
  for (int i = 0; i < num_opcode_map; i++)
    read_native_int(arch);

  int num_locvar_map = read_native_int(arch);
  for (int i = 0; i < num_locvar_map; i++) {
    read_lua_string(*this, arch, tmp_vec);
    read_native_int(arch);
    read_native_int(arch);
    tmp_vec.clear();
  }

  int num_upval_map = read_native_int(arch);
  for (int i = 0; i < num_upval_map; i++) {
    read_lua_string(*this, arch, tmp_vec);
    tmp_vec.clear();
  }
}

int bytecode_reader::read_native_int(bytecode_architecture arch) {
  if (arch.little == _sys_arch.little && arch.sizeof_int == _sys_arch.sizeof_int) {
    int i;
    _stream.read((char *)&i, sizeof(int));
    return i;
  } else {
    // TODO:
    return 0;
  }
}

size_t bytecode_reader::read_sizet(bytecode_architecture arch) {
  return read_numeric<size_t>(_stream, arch.little, arch.sizeof_sizet, _sys_arch.little);
}

uint8_t bytecode_reader::read_byte() {
  return _stream.get();
}

void bytecode_reader::read_block(uint8_t *out, size_t count) {
  for (size_t i = 0; i < count; i++)
    _stream.get(*(char *)(out + i));
}

lua_instruction bytecode_reader::read_lua_instruction(bytecode_architecture arch) {
  return read_numeric<lua_instruction>(_stream, arch.little, arch.sizeof_instruction, _sys_arch.little);
}

lua_integer bytecode_reader::read_lua_integer(bytecode_architecture arch) {
  return read_numeric<lua_integer>(_stream, arch.little, arch.sizeof_lua_integer, _sys_arch.little);
}

lua_number bytecode_reader::read_lua_number(bytecode_architecture arch) {
  lua_number number;
  if (arch.sizeof_lua_number == _sys_arch.sizeof_lua_number) {
    // Shortcut if it's equal
    _stream.read((char *)&number, sizeof(lua_number));
  } else if (arch.sizeof_lua_number == sizeof(float)) {
    float n;
    _stream.read((char *)&n, arch.sizeof_lua_number);
    number = n;
  } else if (arch.sizeof_lua_number == sizeof(double)) {
    double n;
    _stream.read((char *)&n, arch.sizeof_lua_number);
    number = n;
  }
  return number;
}