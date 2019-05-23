#include "grpl/robotlua/bytecode.h"

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

// bytecode_constant::~bytecode_constant() {
//   // Dealloc the small vector if required
// }

bytecode_reader::bytecode_reader(std::istream &s) : _stream(s) { }

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
  read_lua_string(arch, func.source);
  func.line_defined = read_native_int(arch);
  func.last_line_defined = read_native_int(arch);
  func.num_params = read_byte();
  func.is_var_arg = read_byte();
  func.max_stack_size = read_byte();

  func.num_instructions = read_native_int(arch);
  for (int i = 0; i < func.num_instructions; i++) {
    func.instructions.emplace_back(read_lua_instruction(arch));
  }

  func.num_constants = read_native_int(arch);
  for (int i = 0; i < func.num_constants; i++) {

  }

  func.num_upvalues = read_native_int(arch);
  for (int i = 0; i < func.num_upvalues; i++) {
    func.upvalues.emplace_back(read_byte(), read_byte());
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
  // TODO: Change reads to not store any information.
  // TODO: Does the C api need any info from this? Var names?

  small_vector<char, 32> tmp_vec;
  int num_opcode_map = read_native_int(arch);
  for (int i = 0; i < num_opcode_map; i++)
    read_native_int(arch);

  int num_locvar_map = read_native_int(arch);
  for (int i = 0; i < num_locvar_map; i++) {
    read_lua_string(arch, tmp_vec);
    read_native_int(arch);
    read_native_int(arch);
    tmp_vec.clear();
  }

  int num_upval_map = read_native_int(arch);
  for (int i = 0; i < num_upval_map; i++) {
    read_lua_string(arch, tmp_vec);
    tmp_vec.clear();
  }
}

void bytecode_reader::read_constant(bytecode_architecture arch, bytecode_constant &c) {
  uint8_t type_tag = read_byte();

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
  if (arch.little == _sys_arch.little && arch.sizeof_sizet == _sys_arch.sizeof_sizet) {
    size_t size;
    _stream.read((char *)&size, sizeof(size_t));
    return size;
  } else {
    // TODO:
    return 0;
  }
}

uint8_t bytecode_reader::read_byte() {
  return _stream.get();
}

void bytecode_reader::read_block(uint8_t *out, size_t count) {
  for (size_t i = 0; i < count; i++)
    _stream.get(*(char *)(out + i));
}

void bytecode_reader::read_lua_string(bytecode_architecture arch, base_small_vector<char> &buffer) {
  uint8_t b_size = read_byte();
  if (b_size < 0xFF) {
    // Small string
    for (size_t i = 0; i < b_size - 1; i++) 
      buffer.emplace_back((char)read_byte());
  } else {
    // Long string
    size_t size = read_sizet(arch);
    for (size_t i = 0; i < size - 1; i++)
      buffer.emplace_back((char)read_byte());
  }
}

// TODO: Can generify this?
lua_instruction bytecode_reader::read_lua_instruction(bytecode_architecture arch) {
  if (arch.little == _sys_arch.little && arch.sizeof_instruction == _sys_arch.sizeof_instruction) {
    lua_instruction ins;
    _stream.read((char *)&ins, sizeof(lua_instruction));
    return ins;
  } else {
    // TODO:
    return 0;
  }
}

lua_integer bytecode_reader::read_lua_integer(bytecode_architecture arch) {
  if (arch.little == _sys_arch.little && arch.sizeof_lua_integer == _sys_arch.sizeof_lua_integer) {
    // Read directly from stream
    lua_integer theint;
    _stream.read((char *)&theint, sizeof(lua_integer));
    return theint;
  } else {
    // Need magic
    // TODO:
    return 0;
  }
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