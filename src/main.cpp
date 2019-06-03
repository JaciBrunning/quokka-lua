#include "quokka/engine.h"

using namespace quokka::engine;

#include <fstream>
#include <iostream>

int main() {
  std::ifstream bytecode_in("luac.out");
  bytecode_reader reader(bytecode_in);

  bytecode_chunk chunk;
  reader.read_chunk(chunk);

  vm v;
  v.load(chunk);

  v.define_native_function("print", [](vm &v) {
    tvalue &a = v.argument(0);
    if (a.data.is<tvalue::string_t>())
      std::cout << a.data.get<tvalue::string_t>().c_str() << std::endl;
    else
      std::cout << a.tonumber() << std::endl;
    return 0;
  });

  v.call(0, 0);

  v.push(v.env().get("test"));
  v.push(123);
  v.call(1, 2);
  std::cout << v.argument(0).tonumber() << " " << v.argument(1).tonumber() << std::endl;
  v.pop(2);

  return 0;
}