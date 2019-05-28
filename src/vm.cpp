#include "grpl/robotlua/vm.h"

using namespace grpl::robotlua;

vm::vm() {
  // Load distinguished env.
  _distinguished_env.tag_type = construct_tag_type(tag::TABLE);
  object_store_ref objstore = alloc_object();
  (*objstore)->new_table();
  _distinguished_env = tvalue(objstore);
}

void vm::load(bytecode_chunk &bytecode) {
  // TODO: Check header

  // Add prototype to _rootprotos
  size_t proto_slot = first_avail_idx(_rootprotos);
  _rootprotos[proto_slot].emplace<bytecode_prototype>(bytecode.root_func);
  // Add closure to top of register stack
  object_store_ref root_lua_func = alloc_object();
  (*root_lua_func)->new_lclosure().proto_idx = proto_slot;
  _registers.emplace_back(root_lua_func);
  // Assign upvals
  // bytecode.num_upvals and bytecode.root_proto.num_upvalues are always the same
  for (size_t i = 0; i < bytecode.num_upvalues; i++) {
    size_t next_slot = first_avail_idx(_upvals);
    lua_upval &luv = _upvals[next_slot].emplace<lua_upval>();
    // luv.value.emplace<tvalue>();  // nil
  }
}

object_store_ref vm::alloc_object() {
  for (size_t i = 0; i < _objects.size(); i++) {
    if (_objects[i].is_free)
      return object_store_ref(&_objects, i);
  }
  // Didn't find a free slot, emplace an object and use that.
  _objects.emplace_back();
  return object_store_ref(&_objects, _objects.size() - 1);
}

// void vm::execute() {
//   opcode code = opcode_util::get_opcode(instruction);

//   // Fetch opcode args
//   uint8_t a = opcode_util::get_A(instruction);
//   opcode_util::refreg b = opcode_util::get_B(instruction);
//   opcode_util::refreg c = opcode_util::get_C(instruction);
//   uint32_t bx = opcode_util::get_Ax(instruction);
//   int32_t sbx = opcode_util::get_sBx(instruction);
//   uint32_t ax = opcode_util::get_Ax(instruction);

//   // Decode and execute opcode
//   switch(code) {
//     case opcode::OP_MOVE:
//       break;
//     case opcode::OP_LOADK:
//       break;
//     case opcode::OP_LOADKX:
//       break;
//     case opcode::OP_LOADBOOL:
//       break;
//     case opcode::OP_LOADNIL:
//       break;
//     case opcode::OP_GETUPVAL:
//       break;
//     case opcode::OP_GETTABUP:
//       break;
//     case opcode::OP_GETTABLE:
//       break;
//     case opcode::OP_SETTABUP:
//       break;
//     case opcode::OP_SETUPVAL:
//       break;
//     case opcode::OP_SETTABLE:
//       break;
//     case opcode::OP_NEWTABLE:
//       break;
//     case opcode::OP_SELF:
//       break;
//     case opcode::OP_ADD:
//       break;
//     case opcode::OP_SUB:
//       break;
//     case opcode::OP_MUL:
//       break;
//     case opcode::OP_MOD:
//       break;
//     case opcode::OP_POW:
//       break;
//     case opcode::OP_DIV:
//       break;
//     case opcode::OP_IDIV:
//       break;
//     case opcode::OP_BAND:
//       break;
//     case opcode::OP_BOR:
//       break;
//     case opcode::OP_BXOR:
//       break;
//     case opcode::OP_SHL:
//       break;
//     case opcode::OP_SHR:
//       break;
//     case opcode::OP_UNM:
//       break;
//     case opcode::OP_BNOT:
//       break;
//     case opcode::OP_NOT:
//       break;
//     case opcode::OP_LEN:
//       break;
//     case opcode::OP_CONCAT:
//       break;
//     case opcode::OP_JMP:
//       break;
//     case opcode::OP_EQ:
//       break;
//     case opcode::OP_LT:
//       break;
//     case opcode::OP_LE:
//       break;
//     case opcode::OP_TEST:
//       break;
//     case opcode::OP_TESTSET:
//       break;
//     case opcode::OP_CALL:
//       break;
//     case opcode::OP_TAILCALL:
//       break;
//     case opcode::OP_RETURN:
//       break;
//     case opcode::OP_FORLOOP:
//       break;
//     case opcode::OP_FORPREP:
//       break;
//     case opcode::OP_TFORCALL:
//       break;
//     case opcode::OP_TFORLOOP:
//       break;
//     case opcode::OP_SETLIST:
//       break;
//     case opcode::OP_CLOSURE:
//       break;
//     case opcode::OP_VARARG:
//       break;
//     case opcode::OP_EXTRAARG:
//       break;
//     default:
//       /* Not supported */
//       break;
//   }
// }