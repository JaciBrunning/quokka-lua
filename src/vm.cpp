#include "quokka/engine/vm.h"
#include "quokka/engine/opcodes.h"

#include <math.h>

using namespace quokka::engine;

quokka_vm::quokka_vm() {
  // Load distinguished env.
  object_view objstore = alloc_object();
  lua_table table = (*objstore).data.emplace<lua_table>();
  table.set("__QUOKKA_LE__", "0.0.1");
  _distinguished_env = lua_value(objstore);
}

void quokka_vm::load(bytecode_chunk &bytecode) {
  // TODO: Check header

  // Add prototype to _rootprotos
  // Add closure to top of register stack (the root function)
  object_view root_lua_func = alloc_object();
  lua_closure &root_lua_f = (*root_lua_func).data.emplace<lua_closure>();
  root_lua_f.proto = &bytecode.root_func;
  _registers.emplace_back(root_lua_func);
  // Init upvals (closed)
  // bytecode.num_upvals and bytecode.root_proto.num_upvalues are always the same
  for (size_t i = 0; i < bytecode.num_upvalues; i++) {
    upval_view upv = alloc_upval();
    (*upv).value.emplace<lua_value>();  // nil

    if (i == 0) {
      // Is _ENV
      // Distinguished env, apply to root function
      (*upv).value.emplace<lua_value>(_distinguished_env);
      root_lua_f.upval_views.emplace(0, upv);
    }
  }
}

object_view quokka_vm::alloc_object() {
  for (size_t i = 0; i < _objects.size(); i++) {
    if (_objects[i].is_free)
      return object_view(&_objects, i);
  }
  // Didn't find a free slot, emplace an object and use that.
  _objects.emplace_back();
  return object_view(&_objects, _objects.size() - 1);
}

upval_view quokka_vm::alloc_upval() {
  for (size_t i = 0; i < _upvals.size(); i++) {
    if (_upvals[i].is_free)
      return upval_view(&_upvals, i);
  }
  // Didn't find a free slot, emplace an upval and use that
  _upvals.emplace_back();
  return upval_view(&_upvals, _upvals.size() - 1);
}

void quokka_vm::call(size_t nargs, int nreturn) {
  size_t stack_idx = _registers.size() - nargs - 1;
  if (!precall(stack_idx, nreturn))
    execute();
}

lua_value &quokka_vm::argument(int id) {
  size_t idx = _callinfo.size() > 0 ? (_callinfo.last().func_idx + id + 1) : id;
  return _registers[idx];
}

int quokka_vm::num_arguments() {
  return _registers.size() - (_callinfo.last().func_idx + 1);
}

void quokka_vm::push(const lua_value &v) {
  _registers.emplace_back(v);
}

lua_value &quokka_vm::pop() {
  lua_value &v = _registers.last();
  _registers.chop(_registers.size() - 1);
  return v;
}

void quokka_vm::pop(size_t num) {
  _registers.chop(_registers.size() - num);
}

lua_table &quokka_vm::env() {
  return table(_distinguished_env);
}

object_view quokka_vm::alloc_native_function(lua_native_closure::func_t f) {
  object_view r = alloc_object();
  (*r).data = lua_native_closure{f};
  return r;
}

void quokka_vm::define_native_function(const lua_value &key, lua_native_closure::func_t f) {
  env().set(key, alloc_native_function(f));
}

// PRIVATE //

bool quokka_vm::precall(size_t func_stack_idx, int nreturn) {
  // TODO: Meta method, see ldo.c luaD_precall
  // object_view func_ref = _registers[func_stack_idx].std::get<object_view>(data);
  //lua_closure &closure = (*func_ref)->std::get<lua_closure>(data);
  object_view obj = object(_registers[func_stack_idx]);
  if (is<lua_closure>((*obj).data)) {
    // Lua closure
    lua_closure &lcl = lua_func(obj);
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
    ci.info.lua.base = base;
    ci.info.lua.pc = &proto->instructions[0];   // TODO: Should this be a pointer?
    return false;
  } else if (is<lua_native_closure>((*obj).data)) {
    // Native closure
    lua_native_closure &ncl = native_func(obj);
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
  return true;
}

#define RL_quokka_vm_PC(ci_ref) ((*ci_ref).info.lua.pc)
// Obtain an upvalue
#define RL_quokka_vm_UPV(i, target, cl_ref) { \
  lua_upval *upv_ = lua_func(cl_ref).upval_views[i].get(); \
  target = is<size_t>(upv_->value) ? &_registers[std::get<size_t>(upv_->value)] : &std::get<lua_value>(upv_->value); }
// Decode a B or C register, using a constant value or stack value where appropriate
// Note that lua quokka_vm indexes constants from 1, but upvalues from 0 for some reason.
#define RL_quokka_vm_RK(v) (opcode_util::is_const(v) ? proto->constants[opcode_util::val(v)] : _registers[_base + opcode_util::val(v)])

void quokka_vm::execute() {
  _callinfo.last().callstatus |= CALL_STATUS_FRESH;
 new_call:
  ;
  _ci_ref = call_ref(&_callinfo, _callinfo.size() - 1);
  reg_ref cl_reg_ref(&_registers, (*_ci_ref).func_idx);
  _cl_ref = object(*cl_reg_ref);
  bytecode_prototype *proto = lua_func(_cl_ref).proto;
  _base = (*_ci_ref).info.lua.base;

  while (true) {
    _instruction = *(RL_quokka_vm_PC(_ci_ref)++);
    _code = opcode_util::get_opcode(_instruction);
    _arg_a = opcode_util::get_A(_instruction);
    _ra = _base + (size_t)_arg_a;
    _arg_b = opcode_util::get_B(_instruction);
    _arg_c = opcode_util::get_C(_instruction);

    switch(_code) {
      case opcode::OP_MOVE:
        // Move R(B) to R(A)
        _registers[_ra] = _registers[_base + opcode_util::val(_arg_b)];
        break;
      case opcode::OP_LOADK:
        // Move K(Bx) to R(A)
        _registers.emplace(_ra, proto->constants[opcode_util::get_Bx(_instruction)]);
        break;
      case opcode::OP_LOADKX:
        // Move K(extra arg) to R(A)
        // Next instruction is extra arg, so we have to advance the instruction counter
        _registers.emplace(_ra, proto->constants[opcode_util::get_Ax(*(RL_quokka_vm_PC(_ci_ref)++))]);
        break;
      case opcode::OP_LOADBOOL:
        // Load (Bool)B into R(A), if C, pc++ (skip next instruction)
        _registers.emplace(_ra, _arg_b > 0);
        if (_arg_c > 0)
          RL_quokka_vm_PC(_ci_ref)++;
        break;
      case opcode::OP_LOADNIL:
        // R(A), R(A+1) .. R(A+B) = nil
        do {
          _registers.emplace(_ra++); // set nil
        } while(_arg_b--);
        break;
      case opcode::OP_GETUPVAL: {
        // R(A) = Upval[B]
        lua_value *tv;
        RL_quokka_vm_UPV(_arg_b, tv, _cl_ref);
        _registers.emplace(_ra, *tv);
        break;
      }
      case opcode::OP_GETTABUP: {
        // R(A) = Upval[B][RK(C)]
        lua_value *tuv;
        RL_quokka_vm_UPV(_arg_b, tuv, _cl_ref);
        lua_table &t = table(*tuv);
        // _registers[ra] = table.get(RL_quokka_vm_RK(arg_c));
        _registers.emplace(_ra, t.get(RL_quokka_vm_RK(_arg_c)));
        break;
      }
      case opcode::OP_GETTABLE: {
        // R(A) = R(B)[RK(C)]
        lua_table &t = table(_registers[_base + opcode_util::val(_arg_b)]);
        _registers.emplace(_ra, t.get(RL_quokka_vm_RK(_arg_c)));
        break;
      }
      case opcode::OP_SETTABUP: {
        // Upval[A][RK(B)] = RK(C)
        lua_value *tupv;
        RL_quokka_vm_UPV(_arg_a, tupv, _cl_ref);
        table(*tupv).set(RL_quokka_vm_RK(_arg_b), RL_quokka_vm_RK(_arg_c));
        break;
      }
      case opcode::OP_SETUPVAL: {
        // Upval[B] = R(A)
        // NOTE: Assigns to stack value
        // lua_upval *upv = (*cl_ref)->lclosure().upval_views[arg_a].get();
        // upv->value.emplace<size_t>(ra);
        lua_value *tv;
        RL_quokka_vm_UPV(_arg_b, tv, _cl_ref);
        *tv = _registers[_ra];
        break;
      }
      case opcode::OP_SETTABLE:
        // R(A)[RK(B)] = RK(C)
        table(_registers[_ra]).set(RL_quokka_vm_RK(_arg_b), RL_quokka_vm_RK(_arg_c));
        break;
      case opcode::OP_NEWTABLE: {
        // R(A) = {} (size = B,C)
        object_view objref = alloc_object();
        (*objref).data.emplace<lua_table>();
        _registers.emplace(_ra, objref);
        break;
      }
      case opcode::OP_SELF: {
        // R(A + 1) = R(B); R(A) = R(B)[RK(C)]
        _registers[_ra + 1] = _registers[_base + opcode_util::val(_arg_b)];
        lua_table &t = table(_registers[_base + opcode_util::val(_arg_b)]);
        _registers.emplace(_ra, t.get(RL_quokka_vm_RK(_arg_c)));
        break;
      }
      case opcode::OP_ADD: {
        // R(A) = RK(B) + RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) + std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        } else if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, lnb + lnc);
        }
        break;
      }
      case opcode::OP_SUB: {
        // R(A) = RK(B) - RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) - std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        } else if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, lnb - lnc);
        }
        break;
      }
      case opcode::OP_MUL: {
        // R(A) = RK(B) * RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) * std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        } else if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, lnb * lnc);
        }
        break;
      }
      case opcode::OP_MOD: {
        // R(A) = RK(B) % RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) % std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        } else if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, fmod(lnb, lnc));
        }
        break;
      }
      case opcode::OP_POW: {
        // R(A) = RK(B) ^ RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, pow(lnb, lnc));
        }
        break;
      }
      case opcode::OP_DIV: {
        // R(A) = RK(B) / RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) / std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        } else if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, lnb / lnc);
        }
        break;
      }
      case opcode::OP_IDIV: {
        // R(A) = RK(B) // RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        lua_number lnb, lnc;
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) / std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        } else if (tonumber(nb, lnb) && tonumber(nc, lnc)) {
          _registers.emplace(_ra, (lua_integer)(lnb / lnc));
        }
        break;
      }
      case opcode::OP_BAND: {
        // R(A) = RK(B) + RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) & std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        }
        break;
      }
      case opcode::OP_BOR: {
        // R(A) = RK(B) | RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) | std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        }
        break;
      }
      case opcode::OP_BXOR: {
        // R(A) = RK(B) ~ RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) ^ std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        }
        break;
      }
      case opcode::OP_SHL: {
        // R(A) = RK(B) << RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) << std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        }
        break;
      }
      case opcode::OP_SHR: {
        // R(A) = RK(B) >> RK(C)
        lua_value &nb = RL_quokka_vm_RK(_arg_b);
        lua_value &nc = RL_quokka_vm_RK(_arg_c);
        if (is<lua_integer>(nb) && is<lua_integer>(nc)) {
          lua_integer result = std::get<lua_integer>(nb) >> std::get<lua_integer>(nc);
          _registers.emplace(_ra, result);
        }
        break;
      }
      case opcode::OP_UNM: {
        // R(A) = -R(B)
        lua_value &n = _registers[_base + opcode_util::val(_arg_b)];
        lua_number ln;
        if (is<lua_integer>(n)) {
          _registers.emplace(_ra, -std::get<lua_integer>(n));
        } else if (tonumber(n, ln)) {
          _registers.emplace(_ra, -ln);
        }
        break;
      }
      case opcode::OP_BNOT: {
        // R(A) = ~R(B)
        lua_value &n = _registers[_base + opcode_util::val(_arg_b)];
        lua_integer li;
        if (tointeger(n, li)) {
          // _registers[ra] = ~li;
          _registers.emplace(_ra, ~li);
        }
        break;
      }
      case opcode::OP_NOT: {
        // R(A) = not R(B)
        lua_value &n = _registers[_base + opcode_util::val(_arg_b)];
        _registers.emplace(_ra, falsey(n));
        break;
      }
      case opcode::OP_LEN: {
        // R(A) = length of R(B)
        lua_value &n = _registers[_base + opcode_util::val(_arg_b)];
        if (is<lua_string>(n)) {
          _registers.emplace(_ra, (lua_integer) std::get<lua_string>(n).length());
        } else if (is<object_view>(n)) {
          object_view o = object(n);
          if (is<lua_table>((*o).data)) {
            _registers.emplace(_ra, (lua_integer) table(o).entries.size());
          }
        }
        break;
      }
      case opcode::OP_CONCAT: {
        // R(A) = R(B) .. .. R(C)
        if (_ra != _base + _arg_b)
          _registers.emplace(_ra, _registers[_base + _arg_b]);
        for (size_t i = _base + _arg_b + 1; i <= _base + _arg_c; i++)
          std::get<lua_string>(_registers[_ra]).concat_str(tostring(_registers[i]));
        break;
      }
      case opcode::OP_JMP: {
        // pc += sBx; if (A) close all upvals >= R(A - 1)
        if (_arg_a != 0) {
          // Close upvals
          close_upvals(_ra - 1);
        }
        RL_quokka_vm_PC(_ci_ref) += opcode_util::get_sBx(_instruction);
        break;
      }
      case opcode::OP_EQ: {
        // if ((RK(B) == RK(C)) ~= A) then pc++ (skip jmp), otherwise do next jump
        if ((RL_quokka_vm_RK(_arg_b) == RL_quokka_vm_RK(_arg_c)) != _arg_a) {
          RL_quokka_vm_PC(_ci_ref)++;
        }
        break;
      }
      case opcode::OP_LT:
        // if ((RK(B) <  RK(C)) ~= A) then pc++ (skip jmp), otherwise do next jump
        if ((RL_quokka_vm_RK(_arg_b) < RL_quokka_vm_RK(_arg_c)) != _arg_a) {
          RL_quokka_vm_PC(_ci_ref)++;
        }
        break;
      case opcode::OP_LE:
        // if ((RK(B) <= RK(C)) ~= A) then pc++ (skip jmp), otherwise do next jump
        if ((RL_quokka_vm_RK(_arg_b) <= RL_quokka_vm_RK(_arg_c)) != _arg_a) {
          RL_quokka_vm_PC(_ci_ref)++;
        }
        break;
      case opcode::OP_TEST: {
        // if not (R(A) <=> C) then pc++
        lua_value &ta = _registers[_ra];
        if (_arg_c ? ta.is_falsey() : !ta.is_falsey()) {
          RL_quokka_vm_PC(_ci_ref)++;
        } else {
          // Continue to next instruction (jmp)
        }
        break;
      }
      case opcode::OP_TESTSET: {
        // if (R(B) <=> C) then R(A) = R(B) else pc++
        lua_value &tb = _registers[_base + opcode_util::val(_arg_b)];
        if (_arg_c ? tb.is_falsey() : !tb.is_falsey()) {
          RL_quokka_vm_PC(_ci_ref)++;
        } else {
          // R(A) = R(B), do next jump
          _registers.emplace(_ra, tb);
          // Continue to next instruction (jmp)
        }
        break;
      }
      case opcode::OP_CALL: {
        int nresults = _arg_c - 1;
        if (_arg_b != 0) {
          // Set new top? This should be done already.
        }
        if (!precall(_ra, nresults)) {
          // Lua Func
          goto new_call;
        }
        break;
      }
      case opcode::OP_TAILCALL: {
        // return R(A)(R(A+1) ... R(A+B-1))
        // -1 = multiret
        if (!precall(_ra, -1)) {
          // Lua function
          size_t oci_idx = _callinfo.size() - 2;
          lua_call &nci = _callinfo.last();                 // New, called frame
          lua_call &oci = _callinfo[oci_idx];  // Caller frame

          size_t lim = nci.info.lua.base + _registers[nci.func_idx].obj().get()->lua_func().proto->num_params;

          if ((*_cl_ref)->lua_func().proto->num_params > 0)
            close_upvals(oci.info.lua.base);
          
          // Move called frame into the caller
          for (int aux = 0; nci.func_idx + aux < lim; aux++) {
            _registers.emplace(oci.func_idx + aux, _registers[nci.func_idx + aux]);
          }

          oci.info.lua.base = oci.func_idx + (nci.info.lua.base - nci.func_idx);
          oci.info.lua.pc = nci.info.lua.pc;
          oci.callstatus |= CALL_STATUS_TAIL;
          // Remove new frame
          _callinfo.chop(oci_idx + 1);
          goto new_call;
        }
        break;
      }
      case opcode::OP_RETURN: {
        if ((*_cl_ref)->lua_func().proto->num_protos > 0) {
          // Close upvals
          close_upvals(_base);
        }
        postcall(_ra, (_arg_b != 0 ? (_arg_b - 1) : (_registers.size() - _ra)));
        if ((*_ci_ref)->callstatus & CALL_STATUS_FRESH)
          return;   // Invoked externally, can just return
        else {
          goto new_call;
        }
      }
      case opcode::OP_FORLOOP: {
        // R(A) += R(A+2); if R(A) <?= R(A+1) then { pc += sBx; R(A+3) = R(A) }
        if (is<lua_integer>(_registers[_ra].data)) {
          // integer loop
          lua_integer step = std::get<lua_integer>(_registers[_ra + 2].data);
          lua_integer idx = std::get<lua_integer>(_registers[_ra].data) + step;
          lua_integer limit = std::get<lua_integer>(_registers[_ra + 1].data);
          if ( (step > 0) ? (idx <= limit) : (limit <= idx) ) {
            // Jump pc by sBx
            RL_quokka_vm_PC(_ci_ref) += opcode_util::get_sBx(_instruction);
            _registers.emplace(_ra, idx);
            _registers.emplace(_ra + 3, idx);
          }
        } else {
          // floating point loop
          lua_number step = std::get<lua_number>(_registers[_ra + 2].data);
          lua_number idx = std::get<lua_number>(_registers[_ra].data) + step;
          lua_number limit = std::get<lua_number>(_registers[_ra + 1].data);
          if ( (step > 0) ? (idx <= limit) : (limit <= idx) ) {
            // Jump pc by sBx
            RL_quokka_vm_PC(_ci_ref) += opcode_util::get_sBx(_instruction);
            _registers.emplace(_ra, idx);
            _registers.emplace(_ra + 3, idx);
          }
        }
        break;
      }
      case opcode::OP_FORPREP: {
        // R(A) -= R(A+2); pc += sBx
        lua_value &init = _registers[_ra];
        lua_value &limit = _registers[_ra + 1];
        lua_value &step = _registers[_ra + 2];

        lua_integer int_limit;
        bool valid_int_limit = limit.tointeger(int_limit);

        if (is<lua_integer>(init.data) && is<lua_integer>(step.data) && valid_int_limit) {
          lua_integer istep = std::get<lua_integer>(step.data);
          lua_integer iinit = std::get<lua_integer>(init.data);
          
          limit.data.emplace<lua_integer>(int_limit);
          init.data.emplace<lua_integer>(iinit - istep);
        } else {
          // Try making everything a float
          lua_number nlimit, ninit, nstep;
          if (!init.tonumber(ninit)) {}
          if (!limit.tonumber(nlimit)) {}
          if (!step.tonumber(nstep)) {}
          // TODO: Error if number conversion fails
          limit.data.emplace<lua_number>(nlimit);
          init.data.emplace<lua_number>(ninit - nstep);
          step.data.emplace<lua_number>(nstep);
        }
        RL_quokka_vm_PC(_ci_ref) += opcode_util::get_sBx(_instruction);
        break;
      }
      case opcode::OP_TFORCALL: {
        // R(A+3) ... R(A+2+C) = R(A)( R(A+1), R(A+2) )
        // Setup the R(A)( R(A+1), R(A+2) )
        size_t call_base = _ra + 3;
        _registers.emplace(call_base + 2, _registers[_ra + 2]);
        _registers.emplace(call_base + 1, _registers[_ra + 1]);
        _registers.emplace(call_base, _registers[_ra]);
        // Expecting _arg_c return values
        if (!precall(call_base, _arg_c))
          execute();
        // Next instruction is OP_TFORLOOP, so let the loop go ahead
        break;
      }
      case opcode::OP_TFORLOOP: {
        // if R(A+1) ~= nil then { R(A) = R(A+1); pc += sBx }
        lua_value &tv1 = _registers[_ra + 1];
        if (!tv1.is_nil()) {
          _registers.emplace(_ra, tv1);
          RL_quokka_vm_PC(_ci_ref) += opcode_util::get_sBx(_instruction);
        }
        break;
      }
      case opcode::OP_SETLIST: {
        // R(A)[(C - 1) * FPF + i] = R(A+i), 1 <= i <= B
        // Note that in lua, FPF is LFIELDS_PER_FLUSH, a magic value of '50'
        // that's been in the Lua source for the last 15 years. It's not possible
        // to infer the value of FPF from the bytecode alone, so there is a huge 
        // assumption here that FPF=50.
        // Note that if B == 0, b = reg top - ra - 1
        // Note also that if C == 0, c = extra arg as Ax
        size_t fpf = 50;
        size_t b = _arg_b;
        size_t c = _arg_c;
        lua_table &table = _registers[_ra].obj().get()->table();

        if (b == 0) {
          // b = reg top - ra - 1
          b = _registers.size() - _ra - 1;
        }
        if (c == 0) {
          // Get extra arg
          c = opcode_util::get_Ax(*(RL_quokka_vm_PC(_ci_ref)++));
        }
        size_t stack_pop = _registers.size() - b;

        // Work backwards, saves us assigning a new iterator var
        lua_integer table_idx = ((c - 1) * fpf + b);
        for (; b > 0; b--) {
          table.set(table_idx--, _registers[_ra + b]);
        }

        // Pop the stack constants
        _registers.chop(stack_pop);
        break;
      }
      case opcode::OP_CLOSURE: {
        // R(A) = closure(KPROTO[Bx])
        lua_closure &this_closure = (*_cl_ref)->lua_func();
        bytecode_prototype &proto = *this_closure.proto->protos[opcode_util::get_Bx(_instruction)];
        object_view cache = lclosure_cache(proto, _base, _cl_ref);
        if (cache.is_valid()) {
          _registers.emplace(_ra, cache);
        } else {
          _registers.emplace(_ra, lclosure_new(proto, _base, _cl_ref));
        }
        break;
      }
      case opcode::OP_VARARG: {
        // R(A) R(A+1) ... R(A+B-2) = vararg
        // b = required results
        int b = _arg_b - 1;
        int n = (_base - (*_ci_ref)->func_idx) - (*_cl_ref)->lua_func().proto->num_params - 1;
        // Less args than params
        if (n < 0)
          n = 0;
        if (b < 0)
          b = n;
        
        int j;
        for (j = 0; j < b && j < n; j++)
          _registers.emplace(_ra + j, _registers[_base - n + j]);
        for (; j < b; j++)
          _registers.emplace(_ra + j); // nil
        
        break;
      }
      default:
        /* Not supported */
        break;
    }
  }
}

bool quokka_vm::postcall(size_t first_result_idx, int nreturn) {
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
    case MULTIRET: {
      // Multiple returns
      for (int i = 0; i < nreturn; i++)
        _registers.emplace(res + i, _registers[first_result_idx + i]);
      _registers.chop(res + nreturn);
      return false;
    }
    default: {
      int i;
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

void quokka_vm::close_upvals(size_t level) {
  for (size_t i = 0; i < _upvals.size(); i++) {
    lua_upval &uv = _upvals[i];
    // Ensure upval is open
    if (!uv.is_free && is<size_t>(uv.value)) {
      size_t stack_idx = std::get<size_t>(uv.value);
      if (level <= stack_idx) {
        // Close upval
        uv.value.emplace<lua_value>(_registers[stack_idx]);
      }
    }
  }
}

object_view quokka_vm::lclosure_cache(bytecode_prototype &proto, size_t base, object_view parent_cl) {
  object_view cl_ref = proto.closure_cache;
  if (cl_ref.is_valid()) {
    int num_upval = proto.num_upvalues;
    for (int i = 0; i < num_upval; i++) {
      bytecode_upvalue v = proto.upvalues[i];
      lua_value tval_cache, tval_target;
      // Get the actual upvalue from the closure cache
      RL_quokka_vm_UPV(i, tval_cache, cl_ref);
      // Get the target tval (either in the stack, or inherited from parent)
      if (v.instack) {
        tval_target = _registers[base + v.idx];
      } else {
        RL_quokka_vm_UPV(v.idx, tval_target, parent_cl);
      }

      if (!(tval_cache == tval_target))
        return object_view(); // upvalues don't match
    }
  }
  return cl_ref;
}

object_view quokka_vm::lclosure_new(bytecode_prototype &proto, size_t base, object_view parent_cl) {
  int num_upval = proto.num_upvalues;
  object_view new_closure = alloc_object();
  lua_closure &ncl = (*new_closure)->lua_func();
  ncl.proto = &proto;
  
  // Assign each upval
  for (int i = 0; i < num_upval; i++) {
    bytecode_upvalue v = proto.upvalues[i];
    if (v.instack) {
      // Find upval
      bool upval_found = false;
      size_t level = base + v.idx;
      for (size_t i = 0; i < _upvals.size() && !upval_found; i++) {
        lua_upval &uv = _upvals[i];
        // Ensure upval is open
        if (!uv.is_free && is<size_t>(uv.value)) {
          size_t stack_idx = std::get<size_t>(uv.value);
          if (stack_idx == level) {
            ncl.upval_views.emplace(i, upval_view(&_upvals, i));
            upval_found = true;
          }
        }
      }

      // Upval could not be found - make a new one
      if (!upval_found) {
        upval_view uvr = alloc_upval();
        (*uvr)->value.emplace<size_t>(level);
        ncl.upval_views.emplace(i, uvr);
      }
    } else {
      // Use upval from parent function
      ncl.upval_views.emplace(i, (*parent_cl)->lua_func().upval_views[v.idx]);
    }
  }

  // Save the closure in a cache in the prototype
  proto.closure_cache = new_closure;

  return new_closure;
}