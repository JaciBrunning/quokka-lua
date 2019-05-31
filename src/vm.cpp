#include "jaci/robotlua/vm.h"
#include "jaci/robotlua/opcodes.h"

#include <math.h>

using namespace jaci::robotlua;

vm::vm() {
  // Load distinguished env.
  _distinguished_env.tag_type = construct_tag_type(tag::TABLE);
  object_store_ref objstore = alloc_object();
  (*objstore)->table();
  _distinguished_env = tvalue(objstore);
}

void vm::load(bytecode_chunk &bytecode) {
  // TODO: Check header

  // Add prototype to _rootprotos
  // Add closure to top of register stack (the root function)
  object_store_ref root_lua_func = alloc_object();
  (*root_lua_func)->lclosure().proto = &bytecode.root_func;
  _registers.emplace_back(root_lua_func);
  // Init upvals (closed)
  // bytecode.num_upvals and bytecode.root_proto.num_upvalues are always the same
  for (size_t i = 0; i < bytecode.num_upvalues; i++) {
    upval_ref upv = alloc_upval();
    (*upv)->value.emplace<tvalue>();  // nil

    if (i == 0) {
      // Is _ENV
      // Distinguished env, apply to root function
      (*upv)->value.emplace<tvalue>(_distinguished_env);
      (*root_lua_func)->lclosure().upval_refs.emplace(0, upv);
    }
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

upval_ref vm::alloc_upval() {
  for (size_t i = 0; i < _upvals.size(); i++) {
    if (_upvals[i].is_free)
      return upval_ref(&_upvals, i);
  }
  // Didn't find a free slot, emplace an upval and use that
  _upvals.emplace_back();
  return upval_ref(&_upvals, _upvals.size() - 1);
}

void vm::call(size_t nargs, int nreturn) {

}

void vm::call_at(size_t func_stack_idx, int nreturn) {
  if (!precall(func_stack_idx, nreturn))
    execute();
}

bool vm::precall(size_t func_stack_idx, int nreturn) {
  // TODO: Meta method, see ldo.c luaD_precall
  object_store_ref func_ref = _registers[func_stack_idx].data.get<object_store_ref>();
  lua_closure &closure = (*func_ref)->data.get<lua_closure>();
  if (closure.impl.is<lua_lclosure>()) {
    // Lua closure
    lua_lclosure &lcl = closure.impl.get<lua_lclosure>();
    bytecode_prototype *proto = lcl.proto;
    // Actual number of arguments, not necessarily what's required
    size_t nargs = _registers.size() - func_stack_idx - 1;
    // Grow the stack pre-emptively
    _registers.reserve(_registers.size() + proto->max_stack_size);
    size_t base;
    if (proto->is_var_arg) {
      // Adjust varargs....
      size_t i;
      size_t fixed = _registers.size() - nargs;
      base = _registers.size();
      for (i = 0; i < proto->num_params && i < nargs; i++) {
        // Copy to stack
        _registers.emplace_back(_registers[fixed + i]);
        _registers.emplace(fixed + i);  // Set original to nil
      }
      for (; i < proto->num_params; i++) {
        // Fill the reaminder of params with nil
        _registers.emplace_back();
      }
    } else {
      for (; nargs < proto->num_params; nargs++) {
        // Fill the remainder of the params with nil
        _registers.emplace_back();
      }
      base = func_stack_idx + 1;
    }
    // Push a new call frame.
    lua_call &ci = _callinfo.emplace_back();
    ci.callstatus = CALL_STATUS_LUA;
    ci.func_idx = func_stack_idx;
    ci.numresults = nreturn;
    // TODO: Need top?
    // ci.top = base + proto->max_stack_size;
    // NOTE: Am I missing setting registers top?
    ci.info.lua.base = base;
    ci.info.lua.pc = &proto->instructions[0];   // TODO: Should this be a pointer?
    return false;
  } else if (closure.impl.is<lua_native_closure>()) {
    // Native closure
    lua_native_closure &ncl = closure.impl.get<lua_native_closure>();
    // Push a new call frame
    lua_call &ci = _callinfo.emplace_back();
    ci.callstatus = 0;
    ci.func_idx = func_stack_idx;
    ci.numresults = nreturn;
    // Call function
    int n = ncl.func(*this);
    postcall(_registers.size() - n, n);
    return true;
  }
}

#define RL_VM_PC(ci_ref) ((*ci_ref)->info.lua.pc)
// Obtain an upvalue
#define RL_VM_UPV(i, target) { \
  lua_upval *upv_ = (*cl_ref)->lclosure().upval_refs[i].get(); \
  target = upv_->value.is<size_t>() ? &_registers[upv_->value.get<size_t>()] : &upv_->value.get<tvalue>(); }
// Decode a B or C register, using a constant value or stack value where appropriate
#define RL_VM_RK(v) (opcode_util::is_const(v) ? proto->constants[opcode_util::val(v)] : _registers[base + opcode_util::val(v)])

void vm::execute() {
  _callinfo[_callinfo.size() - 1].callstatus |= CALL_STATUS_FRESH;
 new_call:
  ;
  call_ref ci_ref(&_callinfo, _callinfo.size() - 1);
  reg_ref cl_reg_ref(&_registers, (*ci_ref)->func_idx);
  object_store_ref cl_ref = (*cl_reg_ref)->data.get<object_store_ref>();
  bytecode_prototype *proto = (*cl_ref)->lclosure().proto;
  size_t base = (*ci_ref)->info.lua.base;

  while (true) {
    lua_instruction instruction = *(RL_VM_PC(ci_ref)++);
    opcode code = opcode_util::get_opcode(instruction);
    uint8_t arg_a = opcode_util::get_A(instruction);
    size_t ra = base + (size_t)arg_a;
    unsigned int arg_b = opcode_util::get_B(instruction);
    unsigned int arg_c = opcode_util::get_C(instruction);
    uint32_t bx = opcode_util::get_Bx(instruction);

    switch(code) {
      case opcode::OP_MOVE:
        // Move R(B) to R(A)
        _registers[ra] = _registers[base + opcode_util::val(arg_b)];
        break;
      case opcode::OP_LOADK:
        // Move K(Bx) to R(A)
        _registers[ra] = proto->constants[bx];
        break;
      case opcode::OP_LOADKX:
        // Move K(extra arg) to R(A)
        // Next instruction is extra arg, so we have to advance the instruction counter
        _registers[ra] = proto->constants[opcode_util::get_Ax(*(RL_VM_PC(ci_ref)++))];
        break;
      case opcode::OP_LOADBOOL:
        // Load (Bool)B into R(A), if C, pc++ (skip next instruction)
        _registers.emplace(ra, arg_b > 0);
        if (arg_c > 0)
          RL_VM_PC(ci_ref)++;
        break;
      case opcode::OP_LOADNIL:
        // R(A), R(A+1) .. R(A+B) = nil
        do {
          _registers.emplace(ra++); // set nil
        } while(arg_b--);
        break;
      case opcode::OP_GETUPVAL: {
        // R(A) = Upval[B]
        tvalue *tv;
        RL_VM_UPV(arg_b, tv);
        _registers.emplace(ra, *tv);
        break;
      }
      case opcode::OP_GETTABUP: {
        // R(A) = Upval[B][RK(C)]
        tvalue *tuv;
        RL_VM_UPV(arg_b, tuv);
        lua_table &table = tuv->obj().get()->table();
        _registers[ra] = table.get(RL_VM_RK(arg_c));
        break;
      }
      case opcode::OP_GETTABLE: {
        // R(A) = R(B)[RK(C)]
        lua_table &table = _registers[base + opcode_util::val(arg_b)].obj().get()->table();
        _registers[ra] = table.get(RL_VM_RK(arg_c));
        break;
      }
      case opcode::OP_SETTABUP: {
        // Upval[A][RK(B)] = RK(C)
        tvalue *tupv;
        RL_VM_UPV(arg_a, tupv);
        tupv->obj().get()->table().set(RL_VM_RK(arg_b), RL_VM_RK(arg_c));
        break;
      }
      case opcode::OP_SETUPVAL: {
        // Upval[B] = R(A)
        // NOTE: Assigns to stack value
        lua_upval *upv = (*cl_ref)->lclosure().upval_refs[arg_a].get();
        upv->value.emplace<size_t>(ra);
        break;
      }
      case opcode::OP_SETTABLE:
        // R(A)[RK(B)] = RK(C)
        _registers[ra].obj().get()->table().set(RL_VM_RK(arg_b), RL_VM_RK(arg_c));
        break;
      case opcode::OP_NEWTABLE: {
        // R(A) = {} (size = B,C)
        // TODO: B is size of array, C is size of hash. I don't think we need this
        object_store_ref objref = alloc_object();
        (*objref)->table();
        _registers.emplace(ra, objref);
        break;
      }
      case opcode::OP_SELF: {
        // R(A + 1) = R(B); R(A) = R(B)[RK(C)]
        _registers[ra + 1] = _registers[base + opcode_util::val(arg_b)];
        lua_table &table = _registers[base + opcode_util::val(arg_b)].obj().get()->table();
        _registers.emplace(ra, table.get(RL_VM_RK(arg_c)));
        break;
      }
      case opcode::OP_ADD: {
        // R(A) = RK(B) + RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() + nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        } else if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, lnb + lnc);
        }
        break;
      }
      case opcode::OP_SUB: {
        // R(A) = RK(B) - RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() - nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        } else if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, lnb - lnc);
        }
        break;
      }
      case opcode::OP_MUL: {
        // R(A) = RK(B) * RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() * nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        } else if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, lnb * lnc);
        }
        break;
      }
      case opcode::OP_MOD: {
        // R(A) = RK(B) % RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() % nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        } else if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, fmod(lnb, lnc));
        }
        break;
      }
      case opcode::OP_POW: {
        // R(A) = RK(B) ^ RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, pow(lnb, lnc));
        }
        break;
      }
      case opcode::OP_DIV: {
        // R(A) = RK(B) / RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() / nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        } else if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, lnb / lnc);
        }
        break;
      }
      case opcode::OP_IDIV: {
        // R(A) = RK(B) // RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        lua_number lnb, lnc;
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() / nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        } else if (conv::tonumber(nb, lnb) && conv::tonumber(nc, lnc)) {
          _registers.emplace(ra, (lua_integer)(lnb / lnc));
        }
        break;
      }
      case opcode::OP_BAND: {
        // R(A) = RK(B) + RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() & nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        }
        break;
      }
      case opcode::OP_BOR: {
        // R(A) = RK(B) | RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() | nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        }
        break;
      }
      case opcode::OP_BXOR: {
        // R(A) = RK(B) ~ RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() ^ nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        }
        break;
      }
      case opcode::OP_SHL: {
        // R(A) = RK(B) << RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() << nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        }
        break;
      }
      case opcode::OP_SHR: {
        // R(A) = RK(B) >> RK(C)
        tvalue &nb = RL_VM_RK(arg_b);
        tvalue &nc = RL_VM_RK(arg_c);
        if (nb.data.is<lua_integer>() && nc.data.is<lua_integer>()) {
          lua_integer result = nb.data.get<lua_integer>() >> nc.data.get<lua_integer>();
          _registers.emplace(ra, result);
        }
        break;
      }
      case opcode::OP_UNM: {
        // R(A) = -R(B)
        tvalue &n = _registers[base + opcode_util::val(arg_b)];
        lua_number ln;
        if (n.data.is<lua_integer>()) {
          _registers.emplace(ra, -n.data.get<lua_integer>());
        } else if (conv::tonumber(n, ln)) {
          _registers.emplace(ra, -ln);
        }
        break;
      }
      case opcode::OP_BNOT: {
        // R(A) = ~R(B)
        tvalue &n = _registers[base + opcode_util::val(arg_b)];
        lua_integer li;
        if (conv::tointeger(n, li)) {
          _registers[ra] = ~li;
        }
        break;
      }
      case opcode::OP_NOT: {
        // R(A) = not R(B)
        tvalue &n = _registers[base + opcode_util::val(arg_b)];
        _registers.emplace(ra, n.is_falsey());
        break;
      }
      case opcode::OP_LEN: {
        // R(A) = length of R(B)
        tvalue &n = _registers[base + opcode_util::val(arg_b)];
        if (n.data.is<tvalue::string_vec>()) {
          _registers.emplace(ra, (lua_integer) n.data.get<tvalue::string_vec>().size());
        } else if (n.data.is<object_store_ref>()) {
          object_store_ref o = n.obj();
          if (o.get()->data.is<lua_table>()) {
            _registers.emplace(ra, (lua_integer) o.get()->table().entries.size());
          }
        }
        break;
      }
      case opcode::OP_CONCAT:
        // TODO:
        break;
      case opcode::OP_JMP: {
        // pc += sBx; if (A) close all upvals >= R(A - 1)
        if (arg_a != 0) {
          // Close upvals
          close_upvals(ra - 1);
        }
        RL_VM_PC(ci_ref) += opcode_util::get_sBx(instruction);
        break;
      }
      case opcode::OP_EQ: {
        // if ((RK(B) == RK(C)) ~= A) then pc++ (skip jmp), otherwise do next jump
        if ((RL_VM_RK(arg_b) == RL_VM_RK(arg_c)) != arg_a) {
          RL_VM_PC(ci_ref)++;
        } else {
          // Continue to next instruction (jmp)
        }
        break;
      }
      case opcode::OP_LT:
        // if ((RK(B) <  RK(C)) ~= A) then pc++ (skip jmp), otherwise do next jump
        // TODO:
        break;
      case opcode::OP_LE:
        // if ((RK(B) <= RK(C)) ~= A) then pc++ (skip jmp), otherwise do next jump
        // TODO:
        break;
      case opcode::OP_TEST: {
        // if not (R(A) <=> C) then pc++
        tvalue &ta = _registers[ra];
        if (arg_c ? ta.is_falsey() : !ta.is_falsey()) {
          RL_VM_PC(ci_ref)++;
        } else {
          // Continue to next instruction (jmp)
        }
        break;
      }
      case opcode::OP_TESTSET: {
        // if (R(B) <=> C) then R(A) = R(B) else pc++
        tvalue &tb = _registers[base + opcode_util::val(arg_b)];
        if (arg_c ? tb.is_falsey() : !tb.is_falsey()) {
          RL_VM_PC(ci_ref)++;
        } else {
          // R(A) = R(B), do next jump
          _registers.emplace(ra, tb);
          // Continue to next instruction (jmp)
        }
        break;
      }
      case opcode::OP_CALL: {
        int nresults = arg_c - 1;
        if (arg_b != 0) {
          // Set new top? This should be done already.
        }
        if (precall(ra, nresults)) {
          // C Func
        } else {
          // Lua Func
          goto new_call;
        }
        break;
      }
      case opcode::OP_TAILCALL:
        break;
      case opcode::OP_RETURN: {
        if ((*cl_ref)->lclosure().proto->num_protos > 0) {
          // Close upvals
          close_upvals(base);
        }
        bool bb = postcall(ra, (arg_b != 0 ? (arg_b - 1) : (_registers.size() - ra)));
        if ((*ci_ref)->callstatus & CALL_STATUS_FRESH)
          return;   // Invoked externally, can just return
        else {
          // TODO: I don't think I need to adjust the top
          goto new_call;
        }
      }
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
      default:
        /* Not supported */
        break;
    }
  }
}

bool vm::postcall(size_t first_result_idx, int nreturn) {
  lua_call &ci_last = _callinfo[_callinfo.size() - 1];
  _callinfo.chop(_callinfo.size() - 1); // Chop the top (pop the top frame)
  
  size_t res = ci_last.func_idx;
  int wanted = ci_last.numresults;
  // Move results
  switch (wanted) {
    case 0: break;
    case 1: {
      // Single return
      if (nreturn == 0) {
        // Set first result to nil
        _registers.emplace(first_result_idx);
      }
      _registers.emplace(res, _registers[first_result_idx]);
      break;
    }
    case -1: {
      // Multiple returns
      for (size_t i = 0; i < nreturn; i++)
        _registers.emplace(res + i, _registers[first_result_idx + i]);
      _registers.chop(res + nreturn);
      return false;
    }
    default: {
      size_t i;
      if (wanted <= nreturn) {
        // Have enough (or more than enough) results
        for (i = 0; i < wanted; i++)
          _registers.emplace(res + i, _registers[first_result_idx + i]);
      } else {
        // Not enough results, pad with nils
        for (i = 0; i < nreturn; i++)
          _registers.emplace(res + i, _registers[first_result_idx + i]);
        for (; i < wanted; i++)
          _registers.emplace(res + i);  // nil
      }
      break;
    }
  }
  _registers.chop(res + wanted);
  return true;
}

void vm::close_upvals(size_t level) {
  for (size_t i = 0; i < _upvals.size(); i++) {
    lua_upval &uv = _upvals[i];
    if (!uv.is_free && uv.value.is<size_t>()) {
      size_t stack_idx = uv.value.get<size_t>();
      if (level <= stack_idx) {
        // Close upval
        uv.value.emplace<tvalue>(_registers[stack_idx]);
      }
    }
  }
}