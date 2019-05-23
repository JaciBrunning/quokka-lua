#include "grpl/robotlua.h"

using namespace grpl::robotlua;

#include <fstream>
#include <iostream>

int main() {
  std::fstream bytecode("luac.out");
  bytecode_reader reader(bytecode);
  // bytecode_header header;
  // reader.read_header(header);
// 
  // std::cout << "Header: " << std::endl;
  // std::cout << "  Version: " << std::hex << (int)header.version << std::endl;
  // std::cout << "  Format: " << std::hex << (int)header.format << std::endl;
  // std::cout << "  Arch: " << std::endl;
  // std::cout << "    Little? " << (int)header.arch.little << std::endl;
  // std::cout << "    Int: "      << (int) header.arch.sizeof_int << std::endl;
  // std::cout << "    SizeT: "    << (int) header.arch.sizeof_sizet << std::endl;
  // std::cout << "    Instr: "    << (int) header.arch.sizeof_instruction << std::endl;
  // std::cout << "    Lua Int: "  << (int) header.arch.sizeof_lua_integer << std::endl;
  // std::cout << "    Lua Num: "  << (int) header.arch.sizeof_lua_number << std::endl;
  // std::cout << "  Int: " << header.linteger << std::endl;
  // std::cout << "  Num: " << header.lnumber << std::endl;

  bytecode_chunk chunk;
  reader.read_chunk(chunk);

  return 0;
}