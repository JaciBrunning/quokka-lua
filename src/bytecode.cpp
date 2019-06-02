#include "jaci/robotlua/bytecode.h"

using namespace jaci::robotlua;

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
static inline uint16_t __bswap_16(uint16_t v) {
  return OSSwapInt16(v);
}

static inline uint32_t __bswap_32(uint32_t v) {
  return OSSwapInt32(v);
}

static inline uint64_t __bswap_64(uint64_t v) {
  return OSSwapInt64(v);
}
#elif defined(NO_BYTESWAP)
static inline uint16_t __bswap_16(uint16_t v) {
  return ((uint16_t) ((((v) >> 8) & 0xff) | (((v) & 0xff) << 8)));
}

static inline uint32_t __bswap_32(uint32_t v) {
  return ((((v) & 0xff000000u) >> 24) | (((v) & 0x00ff0000u) >> 8)
   | (((v) & 0x0000ff00u) << 8) | (((v) & 0x000000ffu) << 24));
}

static inline uint64_t __bswap_64(uint64_t v) {
  return ((((v) & 0xff00000000000000ull) >> 56)	
   | (((v) & 0x00ff000000000000ull) >> 40)	
   | (((v) & 0x0000ff0000000000ull) >> 24)	
   | (((v) & 0x000000ff00000000ull) >> 8)	
   | (((v) & 0x00000000ff000000ull) << 8)	
   | (((v) & 0x0000000000ff0000ull) << 24)	
   | (((v) & 0x000000000000ff00ull) << 40)	
   | (((v) & 0x00000000000000ffull) << 56));
}
#else
#include <byteswap.h>
#endif

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
    vec.end_str();
  } else {
    // Long string
    size_t size = reader.read_sizet(arch);
    for (size_t i = 0; i < (size_t)size - 1; i++)
      vec.emplace_back((char)reader.read_byte());
    vec.end_str();
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

void bytecode_reader::read_function(bytecode_architecture arch, bytecode_prototype &func) {
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
      func.constants.emplace_back((bool)read_byte());
    } else if (type_tag == construct_tag_type(tag::NUMBER, variant::NUM_FLOAT)) {
      func.constants.emplace_back(read_lua_number(arch));
    } else if (type_tag == construct_tag_type(tag::NUMBER, variant::NUM_INT)) {
      func.constants.emplace_back(read_lua_integer(arch));
    } else if (t == tag::STRING) {
      tvalue &val = func.constants.emplace_back(type_tag);
      // read_lua_string(*this, arch, *val.value_string());
      read_lua_string(*this, arch, val.data.get<tvalue::string_vec>());
    }
  }

  func.num_upvalues = read_native_int(arch);
  for (int i = 0; i < func.num_upvalues; i++) {
    func.upvalues.emplace_back(bytecode_upvalue{read_byte() > 0, (uint8_t)read_byte()});
  }

  func.num_protos = read_native_int(arch);
  for (int i = 0; i < func.num_protos; i++) {
    bytecode_prototype *f = new bytecode_prototype();
    read_function(arch, *f);
    func.protos.emplace_back(f);
  }

  /* Read debugging information */
  /* We ignore the debugging info, but we have to advance the 
     stream anyway */

  small_string<32> tmp_vec;
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

uint8_t bytecode_reader::read_byte() {
  return _stream.get();
}

void bytecode_reader::read_block(uint8_t *out, size_t count) {
  _stream.read((char *)out, count);
}

int bytecode_reader::read_native_int(bytecode_architecture arch) {
  return read_numeric<int>(_stream, arch.little, arch.sizeof_int, _sys_arch.little);
}

size_t bytecode_reader::read_sizet(bytecode_architecture arch) {
  return read_numeric<size_t>(_stream, arch.little, arch.sizeof_sizet, _sys_arch.little);
}

lua_instruction bytecode_reader::read_lua_instruction(bytecode_architecture arch) {
  return read_numeric<lua_instruction>(_stream, arch.little, arch.sizeof_instruction, _sys_arch.little);
}

lua_integer bytecode_reader::read_lua_integer(bytecode_architecture arch) {
  return read_numeric<lua_integer>(_stream, arch.little, arch.sizeof_lua_integer, _sys_arch.little);
}

lua_number bytecode_reader::read_lua_number(bytecode_architecture arch) {
  lua_number number = 0;
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

/* WRITER */

#ifndef NO_BYTECODE_WRITER
template<typename VT>
static void write_lua_string(bytecode_writer &writer, VT &vec) {
  if (vec.size() + 1 < 0xFF) {
    writer.write_byte(vec.size() + 1);
  } else {
    writer.write_byte(0xFF);
    writer.write_sizet(vec.size() + 1);
  }

  writer.write_block((uint8_t *)vec.raw_buffer(), vec.size());
}

template<typename T>
static void write_size(T n, std::ostream &stream) {
  stream.write((char *)&n, sizeof(T));
}

template<typename T>
static void write_numeric(T n, std::ostream &stream, bool little, uint8_t size_target, bool sys_little) {
  uint8_t size_sys = (uint8_t)sizeof(T);
  bool endian_match = little == sys_little;

  if (endian_match && size_target == size_sys) {
    write_size(n, stream);
  } else if (size_target == sizeof(uint16_t)) {
    uint16_t v = n;
    if (!endian_match)
      v = __bswap_16(v);
    write_size(v, stream);
  } else if (size_target == sizeof(uint32_t)) {
    uint32_t v = n;
    if (!endian_match)
      v = __bswap_32(v);
    write_size(v, stream);
  } else if (size_target == sizeof(uint64_t)) {
    uint64_t v = n;
    if (!endian_match)
      v = __bswap_64(v);
    write_size(v, stream);
  }
}

bytecode_writer::bytecode_writer(std::ostream &stream, bytecode_architecture target_arch) : _stream(stream), _target_arch(target_arch) {}

void bytecode_writer::write_chunk(bytecode_chunk &chunk) {
  write_header(chunk.header);
  write_byte(chunk.num_upvalues);
  write_function(chunk.root_func);
}

void bytecode_writer::write_header(bytecode_header &header) {
  write_block((uint8_t *)header.signature, 4);
  write_byte(header.version);
  write_byte(header.format);
  write_block((uint8_t *)header.data, 6);
  // Ignore the header arch since we're overriding it with our own
  write_byte(_target_arch.sizeof_int);
  write_byte(_target_arch.sizeof_sizet);
  write_byte(_target_arch.sizeof_instruction);
  write_byte(_target_arch.sizeof_lua_integer);
  write_byte(_target_arch.sizeof_lua_number);
  write_lua_integer(header.linteger);
  write_lua_number(header.lnumber);
}

void bytecode_writer::write_function(bytecode_prototype &func) {
  write_lua_string(*this, func.source);

  write_native_int(func.line_defined);
  write_native_int(func.last_line_defined);
  write_byte(func.num_params);
  write_byte(func.is_var_arg);
  write_byte(func.max_stack_size);

  // Instructions
  write_native_int(func.num_instructions);
  for (int i = 0; i < func.num_instructions; i++)
    write_lua_instruction(func.instructions[i]);

  // Constants
  write_native_int(func.num_constants);
  for (int i = 0; i < func.num_constants; i++) {
    tvalue &tv = func.constants[i];
    // TODO: this needs to be sanitized according to ldump.c
    write_byte(tv.tag_type);
    tag t = get_tag_from_tag_type(tv.tag_type);
    if (t == tag::BOOL) {
      write_byte(tv.data.get<bool>() ? 1 : 0);
    } else if (tv.tag_type == construct_tag_type(tag::NUMBER, variant::NUM_FLOAT)) {
      write_lua_number(tv.data.get<lua_number>());
    } else if (tv.tag_type == construct_tag_type(tag::NUMBER, variant::NUM_INT)) {
      write_lua_integer(tv.data.get<lua_integer>());
    } else if (t == tag::STRING) {
      write_lua_string(*this, tv.data.get<tvalue::string_vec>());
    }
  }

  // Upvals
  write_native_int(func.num_upvalues);
  for (int i = 0; i < func.num_upvalues; i++) {
    write_byte(func.upvalues[i].instack ? 1 : 0);
    write_byte(func.upvalues[i].idx);
  }

  // Protos
  write_native_int(func.num_protos);
  for (int i = 0; i < func.num_protos; i++) {
    write_function(*func.protos[i]);
  }

  // Debugging (shims, to make luac happy)
  write_native_int(0); // opcode mapping
  write_native_int(0); // loc var mapping
  write_native_int(0); // upval mapping
}

void bytecode_writer::write_byte(uint8_t b) {
  _stream.put(b);
}

void bytecode_writer::write_block(uint8_t *buf, size_t count) {
  _stream.write((char *)buf, count);
}

void bytecode_writer::write_native_int(int i) {
  write_numeric<int>(i, _stream, _target_arch.little, _target_arch.sizeof_int, _sys_arch.little);
}

void bytecode_writer::write_sizet(size_t s) {
  write_numeric<size_t>(s, _stream, _target_arch.little, _target_arch.sizeof_sizet, _sys_arch.little);
}

void bytecode_writer::write_lua_instruction(lua_instruction inst) {
  write_numeric<lua_instruction>(inst, _stream, _target_arch.little, _target_arch.sizeof_instruction, _sys_arch.little);
}

void bytecode_writer::write_lua_integer(lua_integer i) {
  write_numeric<lua_integer>(i, _stream, _target_arch.little, _target_arch.sizeof_lua_integer, _sys_arch.little);
}

void bytecode_writer::write_lua_number(lua_number n) {
  write_numeric<lua_number>(n, _stream, _target_arch.little, _target_arch.sizeof_lua_number, _sys_arch.little);
}
#endif