#include "jaci/robotlua.h"

using namespace jaci::robotlua;

#include <fstream>
#include <iostream>

int fprint(vm &v) {
  std::cout << "Hello from C, called by Lua!" << std::endl;
}

int main() {
  std::ifstream bytecode_in("luac.out");
  std::ofstream bytecode_out("luac.out.transpiled");
  bytecode_reader reader(bytecode_in);

  bytecode_chunk chunk;
  reader.read_chunk(chunk);

  bytecode_writer writer(bytecode_out, chunk.header.arch);
  writer.write_chunk(chunk);

  vm v;
  v.load(chunk);

  v.define_native_function("print", [](vm &v) {
    tvalue &a = v.argument(0);
    std::cout << v.num_params() << std::endl;
    return 0;
  });

  v.call_at(0, 0);

  return 0;
}