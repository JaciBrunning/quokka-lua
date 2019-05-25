#include "grpl/robotlua/vm.h"

using namespace grpl::robotlua;

vm::vm() : _pc(0) { }

void vm::load(bytecode_chunk &bytecode) {

}

void vm::execute() {
  opcode code = opcode_util::get_opcode(instruction);

  // Fetch opcode args
  uint8_t a = opcode_util::get_A(instruction);
  opcode_util::refreg b = opcode_util::get_B(instruction);
  opcode_util::refreg c = opcode_util::get_C(instruction);
  uint32_t bx = opcode_util::get_Ax(instruction);
  int32_t sbx = opcode_util::get_sBx(instruction);
  uint32_t ax = opcode_util::get_Ax(instruction);

  // Decode and execute opcode
  switch(code) {
    case opcode::OP_MOVE:
      break;
    case opcode::OP_LOADK:
      break;
    case opcode::OP_LOADKX:
      break;
    case opcode::OP_LOADBOOL:
      break;
    case opcode::OP_LOADNIL:
      break;
    case opcode::OP_GETUPVAL:
      break;
    case opcode::OP_GETTABUP:
      break;
    case opcode::OP_GETTABLE:
      break;
    case opcode::OP_SETTABUP:
      break;
    case opcode::OP_SETUPVAL:
      break;
    case opcode::OP_SETTABLE:
      break;
    case opcode::OP_NEWTABLE:
      break;
    case opcode::OP_SELF:
      break;
    case opcode::OP_ADD:
      break;
    case opcode::OP_SUB:
      break;
    case opcode::OP_MUL:
      break;
    case opcode::OP_MOD:
      break;
    case opcode::OP_POW:
      break;
    case opcode::OP_DIV:
      break;
    case opcode::OP_IDIV:
      break;
    case opcode::OP_BAND:
      break;
    case opcode::OP_BOR:
      break;
    case opcode::OP_BXOR:
      break;
    case opcode::OP_SHL:
      break;
    case opcode::OP_SHR:
      break;
    case opcode::OP_UNM:
      break;
    case opcode::OP_BNOT:
      break;
    case opcode::OP_NOT:
      break;
    case opcode::OP_LEN:
      break;
    case opcode::OP_CONCAT:
      break;
    case opcode::OP_JMP:
      break;
    case opcode::OP_EQ:
      break;
    case opcode::OP_LT:
      break;
    case opcode::OP_LE:
      break;
    case opcode::OP_TEST:
      break;
    case opcode::OP_TESTSET:
      break;
    case opcode::OP_CALL:
      break;
    case opcode::OP_TAILCALL:
      break;
    case opcode::OP_RETURN:
      break;
    case opcode::OP_FORLOOP:
      break;
    case opcode::OP_FORPREP:
      break;
    case opcode::OP_TFORCALL:
      break;
    case opcode::OP_TFORLOOP:
      break;
    case opcode::OP_SETLIST:
      break;
    case opcode::OP_CLOSURE:
      break;
    case opcode::OP_VARARG:
      break;
    case opcode::OP_EXTRAARG:
      break;
    default:
      /* Not supported */
      break;
  }
}