Quokka Lua Engine
====

The Quokka Lua Engine (Quokka LE) is a runtime environment and transpiler for Lua bytecode.  

Quokka LE is:
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
   * print("Hello world, from Quokka LE!")
   */
  std::ifstream bytecode_file("my_lua_bytecode_file");
  bytecode_reader reader(bytecode_file);

  // Read a Lua program and load it into the VM.
  bytecode_chunk lua_program = reader.read_chunk();
  quokka_vm v(lua_program);

  // Define a C++ function, making it available to the Lua program
  v.define_native_function("print", [](vm &v) {
    std::cout << v.argument(0).tostring().c_str() << std::endl;
    return 0; // No return values
  });

  // Run the lua program
  v.call();
  return 0;
}
```

## Differences from the PUC-RIO Lua VM
| Feature | Quokka | PUC-RIO (lua.org) | Notes |
| - | - | - | - |
| API | C++17 | C99 | Quokka is built on C++11, and uses C++17 constructs in order to implement a modern VM. |
| VM Registers and Call Stack | Small Vector | Vector | Quokka uses Small Vectors, which are pre-allocated on the stack up to a certain size. When the vector needs to grow, it is moved onto the heap. This optimizes performance and memory footprint, and is the main advantage of Quokka. |
| Bytecode Interpreter | Any source architecture | Source architecture must match running architecture | Quokka can understand bytecodes compiled on different architectures, although if the architectures do not match (i.e. a 64-bit program running on 32-bit), performance during bytecode parsing can drop. Quokka offers the ability to 'transpile' bytecode between architectures, allowing you to "cross compile" lua programs. As a bonus, transpiled bytecodes will run on a standard PUC-RIO installation. |
| Threads | Not implemented | Present | Quokka is optimized for embedded platforms, with small memory regions. Threading implementations, including coroutines, yields, etc, all take up a large amount of binary space and are often not required in embedded systems. Threading requirements can be met using multiple instances of `quokka_vm` and handling synchronization through native function declarations. |
| Metatables | Not implemented | Present | Metatables are not present in Quokka at this time. They have been forgone in the interest of size optimization. |
| Debugging Information | Removed | Present\* | Quokka discards debugging information. \*: PUC-RIO can strip bytecode information with `luac -s`, which is the equivilent of the Quokka bytecode parser's approach to Debugging information |

## TODO
- Can we make lua_value just a std::variant, and do all the funcs outside? Could improve use of the std library (e.g. std::visit)