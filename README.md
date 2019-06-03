Quokka Lua Engine
====

The Quokka Lua Engine (Quokka LE) is a runtime environment and transpiler for Lua bytecode.  

Quokka LE is:
- Small
- Modern
- Cross Platform
- Stack Optimized

```cpp
#include "quokka/engine.h"
#include <iostream>

using namespace quokka::engine;

int main() {
  /**
   * # Lua program: 
   * my_func("Hello world, from Quokka LE!")
   */
  std::ifstream bytecode_file("my_lua_bytecode_file");
  bytecode_reader reader(bytecode_file);

  // Read a Lua program and load it into the VM.
  bytecode_chunk lua_program;
  reader.read_chunk(lua_program);

  quokka_vm v(lua_program);

  // Define a C++ function, making it available to the Lua program
  v.define_native_function("my_func", [](vm &v) {
    std::cout << v.argument(0).tostring().c_str() << std::endl;
    return 0; // No return values
  });

  // Run the lua program
  v.call();
  return 0;
}
```