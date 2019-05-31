#include "jaci/robotlua.h"

using namespace jaci::robotlua;

#include <fstream>
#include <iostream>

int main() {
  std::ifstream bytecode_in("luac.out");
  bytecode_reader reader(bytecode_in);

  bytecode_chunk chunk;
  reader.read_chunk(chunk);

  vm v;
  v.load(chunk);

  v.define_native_function("test", [](vm &v) {
    tvalue &a = v.argument(0);
    std::cout << conv::tonumber2(a) << std::endl;
    v.push(12.5);
    return 1;
  });

  v.call_at(0, 0);

  return 0;
}