#include "grpl/robotlua.h"

using namespace grpl::robotlua;

#include <fstream>
#include <iostream>

int fprint(vm &v) {

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
  oref.get()->native_closure().func = &fprint;
  v._distinguished_env.data.get<object_store_ref>().get()->table().set(tvalue(123), tvalue(oref));

  v.call_at(0, 0);

  return 0;
}