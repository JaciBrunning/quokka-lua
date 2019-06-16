#include "quokka/engine.h"

using namespace quokka::engine;

#include <fstream>
#include <iostream>
#include <time.h>

int main() {
  std::cout << "sizeof(vm) " << sizeof(quokka_vm) << std::endl;
  std::cout << "sizeof(lua_value) " << sizeof(lua_value) << std::endl;
  std::cout << "sizeof(lua_object) " << sizeof(lua_object) << std::endl;
  std::cout << "sizeof(lua_upval) " << sizeof(lua_upval) << std::endl;

  std::ifstream bytecode_in("luac.out");
  bytecode_reader reader(bytecode_in);

  bytecode_chunk chunk = reader.read_chunk();
  quokka_vm v(chunk);

  v.define_native_function("print", [](quokka_vm &v) {
    std::cout << tostring(v.argument(0)).c_str() << std::endl;
    return 0;
  });

  v.define_native_function("native_type", [](quokka_vm &vm) {
    std::visit([&vm](auto &&t) {
      using T = typename std::decay<decltype(t)>::type;
      vm.push(typeid(T).name());
    }, vm.argument(0));

    return 1;
  });
  
  object_view os = v.alloc_object();
  os->emplace<lua_table>().set("clock", v.alloc_native_function([](quokka_vm &v) {
    v.push( (double)(clock()) / (double)(CLOCKS_PER_SEC) );
    return 1;
  }));
  v.env().set("os", os);

  v.call();

  return 0;
}