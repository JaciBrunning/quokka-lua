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

  object_store_ref oref = v.alloc_object();
  oref.get()->native_closure().func = [](vm &v) {
    tvalue &a = v.argument(0);
    tvalue &b = v.argument(1);

    std::cout << "Hello C from Lua!" << std::endl;
    std::cout << conv::tonumber2(a) << ", " << conv::tonumber2(b) << std::endl;
    return 0;
  };
  v._distinguished_env.data.get<object_store_ref>().get()->table().set(tvalue("print"), tvalue(oref));

  v.call_at(0, 0);

  return 0;
}