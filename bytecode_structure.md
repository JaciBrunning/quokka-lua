Lua Bytecode Structure
=====

See `ldump.c` for Lua implementation.

Lua stores each file as a function (see: Dump). 

Note the distinction between Vector (a memcpy) and Array (a for loop containing other dumps)

Note the distinction between Int (a native `int` type), and Integer (`lua_Integer`, typedef'd to `long long`).

# TValues
TValues describe a struct containing a Value and a Type Tag.
Note the following from `lobject.h`: 
```c
typedef struct lua_TValue {
  // Expanded macro
  Value value_;
  int tt_;
} TValue;
```

`tt_` is a Type tag, describing the type of a TValue. The Type Tag itself is made up of two portions:
Bits 0-3 are for the Tag, and 4-5 for the Variant.
Bit 6 marks if the value is garbage collectable

| Tag  | Dec | Bin (Tag) | Variants | Notes | 
| ---- | --- | --- | ----- | --- |
| Nil  | 0   | 0000 | | |
| Bool | 1   | 0001 | | |
| Light User Data | 2 | 0010 | | |
| Number | 3 | 0011 | 00 - Float, 01 - Integer | |
| String | 4 | 0100 | 00 - Short String, 01 - Long String | |
| Table  | 5 | 0101 | | |
| Function | 6 | 0110 | 00 - Lua function, 01 - Light C Function, 10 - Regular C Function | |
| Userdata | 7 | 0111 | | |
| Thread | 8 | 1000 |  | |

For the purposes of Lua bytecode, the garbage collectable flag is not required.
Furthermore, the Tag types are only used for storing constant values, therefore the following tag types are not used in Bytecode.

| Tag | Reason |
| --- | ------ |
| Light User Data | Attached by the runtime. Has no use at compile time |
| User Data | Same as above |
| Function | Function has its own bytecode structure below, a function is never expressed as a constant in bytecode |
| Thread | Threads cannot be constants, and are created at runtime. |

# Bytecode Structures
These are the structures that occur in Lua bytecode. 'Dump' is the root type of a .lua file.

## Dump
| Type | Desc |
| ---- | ---- |
| Header | The header of the bytecode file |
| Byte | Size of the upvalues table |
| Function | The root function |

## Header (DumpHeader)
| Type | Desc |
| ---- | ---- |
| Literal | LUA_SIGNATURE: 4-byte literal: "\x1BLua" |
| Byte | LUAC_VERSION: 0x53 for Lua 5.3 |
| Byte | LUAC_FORMAT: 0x0 |
| Literal | LUAC_DATA: Literal 6 bytes "\x19\x93\r\n\x1A\n" |
| Byte | sizeof(int) |
| Byte | sizeof(size_t) |
| Byte | sizeof(Instruction) |
| Byte | sizeof(lua_Integer) |
| Byte | sizeof(lua_Number) |
| Integer | LUAC_INT: Literal 0x5678 (used for endian check) |
| Number| LUAC_NUM: Literal 370.5 |

## Function (DumpFunction)
| Type | Desc |
| ---- | ---- |
| String | NULL if stripped, Source otherwise |
| Int | Line defined |
| Int | Last line defined |
| Byte | Number of parameters |
| Byte | Is var arg? |
| Byte | Max stack size |
| Code | The code of the function |
| Constants | The constants of the function |
| Upvalues | The upvalues of the function |
| Protos | The protos of the function |
| Debug | Debugging info. Note this is present even if stripped |

## Code (DumpCode)
| Type | Desc |
| ---- | ---- |
| Int | Size of code |
| Vector\<Instruction>(size of code) | Vector of instructions for the code |

## Constants (DumpConstants)
| Type | Desc |
| ---- | ---- |
| Int | Number of constants |
| Array of constants |

### Constant: 
| Type | Desc |  
| ---- | ---- |
| Byte | Type Tag of constant (TValue), see TValues above |
| Data | Not present if nil, Byte if T_BOOLEAN, Number if NumberFLT, Integer if NumberINT, String if Short/LongSTR |

## Upvalues (DumpUpvalues)
| Type | Desc |
| ---- | ---- |
| Int | Number of upvalues |
| Array of upvalues |

### Upvalue:
| Type | Desc |
| ---- | ---- |
| Byte | Is the upval in stack? (local variable). If not, go another layer up until you find it. |
| Byte | Idx of upvalue, relative to the base of the calling function. |

## Protos (DumpProtos)
Protos are just another name for internal functions. In this case, this is really just a function table.

| Type | Desc |
| ---- | ---- |
| Int | Number of protos |
| Array of protos (functions) |

## Debug Info (DumpDebug)
| Type | Desc |
| ---- | ---- |
| Int | Number of entries in the opcode to source line table (0 if stripped) |
| Vector\<Int>(size: above) | The opcode to source line mapping table |
| Int | Number of loc var entries in the debugging table (0 if stripped) |
| Array of loc vars (see below) |
| Int | Number of upvalue entries in the debugging table (0 if stripped) |
| Array of Strings (upvalue names) |

### LocVar: 
| Type | Desc |
| ---- | ---- |
| String | Name of the loc var |
| Int | Start PC (first point the variable is 'active') |
| Int | End PC (first point the variable is 'dead')

# Helpers
## DumpVector
If size is 0, nothing is dumped, else it is written raw as it appears in memory.

## DumpVar (Byte, Int, Number, Integer)
Dumps the variable straight into memory (a size 1 DumpVector of the given type).

## DumpString
```
if NULL:  
  0x0: 0  
else:  
  if size + 1 (size + '\0') < 0xFF:  
    0x0: size + 1 (as byte)
    0x1: String without trailing \0 (note size - 1)  
  else  
    0x0: 0xFF  
    0x1: size + 1 (as size_t)
    0x1 + size_t: String without trailing \0 (note size - 1)  
```