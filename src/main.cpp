#include "grpl/robotlua.h"

using namespace grpl::robotlua;

#include <fstream>
#include <iostream>

int main() {
  std::ifstream bytecode_in("luac.out");
  std::ofstream bytecode_out("luac.out.transpiled");
  bytecode_reader reader(bytecode_in);

  bytecode_chunk chunk;
  reader.read_chunk(chunk);

  bytecode_writer writer(bytecode_out, chunk.header.arch);
  writer.write_chunk(chunk);

  return 0;
}