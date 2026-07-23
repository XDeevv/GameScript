/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#include <math.h>
#include <stdlib.h>
#include "GSopcodes.h"
#include "GSvm.h"
#include "GSfuncproto.h"
#include "GSclosure.h"
#include "GSstring.h"
#include "GStable.h"
#include "GSuserdata.h"
#include "GSarray.h"
#include "GSclass.h"
#include "gstdio.h"

#include <stdio.h>
#include "GScompiler.h"

#define TOP() (_stack._vals[_top-1])
#define TARGET _stack._vals[_stackbase+arg0]
#define STK(a) _stack._vals[_stackbase+(a)]

bool GSVM::BW_OP(GSUnsignedInteger op,GSObjectPtr &trg,const GSObjectPtr &o1,const GSObjectPtr &o2)
{
    GSInteger res;
    if((GS_type(o1)| GS_type(o2)) == OT_INTEGER)
    {
        GSInteger i1 = _integer(o1), i2 = _integer(o2);
        switch(op) {
            case BW_AND:    res = i1 & i2; break;
            case BW_OR:     res = i1 | i2; break;
            case BW_XOR:    res = i1 ^ i2; break;
            case BW_SHIFTL: res = i1 << i2; break;
            case BW_SHIFTR: res = i1 >> i2; break;
            case BW_USHIFTR:res = (GSInteger)(*((GSUnsignedInteger*)&i1) >> i2); break;
            default: { Raise_Error(_SC("internal vm error bitwise op failed")); return false; }
        }
    }
    else { Raise_Error(_SC("bitwise op between '%s' and '%s'"),GetTypeName(o1),GetTypeName(o2)); return false;}
    trg = res;
    return true;
}

#define _ARITH_(op,trg,o1,o2) \
{ \
    GSInteger tmask = GS_type(o1)|GS_type(o2); \
    switch(tmask) { \
        case OT_INTEGER: trg = _integer(o1) op _integer(o2);break; \
        case (OT_FLOAT|OT_INTEGER): \
        case (OT_FLOAT): trg = tofloat(o1) op tofloat(o2); break;\
        default: _GUARD(ARITH_OP((#op)[0],trg,o1,o2)); break;\
    } \
}

#define _ARITH_NOZERO(op,trg,o1,o2,err) \
{ \
    GSInteger tmask = GS_type(o1)|GS_type(o2); \
    switch(tmask) { \
        case OT_INTEGER: { GSInteger i2 = _integer(o2); if(i2 == 0) { Raise_Error(err); GS_THROW(); } trg = _integer(o1) op i2; } break;\
        case (OT_FLOAT|OT_INTEGER): \
        case (OT_FLOAT): trg = tofloat(o1) op tofloat(o2); break;\
        default: _GUARD(ARITH_OP((#op)[0],trg,o1,o2)); break;\
    } \
}

bool GSVM::ARITH_OP(GSUnsignedInteger op,GSObjectPtr &trg,const GSObjectPtr &o1,const GSObjectPtr &o2)
{
    GSInteger tmask = GS_type(o1)| GS_type(o2);
    switch(tmask) {
        case OT_INTEGER:{
            GSInteger res, i1 = _integer(o1), i2 = _integer(o2);
            switch(op) {
            case '+': res = i1 + i2; break;
            case '-': res = i1 - i2; break;
            case '/': if (i2 == 0) { Raise_Error(_SC("division by zero")); return false; }
                    else if (i2 == -1 && i1 == INT_MIN) { Raise_Error(_SC("integer overflow")); return false; }
                    res = i1 / i2;
                    break;
            case '*': res = i1 * i2; break;
            case '%': if (i2 == 0) { Raise_Error(_SC("modulo by zero")); return false; }
                    else if (i2 == -1 && i1 == INT_MIN) { res = 0; break; }
                    res = i1 % i2;
                    break;
            default: res = 0xDEADBEEF;
            }
            trg = res; }
            break;
        case (OT_FLOAT|OT_INTEGER):
        case (OT_FLOAT):{
            GSFloat res, f1 = tofloat(o1), f2 = tofloat(o2);
            switch(op) {
            case '+': res = f1 + f2; break;
            case '-': res = f1 - f2; break;
            case '/': res = f1 / f2; break;
            case '*': res = f1 * f2; break;
            case '%': res = GSFloat(fmod((double)f1,(double)f2)); break;
            default: res = 0x0f;
            }
            trg = res; }
            break;
        default:
            if(op == '+' && (tmask & _RT_STRING)){
                if(!StringCat(o1, o2, trg)) return false;
            }
            else if(!ArithMetaMethod(op,o1,o2,trg)) {
                return false;
            }
    }
    return true;
}

GSVM::GSVM(GSSharedState *ss)
{
    _sharedstate=ss;
    _suspended = GSFalse;
    _suspended_target = -1;
    _suspended_root = GSFalse;
    _suspended_traps = -1;
    _foreignptr = NULL;
    _nnativecalls = 0;
    _nmetamethodscall = 0;
    _lasterror.Null();
    _errorhandler.Null();
    _debughook = false;
    _debughook_native = NULL;
    _debughook_closure.Null();
    _openouters = NULL;
    ci = NULL;
    _releasehook = NULL;
    INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);
}

void GSVM::Finalize()
{
    if(_releasehook) { _releasehook(_foreignptr,0); _releasehook = NULL; }
    if(_openouters) CloseOuters(&_stack._vals[0]);
    _roottable.Null();
    _lasterror.Null();
    _errorhandler.Null();
    _debughook = false;
    _debughook_native = NULL;
    _debughook_closure.Null();
    temp_reg.Null();
    _callstackdata.resize(0);
    GSInteger size=_stack.size();
    for(GSInteger i=0;i<size;i++)
        _stack[i].Null();
}

GSVM::~GSVM()
{
    Finalize();
    REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
}

bool GSVM::ArithMetaMethod(GSInteger op,const GSObjectPtr &o1,const GSObjectPtr &o2,GSObjectPtr &dest)
{
    GSMetaMethod mm;
    switch(op){
        case _SC('+'): mm=MT_ADD; break;
        case _SC('-'): mm=MT_SUB; break;
        case _SC('/'): mm=MT_DIV; break;
        case _SC('*'): mm=MT_MUL; break;
        case _SC('%'): mm=MT_MODULO; break;
        default: mm = MT_ADD; assert(0); break; //shutup compiler
    }
    if(is_delegable(o1) && _delegable(o1)->_delegate) {

        GSObjectPtr closure;
        if(_delegable(o1)->GetMetaMethod(this, mm, closure)) {
            Push(o1);Push(o2);
            return CallMetaMethod(closure,mm,2,dest);
        }
    }
    Raise_Error(_SC("arith op %c on between '%s' and '%s'"),op,GetTypeName(o1),GetTypeName(o2));
    return false;
}

bool GSVM::NEG_OP(GSObjectPtr &trg,const GSObjectPtr &o)
{

    switch(GS_type(o)) {
    case OT_INTEGER:
        trg = -_integer(o);
        return true;
    case OT_FLOAT:
        trg = -_float(o);
        return true;
    case OT_TABLE:
    case OT_USERDATA:
    case OT_INSTANCE:
        if(_delegable(o)->_delegate) {
            GSObjectPtr closure;
            if(_delegable(o)->GetMetaMethod(this, MT_UNM, closure)) {
                Push(o);
                if(!CallMetaMethod(closure, MT_UNM, 1, temp_reg)) return false;
                _Swap(trg,temp_reg);
                return true;

            }
        }
    default:break; //shutup compiler
    }
    Raise_Error(_SC("attempt to negate a %s"), GetTypeName(o));
    return false;
}

#define _RET_SUCCEED(exp) { result = (exp); return true; }
bool GSVM::ObjCmp(const GSObjectPtr &o1,const GSObjectPtr &o2,GSInteger &result)
{
    GSObjectType t1 = GS_type(o1), t2 = GS_type(o2);
    if(t1 == t2) {
        if(_rawval(o1) == _rawval(o2))_RET_SUCCEED(0);
        GSObjectPtr res;
        switch(t1){
        case OT_STRING:
            _RET_SUCCEED(scstrcmp(_stringval(o1),_stringval(o2)));
        case OT_INTEGER:
            _RET_SUCCEED((_integer(o1)<_integer(o2))?-1:1);
        case OT_FLOAT:
            _RET_SUCCEED((_float(o1)<_float(o2))?-1:1);
        case OT_TABLE:
        case OT_USERDATA:
        case OT_INSTANCE:
            if(_delegable(o1)->_delegate) {
                GSObjectPtr closure;
                if(_delegable(o1)->GetMetaMethod(this, MT_CMP, closure)) {
                    Push(o1);Push(o2);
                    if(CallMetaMethod(closure,MT_CMP,2,res)) {
                        if(GS_type(res) != OT_INTEGER) {
                            Raise_Error(_SC("_cmp must return an integer"));
                            return false;
                        }
                        _RET_SUCCEED(_integer(res))
                    }
                    return false;
                }
            }
            //continues through (no break needed)
        default:
            _RET_SUCCEED( _userpointer(o1) < _userpointer(o2)?-1:1 );
        }
        assert(0);
        //if(type(res)!=OT_INTEGER) { Raise_CompareError(o1,o2); return false; }
        //  _RET_SUCCEED(_integer(res));

    }
    else{
        if(GS_isnumeric(o1) && GS_isnumeric(o2)){
            if((t1==OT_INTEGER) && (t2==OT_FLOAT)) {
                if( _integer(o1)==_float(o2) ) { _RET_SUCCEED(0); }
                else if( _integer(o1)<_float(o2) ) { _RET_SUCCEED(-1); }
                _RET_SUCCEED(1);
            }
            else{
                if( _float(o1)==_integer(o2) ) { _RET_SUCCEED(0); }
                else if( _float(o1)<_integer(o2) ) { _RET_SUCCEED(-1); }
                _RET_SUCCEED(1);
            }
        }
        else if(t1==OT_NULL) {_RET_SUCCEED(-1);}
        else if(t2==OT_NULL) {_RET_SUCCEED(1);}
        else { Raise_CompareError(o1,o2); return false; }

    }
    assert(0);
    _RET_SUCCEED(0); //cannot happen
}

bool GSVM::CMP_OP(CmpOP op, const GSObjectPtr &o1,const GSObjectPtr &o2,GSObjectPtr &res)
{
    GSInteger r;
    if(ObjCmp(o1,o2,r)) {
        switch(op) {
            case CMP_G: res = (r > 0); return true;
            case CMP_GE: res = (r >= 0); return true;
            case CMP_L: res = (r < 0); return true;
            case CMP_LE: res = (r <= 0); return true;
            case CMP_3W: res = r; return true;
        }
        assert(0);
    }
    return false;
}

bool GSVM::ToString(const GSObjectPtr &o,GSObjectPtr &res)
{
    switch(GS_type(o)) {
    case OT_STRING:
        res = o;
        return true;
    case OT_FLOAT:
        scsprintf(_sp(GS_rsl(NUMBER_MAX_CHAR+1)),GS_rsl(NUMBER_MAX_CHAR),_SC("%g"),_float(o));
        break;
    case OT_INTEGER:
        scsprintf(_sp(GS_rsl(NUMBER_MAX_CHAR+1)),GS_rsl(NUMBER_MAX_CHAR),_PRINT_INT_FMT,_integer(o));
        break;
    case OT_BOOL:
        scsprintf(_sp(GS_rsl(6)),GS_rsl(6),_integer(o)?_SC("true"):_SC("false"));
        break;
    case OT_NULL:
        scsprintf(_sp(GS_rsl(5)),GS_rsl(5),_SC("null"));
        break;
    case OT_TABLE:
    case OT_USERDATA:
    case OT_INSTANCE:
        if(_delegable(o)->_delegate) {
            GSObjectPtr closure;
            if(_delegable(o)->GetMetaMethod(this, MT_TOSTRING, closure)) {
                Push(o);
                if(CallMetaMethod(closure,MT_TOSTRING,1,res)) {
                    if(GS_type(res) == OT_STRING)
                        return true;
                }
                else {
                    return false;
                }
            }
        }
    default:
        scsprintf(_sp(GS_rsl((sizeof(void*)*2)+NUMBER_MAX_CHAR)),GS_rsl((sizeof(void*)*2)+NUMBER_MAX_CHAR),_SC("(%s : 0x%p)"),GetTypeName(o),(void*)_rawval(o));
    }
    res = GSString::Create(_ss(this),_spval);
    return true;
}

bool GSVM::StringCat(const GSObjectPtr &str,const GSObjectPtr &obj,GSObjectPtr &dest)
{
    GSObjectPtr a, b;
    if(!ToString(str, a)) return false;
    if(!ToString(obj, b)) return false;
    GSInteger l = _string(a)->_len , ol = _string(b)->_len;
#ifdef GS_NO_FAST_STRINGCAT
    GSChar* s = _sp(GS_rsl(l + ol + 1));
    memcpy(s, _stringval(a), GS_rsl(l));
    memcpy(s + l, _stringval(b), GS_rsl(ol));
    dest = GSString::Create(_ss(this), _spval, l + ol);
#else
    dest = GSString::Concat(_ss(this),_stringval(a),l,_stringval(b),ol);
#endif
    return true;
}

bool GSVM::TypeOf(const GSObjectPtr &obj1,GSObjectPtr &dest)
{
    if(is_delegable(obj1) && _delegable(obj1)->_delegate) {
        GSObjectPtr closure;
        if(_delegable(obj1)->GetMetaMethod(this, MT_TYPEOF, closure)) {
            Push(obj1);
            return CallMetaMethod(closure,MT_TYPEOF,1,dest);
        }
    }
    dest = GSString::Create(_ss(this),GetTypeName(obj1));
    return true;
}

bool GSVM::Init(GSVM *friendvm, GSInteger stacksize)
{
    _stack.resize(stacksize);
    _alloccallsstacksize = 4;
    _callstackdata.resize(_alloccallsstacksize);
    _callsstacksize = 0;
    _callsstack = &_callstackdata[0];
    _stackbase = 0;
    _top = 0;
    if(!friendvm) {
        _roottable = GSTable::Create(_ss(this), 0);
        GS_base_register(this);
    }
    else {
        _roottable = friendvm->_roottable;
        _errorhandler = friendvm->_errorhandler;
        _debughook = friendvm->_debughook;
        _debughook_native = friendvm->_debughook_native;
        _debughook_closure = friendvm->_debughook_closure;
    }


    return true;
}


bool GSVM::StartCall(GSClosure *closure,GSInteger target,GSInteger args,GSInteger stackbase,bool tailcall)
{
    GSFunctionProto *func = closure->_function;

    GSInteger paramssize = func->_nparameters;
    const GSInteger newtop = stackbase + func->_stacksize;
    GSInteger nargs = args;
    if(func->_varparams)
    {
        paramssize--;
        if (nargs < paramssize) {
            Raise_Error(_SC("wrong number of parameters (%d passed, at least %d required)"),
              (int)nargs, (int)paramssize);
            return false;
        }

        //dumpstack(stackbase);
        GSInteger nvargs = nargs - paramssize;
        GSArray *arr = GSArray::Create(_ss(this),nvargs);
        GSInteger pbase = stackbase+paramssize;
        for(GSInteger n = 0; n < nvargs; n++) {
            arr->_values[n] = _stack._vals[pbase];
            _stack._vals[pbase].Null();
            pbase++;

        }
        _stack._vals[stackbase+paramssize] = arr;
        //dumpstack(stackbase);
    }
    else if (paramssize != nargs) {
        GSInteger ndef = func->_ndefaultparams;
        GSInteger diff;
        if(ndef && nargs < paramssize && (diff = paramssize - nargs) <= ndef) {
            for(GSInteger n = ndef - diff; n < ndef; n++) {
                _stack._vals[stackbase + (nargs++)] = closure->_defaultparams[n];
            }
        }
        else {
            Raise_Error(_SC("wrong number of parameters (%d passed, %d required)"),
              (int)nargs, (int)paramssize);
            return false;
        }
    }

    if(closure->_env) {
        _stack._vals[stackbase] = closure->_env->_obj;
    }

    if(!EnterFrame(stackbase, newtop, tailcall)) return false;

    ci->_closure  = closure;
    ci->_literals = func->_literals;
    ci->_ip       = func->_instructions;
    ci->_target   = (GSInt32)target;

    if (_debughook) {
        CallDebugHook(_SC('c'));
    }

    if (closure->_function->_bgenerator) {
        GSFunctionProto *f = closure->_function;
        GSGenerator *gen = GSGenerator::Create(_ss(this), closure);
        if(!gen->Yield(this,f->_stacksize))
            return false;
        GSObjectPtr temp;
        Return(1, target, temp);
        STK(target) = gen;
    }


    return true;
}

bool GSVM::Return(GSInteger _arg0, GSInteger _arg1, GSObjectPtr &retval)
{
    GSBool    _isroot      = ci->_root;
    GSInteger callerbase   = _stackbase - ci->_prevstkbase;

    if (_debughook) {
        for(GSInteger i=0; i<ci->_ncalls; i++) {
            CallDebugHook(_SC('r'));
        }
    }

    GSObjectPtr *dest;
    if (_isroot) {
        dest = &(retval);
    } else if (ci->_target == -1) {
        dest = NULL;
    } else {
        dest = &_stack._vals[callerbase + ci->_target];
    }
    if (dest) {
        if(_arg0 != 0xFF) {
            *dest = _stack._vals[_stackbase+_arg1];
        }
        else {
            dest->Null();
        }
        //*dest = (_arg0 != 0xFF) ? _stack._vals[_stackbase+_arg1] : _null_;
    }
    LeaveFrame();
    return _isroot ? true : false;
}

#define _RET_ON_FAIL(exp) { if(!exp) return false; }

bool GSVM::PLOCAL_INC(GSInteger op,GSObjectPtr &target, GSObjectPtr &a, GSObjectPtr &incr)
{
    GSObjectPtr trg;
    _RET_ON_FAIL(ARITH_OP( op , trg, a, incr));
    target = a;
    a = trg;
    return true;
}

bool GSVM::DerefInc(GSInteger op,GSObjectPtr &target, GSObjectPtr &self, GSObjectPtr &key, GSObjectPtr &incr, bool postfix,GSInteger selfidx)
{
    GSObjectPtr tmp, tself = self, tkey = key;
    if (!Get(tself, tkey, tmp, 0, selfidx)) { return false; }
    _RET_ON_FAIL(ARITH_OP( op , target, tmp, incr))
    if (!Set(tself, tkey, target,selfidx)) { return false; }
    if (postfix) target = tmp;
    return true;
}

#define arg0 (_i_._arg0)
#define sarg0 ((GSInteger)*((const signed char *)&_i_._arg0))
#define arg1 (_i_._arg1)
#define sarg1 (*((const GSInt32 *)&_i_._arg1))
#define arg2 (_i_._arg2)
#define arg3 (_i_._arg3)
#define sarg3 ((GSInteger)*((const signed char *)&_i_._arg3))

GSRESULT GSVM::Suspend()
{
    if (_suspended)
        return GS_throwerror(this, _SC("cannot suspend an already suspended vm"));
    if (_nnativecalls!=2)
        return GS_throwerror(this, _SC("cannot suspend through native calls/metamethods"));
    return GS_SUSPEND_FLAG;
}


#define _FINISH(howmuchtojump) {jump = howmuchtojump; return true; }
bool GSVM::FOREACH_OP(GSObjectPtr &o1,GSObjectPtr &o2,GSObjectPtr
&o3,GSObjectPtr &o4,GSInteger GS_UNUSED_ARG(arg_2),int exitpos,int &jump)
{
    GSInteger nrefidx;
    switch(GS_type(o1)) {
    case OT_TABLE:
        if((nrefidx = _table(o1)->Next(false,o4, o2, o3)) == -1) _FINISH(exitpos);
        o4 = (GSInteger)nrefidx; _FINISH(1);
    case OT_ARRAY:
        if((nrefidx = _array(o1)->Next(o4, o2, o3)) == -1) _FINISH(exitpos);
        o4 = (GSInteger) nrefidx; _FINISH(1);
    case OT_STRING:
        if((nrefidx = _string(o1)->Next(o4, o2, o3)) == -1)_FINISH(exitpos);
        o4 = (GSInteger)nrefidx; _FINISH(1);
    case OT_CLASS:
        if((nrefidx = _class(o1)->Next(o4, o2, o3)) == -1)_FINISH(exitpos);
        o4 = (GSInteger)nrefidx; _FINISH(1);
    case OT_USERDATA:
    case OT_INSTANCE:
        if(_delegable(o1)->_delegate) {
            GSObjectPtr itr;
            GSObjectPtr closure;
            if(_delegable(o1)->GetMetaMethod(this, MT_NEXTI, closure)) {
                Push(o1);
                Push(o4);
                if(CallMetaMethod(closure, MT_NEXTI, 2, itr)) {
                    o4 = o2 = itr;
                    if(GS_type(itr) == OT_NULL) _FINISH(exitpos);
                    if(!Get(o1, itr, o3, 0, DONT_FALL_BACK)) {
                        Raise_Error(_SC("_nexti returned an invalid idx")); // cloud be changed
                        return false;
                    }
                    _FINISH(1);
                }
                else {
                    return false;
                }
            }
            Raise_Error(_SC("_nexti failed"));
            return false;
        }
        break;
    case OT_GENERATOR:
        if(_generator(o1)->_state == GSGenerator::eDead) _FINISH(exitpos);
        if(_generator(o1)->_state == GSGenerator::eSuspended) {
            GSInteger idx = 0;
            if(GS_type(o4) == OT_INTEGER) {
                idx = _integer(o4) + 1;
            }
            o2 = idx;
            o4 = idx;
            _generator(o1)->Resume(this, o3);
            _FINISH(0);
        }
    default:
        Raise_Error(_SC("cannot iterate %s"), GetTypeName(o1));
    }
    return false; //cannot be hit(just to avoid warnings)
}

#define COND_LITERAL (arg3!=0?ci->_literals[arg1]:STK(arg1))

#define GS_THROW() { goto exception_trap; }

#define _GUARD(exp) { if(!exp) { GS_THROW();} }

bool GSVM::CLOSURE_OP(GSObjectPtr &target, GSFunctionProto *func,GSInteger boundtarget)
{
    GSInteger nouters;
    
    //////////////////////////////////////////////
    GSClosure *current_closure = _closure(ci->_closure);
    GSClosure *closure = GSClosure::Create(_ss(this), func, current_closure->_root);
    //////////////////////////////////////////////
    
    if((nouters = func->_noutervalues)) {
        for(GSInteger i = 0; i<nouters; i++) {
            GSOuterVar &v = func->_outervalues[i];
            switch(v._type){
            case otLOCAL:
                FindOuter(closure->_outervalues[i], &STK(_integer(v._src)));
                break;
            case otOUTER:
                closure->_outervalues[i] = _closure(ci->_closure)->_outervalues[_integer(v._src)];
                break;
            }
        }
    }
    GSInteger ndefparams;
    if((ndefparams = func->_ndefaultparams)) {
        for(GSInteger i = 0; i < ndefparams; i++) {
            GSInteger spos = func->_defaultparams[i];
            closure->_defaultparams[i] = _stack._vals[_stackbase + spos];
        }
    }
    if (boundtarget != 0xFF) {
        GSObjectPtr &val = _stack._vals[_stackbase + boundtarget];
        GSObjectType t = GS_type(val);
        if (t == OT_TABLE || t == OT_CLASS || t == OT_INSTANCE || t == OT_ARRAY) {
            closure->_env = _refcounted(val)->GetWeakRef(t);
            __ObjAddRef(closure->_env);
        }
        else {
            Raise_Error(_SC("cannot bind a %s as environment object"), IdType2Name(t));
            closure->Release();
            return false;
        }
    }
    target = closure;
    return true;
}


bool GSVM::CLASS_OP(GSObjectPtr &target,GSInteger baseclass,GSInteger attributes)
{
    GSClass *base = NULL;
    GSObjectPtr attrs;
    if(baseclass != -1) {
        if(GS_type(_stack._vals[_stackbase+baseclass]) != OT_CLASS) { Raise_Error(_SC("trying to inherit from a %s"),GetTypeName(_stack._vals[_stackbase+baseclass])); return false; }
        base = _class(_stack._vals[_stackbase + baseclass]);
    }
    if(attributes != MAX_FUNC_STACKSIZE) {
        attrs = _stack._vals[_stackbase+attributes];
    }
    target = GSClass::Create(_ss(this),base);
    if(GS_type(_class(target)->_metamethods[MT_INHERITED]) != OT_NULL) {
        int nparams = 2;
        GSObjectPtr ret;
        Push(target); Push(attrs);
        if(!Call(_class(target)->_metamethods[MT_INHERITED],nparams,_top - nparams, ret, false)) {
            Pop(nparams);
            return false;
        }
        Pop(nparams);
    }
    _class(target)->_attributes = attrs;
    return true;
}

bool GSVM::IsEqual(const GSObjectPtr &o1,const GSObjectPtr &o2,bool &res)
{
	GSObjectType t1 = GS_type(o1), t2 = GS_type(o2);
    if(t1 == t2) {
		if (t1 == OT_FLOAT) {
			res = (_float(o1) == _float(o2));
		}
		else {
			res = (_rawval(o1) == _rawval(o2));
		}
    }
    else {
        if(GS_isnumeric(o1) && GS_isnumeric(o2)) {
            res = (tofloat(o1) == tofloat(o2));
        }
        else {
            res = false;
        }
    }
    return true;
}

bool GSVM::IsFalse(GSObjectPtr &o)
{
    if(((GS_type(o) & GSOBJECT_CANBEFALSE)
        && ( ((GS_type(o) == OT_FLOAT) && (_float(o) == GSFloat(0.0))) ))
#if !defined(GSUSEDOUBLE) || (defined(GSUSEDOUBLE) && defined(_GS64))
        || (_integer(o) == 0) )  //OT_NULL|OT_INTEGER|OT_BOOL
#else
        || (((GS_type(o) != OT_FLOAT) && (_integer(o) == 0))) )  //OT_NULL|OT_INTEGER|OT_BOOL
#endif
    {
        return true;
    }
    return false;
}
extern GSInstructionDesc g_InstrDesc[];

static bool ResolveNamespaceTable(GSVM* vm, GSObjectPtr root, GSObjectPtr namespace_str, GSObjectPtr &target_table) {
    const GSChar *str = _stringval(namespace_str);
    GSObjectPtr current = root;
    GSChar buf[256];
    int i = 0;
    
    while (*str) {
        if (*str == _SC('/')) {
            buf[i] = _SC('\0');
            GSObjectPtr key = GSString::Create(_ss(vm), buf, i);
            GSObjectPtr next_table;
            if (!vm->Get(current, key, next_table, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) return false;
            current = next_table;
            i = 0;
        } else {
            if (i < 255) buf[i++] = *str;
        }
        str++;
    }
    if (i > 0) {
        buf[i] = _SC('\0');
        GSObjectPtr key = GSString::Create(_ss(vm), buf, i);
        GSObjectPtr next_table;
        if (!vm->Get(current, key, next_table, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) return false;
        current = next_table;
    }
    target_table = current;
    return true;
}

static GSObjectPtr BuildNamespaceTables(GSVM* vm, GSObjectPtr root, GSObjectPtr namespace_str) {
    const GSChar *str = _stringval(namespace_str);
    GSObjectPtr current = root;
    GSChar buf[256];
    int i = 0;
    
    while (*str) {
        if (*str == _SC('/')) {
            buf[i] = _SC('\0');
            GSObjectPtr key = GSString::Create(_ss(vm), buf, i);
            GSObjectPtr next_table;
            
            if (!vm->Get(current, key, next_table, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
    next_table = GSTable::Create(_ss(vm), 0);
    
    _table(next_table)->SetDelegate(_table(vm->_roottable));
    
    vm->NewSlot(current, key, next_table, false);
    // scprintf(_SC("Created namespace table for: %s\n"), _stringval(key));
}
            current = next_table;
            i = 0;
        } else {
            if (i < 255) buf[i++] = *str;
        }
        str++;
    }
    if (i > 0) {
        buf[i] = _SC('\0');
        GSObjectPtr key = GSString::Create(_ss(vm), buf, i);
        GSObjectPtr next_table;
        if (!vm->Get(current, key, next_table, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
            next_table = GSTable::Create(_ss(vm), 0);
            
            vm->NewSlot(current, key, next_table, false);
        }
        current = next_table;
    }
    return current;
}

static GSInteger ImportFileRead(GSUserPointer file) {
    FILE *f = (FILE *)file;
    int c = fgetc(f);
    if (c == EOF) return 0;
    return c;
}

bool GSVM::Execute(GSObjectPtr &closure, GSInteger nargs, GSInteger stackbase,GSObjectPtr &outres, GSBool raiseerror,ExecutionType et)
{
    if ((_nnativecalls + 1) > MAX_NATIVE_CALLS) { Raise_Error(_SC("Native stack overflow")); return false; }
    _nnativecalls++;
    AutoDec ad(&_nnativecalls);
    GSInteger traps = 0;
    CallInfo *prevci = ci;

    switch(et) {
        case ET_CALL: {
            temp_reg = closure;
            if(!StartCall(_closure(temp_reg), _top - nargs, nargs, stackbase, false)) {
                //call the handler if there are no calls in the stack, if not relies on the previous node
                if(ci == NULL) CallErrorHandler(_lasterror);
                return false;
            }
            if(ci == prevci) {
                outres = STK(_top-nargs);
                return true;
            }
            ci->_root = GSTrue;
                      }
            break;
        case ET_RESUME_GENERATOR: 
            if(!_generator(closure)->Resume(this, outres)) {
                return false;
            }
            ci->_root = GSTrue;
            traps += ci->_etraps;
            break;
        case ET_RESUME_VM:
        case ET_RESUME_THROW_VM:
            traps = _suspended_traps;
            ci->_root = _suspended_root;
            _suspended = GSFalse;
            if(et  == ET_RESUME_THROW_VM) { GS_THROW(); }
            break;
    }

exception_restore:
    //
    {
        for(;;)
        {
            const GSInstruction &_i_ = *ci->_ip++;
            switch(_i_.op)
            {
            case _OP_LINE: if (_debughook) CallDebugHook(_SC('l'),arg1); continue;
            case _OP_LOAD: TARGET = ci->_literals[arg1]; continue;
            case _OP_NAMESPACE: {
                GSObjectPtr ns_string = STK(arg0);
                GSObjectPtr current_table = BuildNamespaceTables(this, _roottable, ns_string);
                
                GSClosure *c = _closure(ci->_closure);
                GSObjectPtr root_obj = c->_root->_obj;
                if (GS_type(root_obj) == OT_TABLE) {
                    GSObjectPtr ns_key = GSString::Create(_ss(this), _SC("__namespace"));
                    _table(root_obj)->NewSlot(ns_key, current_table);
                }
                
                continue;
            }

            case _OP_IMPORT: {
                GSObjectPtr import_string = STK(arg0);
                
                GSObjectPtr modules_table;
                GSObjectPtr modules_key = GSString::Create(_ss(this), _SC("__modules"));
                if (!Get(_roottable, modules_key, modules_table, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
                    modules_table = GSTable::Create(_ss(this), 0);
                    NewSlot(_roottable, modules_key, modules_table, false);
                }

                GSObjectPtr cached_module;
                if (Get(modules_table, import_string, cached_module, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
                    continue;
                }

                GSChar filepath[1024];
                const GSChar* src = _stringval(import_string);
                int c = 0;
                while (*src && c < 1019) {
                    filepath[c++] = (*src == _SC('/')) ? _SC('/') : *src;
                    src++;
                }
                filepath[c++] = _SC('.'); filepath[c++] = _SC('g'); filepath[c++] = _SC('s'); filepath[c] = _SC('\0');
                
                FILE *f = NULL;
#ifdef GSUNICODE
                f = _wfopen(filepath, _SC("rb"));
#else
                f = fopen(filepath, _SC("rb"));
#endif
                if (f) {
                    GSObjectPtr closure_out;
                    if (Compile(this, ImportFileRead, f, filepath, closure_out, true, true)) {
                        
                        GSObjectPtr sandbox = GSTable::Create(_ss(this), 0);
                        _table(sandbox)->SetDelegate(_table(_roottable));

                        GSObjectPtr ns_key = GSString::Create(_ss(this), _SC("__namespace"));
                        _table(sandbox)->NewSlot(ns_key, sandbox);

                        GSClosure *closure = GSClosure::Create(_ss(this), _funcproto(closure_out), _table(sandbox)->GetWeakRef(OT_TABLE));
                        GSObjectPtr closure_obj = closure;
                        
                        NewSlot(modules_table, import_string, closure_obj, false);

                        GSInteger oldtop = _top;
                        Push(sandbox);
                        GSObjectPtr out;
                        Call(closure_obj, 1, oldtop, out, GSTrue);
                        _top = oldtop;
                    } else {
                        fclose(f);
                        Raise_Error(_SC("failed to compile imported module '%s'"), _stringval(import_string));
                        GS_THROW();
                    }
                    fclose(f);
                } 
                else {
                    Raise_Error(_SC("cannot load imported module '%s' (file not found: %s)"), _stringval(import_string), filepath);
                    GS_THROW();
                }
                
                continue;
            }
            case _OP_LOADINT:
#ifndef _GS64
                TARGET = (GSInteger)arg1; continue;
#else
                TARGET = (GSInteger)((GSInt32)arg1); continue;
#endif
            case _OP_LOADFLOAT: TARGET = *((const GSFloat *)&arg1); continue;
            case _OP_DLOAD: TARGET = ci->_literals[arg1]; STK(arg2) = ci->_literals[arg3];continue;
            case _OP_TAILCALL:{
                GSObjectPtr &t = STK(arg1);
                if (GS_type(t) == OT_CLOSURE
                    && (!_closure(t)->_function->_bgenerator)){
                    GSObjectPtr clo = t;
                    GSInteger last_top = _top;
                    if(_openouters) CloseOuters(&(_stack._vals[_stackbase]));
                    for (GSInteger i = 0; i < arg3; i++) STK(i) = STK(arg2 + i);
                    _GUARD(StartCall(_closure(clo), ci->_target, arg3, _stackbase, true));
                    if (last_top >= _top) {
                        _top = last_top;
                    }
                    continue;
                }
                              }
            case _OP_CALL: {
                    GSObjectPtr clo = STK(arg1);
                    switch (GS_type(clo)) {
                    case OT_CLOSURE:
                        _GUARD(StartCall(_closure(clo), sarg0, arg3, _stackbase+arg2, false));
                        continue;
                    case OT_NATIVECLOSURE: {
                        bool suspend;
						bool tailcall;
                        _GUARD(CallNative(_nativeclosure(clo), arg3, _stackbase+arg2, clo, (GSInt32)sarg0, suspend, tailcall));
                        if(suspend){
                            _suspended = GSTrue;
                            _suspended_target = sarg0;
                            _suspended_root = ci->_root;
                            _suspended_traps = traps;
                            outres = clo;
                            return true;
                        }
                        if(sarg0 != -1 && !tailcall) {
                            STK(arg0) = clo;
                        }
                                           }
                        continue;
                    case OT_CLASS:{
                        GSObjectPtr inst;
                        _GUARD(CreateClassInstance(_class(clo),inst,clo));
                        if(sarg0 != -1) {
                            STK(arg0) = inst;
                        }
                        GSInteger stkbase;
                        switch(GS_type(clo)) {
                            case OT_CLOSURE:
                                stkbase = _stackbase+arg2;
                                _stack._vals[stkbase] = inst;
                                _GUARD(StartCall(_closure(clo), -1, arg3, stkbase, false));
                                break;
                            case OT_NATIVECLOSURE:
                                bool dummy;
                                stkbase = _stackbase+arg2;
                                _stack._vals[stkbase] = inst;
                                _GUARD(CallNative(_nativeclosure(clo), arg3, stkbase, clo, -1, dummy, dummy));
                                break;
                            default: break; //shutup GCC 4.x
                        }
                        }
                        break;
                    case OT_TABLE:
                    case OT_USERDATA:
                    case OT_INSTANCE:{
                        GSObjectPtr closure;
                        if(_delegable(clo)->_delegate && _delegable(clo)->GetMetaMethod(this,MT_CALL,closure)) {
                            Push(clo);
                            for (GSInteger i = 0; i < arg3; i++) Push(STK(arg2 + i));
                            if(!CallMetaMethod(closure, MT_CALL, arg3+1, clo)) GS_THROW();
                            if(sarg0 != -1) {
                                STK(arg0) = clo;
                            }
                            break;
                        }

                        //Raise_Error(_SC("attempt to call '%s'"), GetTypeName(clo));
                        //GS_THROW();
                      }
                    default:
                        Raise_Error(_SC("attempt to call '%s'"), GetTypeName(clo));
                        GS_THROW();
                    }
                }
                  continue;
            case _OP_PREPCALL:
            case _OP_PREPCALLK: {
                    GSObjectPtr &key = _i_.op == _OP_PREPCALLK?(ci->_literals)[arg1]:STK(arg1);
                    GSObjectPtr &o = STK(arg2);
                    if (!Get(o, key, temp_reg,0,arg2)) {
                        GS_THROW();
                    }
                    STK(arg3) = o;
                    _Swap(TARGET,temp_reg);//TARGET = temp_reg;
                }
                continue;
            case _OP_GETK:
                if (!Get(STK(arg2), ci->_literals[arg1], temp_reg, 0,arg2)) { GS_THROW();}
                _Swap(TARGET,temp_reg);//TARGET = temp_reg;
                continue;
            case _OP_MOVE: TARGET = STK(arg1); continue;
            case _OP_NEWSLOT:
                _GUARD(NewSlot(STK(arg1), STK(arg2), STK(arg3),false));
                if(arg0 != 0xFF) TARGET = STK(arg3);
                continue;
            case _OP_DELETE: _GUARD(DeleteSlot(STK(arg1), STK(arg2), TARGET)); continue;
            case _OP_SET:
                if (!Set(STK(arg1), STK(arg2), STK(arg3),arg1)) { GS_THROW(); }
                if (arg0 != 0xFF) TARGET = STK(arg3);
                continue;
            case _OP_GET:
                if (!Get(STK(arg1), STK(arg2), temp_reg, 0,arg1)) { GS_THROW(); }
                _Swap(TARGET,temp_reg);//TARGET = temp_reg;
                continue;
            case _OP_EQ:{
                bool res;
                if(!IsEqual(STK(arg2),COND_LITERAL,res)) { GS_THROW(); }
                TARGET = res?true:false;
                }continue;
            case _OP_NE:{
                bool res;
                if(!IsEqual(STK(arg2),COND_LITERAL,res)) { GS_THROW(); }
                TARGET = (!res)?true:false;
                } continue;
            case _OP_ADD: _ARITH_(+,TARGET,STK(arg2),STK(arg1)); continue;
            case _OP_SUB: _ARITH_(-,TARGET,STK(arg2),STK(arg1)); continue;
            case _OP_MUL: _ARITH_(*,TARGET,STK(arg2),STK(arg1)); continue;
            case _OP_DIV: _ARITH_NOZERO(/,TARGET,STK(arg2),STK(arg1),_SC("division by zero")); continue;
            case _OP_MOD: ARITH_OP('%',TARGET,STK(arg2),STK(arg1)); continue;
            case _OP_BITW:  _GUARD(BW_OP( arg3,TARGET,STK(arg2),STK(arg1))); continue;
            case _OP_RETURN:
                if((ci)->_generator) {
                    (ci)->_generator->Kill();
                }
                
                ///////////////////////////////////////
                {
                    GSClosure *cur_closure = _closure(ci->_closure);
                    GSFunctionProto *proto = cur_closure->_function;
                    if (proto->_rettype != 0 && arg0 != 0xFF) {
                        GSObjectPtr &retval_to_check = STK(arg1);
                        GSObjectType actual_type = GS_type(retval_to_check);
                        bool type_matches = false;


                        // TYPE SYSTEM
                        switch (proto->_rettype) {
                            case TK_VOID: {
                                if (arg0 != 0xFF && actual_type != OT_NULL) {
                                    Raise_Error(_SC("type mismatch: a 'void' function cannot return a value"));
                                    GS_THROW();
                                }
                                type_matches = (arg0 == 0xFF || actual_type == OT_NULL); 
                                break;
                            }
                            case TK_IDENTIFIER:
                                type_matches = true;
                                break;
                            case TK_INT:    
                                type_matches = (actual_type == OT_INTEGER); 
                                break;
                            case TK_FLOAT:  
                                type_matches = (actual_type == OT_FLOAT || actual_type == OT_INTEGER);  // allow int promotion to float
                                break;
                            case TK_STRING: 
                                type_matches = (actual_type == OT_STRING); 
                                break;
                            case TK_BOOL:   
                                type_matches = (actual_type == OT_BOOL); 
                                break;
                        }

                        if (!type_matches) {
                            Raise_Error(_SC("type mismatch: function declared return type does not match returned value"));
                            GS_THROW();
                        }
                    }
                }
                ///////////////////////////////////////

                if(Return(arg0, arg1, temp_reg)){
                    assert(traps==0);
                    _Swap(outres,temp_reg);
                    return true;
                }
                continue;
            case _OP_LOADNULLS:{ for(GSInt32 n=0; n < arg1; n++) STK(arg0+n).Null(); }continue;
            case _OP_LOADROOT:  {
                GSWeakRef *w = _closure(ci->_closure)->_root;
                if(GS_type(w->_obj) != OT_NULL) {
                    TARGET = w->_obj;
                } else {
                    TARGET = _roottable; //shoud this be like this? or null
                }
                                }
                continue;
            case _OP_LOADBOOL: TARGET = arg1?true:false; continue;
            case _OP_DMOVE: STK(arg0) = STK(arg1); STK(arg2) = STK(arg3); continue;
            case _OP_JMP: ci->_ip += (sarg1); continue;
            //case _OP_JNZ: if(!IsFalse(STK(arg0))) ci->_ip+=(sarg1); continue;
            case _OP_JCMP:
                _GUARD(CMP_OP((CmpOP)arg3,STK(arg2),STK(arg0),temp_reg));
                if(IsFalse(temp_reg)) ci->_ip+=(sarg1);
                continue;
            case _OP_JZ: if(IsFalse(STK(arg0))) ci->_ip+=(sarg1); continue;
            case _OP_GETOUTER: {
                GSClosure *cur_cls = _closure(ci->_closure);
                GSOuter *otr = _outer(cur_cls->_outervalues[arg1]);
                TARGET = *(otr->_valptr);
                }
            continue;
            case _OP_SETOUTER: {
                GSClosure *cur_cls = _closure(ci->_closure);
                GSOuter   *otr = _outer(cur_cls->_outervalues[arg1]);
                *(otr->_valptr) = STK(arg2);
                if(arg0 != 0xFF) {
                    TARGET = STK(arg2);
                }
                }
            continue;
            case _OP_NEWOBJ:
                switch(arg3) {
                    case NOT_TABLE: TARGET = GSTable::Create(_ss(this), arg1); continue;
                    case NOT_ARRAY: TARGET = GSArray::Create(_ss(this), 0); _array(TARGET)->Reserve(arg1); continue;
                    case NOT_CLASS: _GUARD(CLASS_OP(TARGET,arg1,arg2)); continue;
                    default: assert(0); continue;
                }
            case _OP_APPENDARRAY:
                {
                    GSObject val;
                    val._unVal.raw = 0;
                switch(arg2) {
                case AAT_STACK:
                    val = STK(arg1); break;
                case AAT_LITERAL:
                    val = ci->_literals[arg1]; break;
                case AAT_INT:
                    val._type = OT_INTEGER;
#ifndef _GS64
                    val._unVal.nInteger = (GSInteger)arg1;
#else
                    val._unVal.nInteger = (GSInteger)((GSInt32)arg1);
#endif
                    break;
                case AAT_FLOAT:
                    val._type = OT_FLOAT;
                    val._unVal.fFloat = *((const GSFloat *)&arg1);
                    break;
                case AAT_BOOL:
                    val._type = OT_BOOL;
                    val._unVal.nInteger = arg1;
                    break;
                default: val._type = OT_INTEGER; assert(0); break;

                }
                
                // the array is full and Raise_Error was already triggered.
                if (!_array(STK(arg0))->Append(val, this)) {
                    GS_THROW();
                }
                continue;
                }
            case _OP_TYPECHECK: {
                GSObject &val = STK(arg0);
                GSInteger expected_tok = arg1;
                GSInteger arr_size = arg2;

                bool is_array_expected = (expected_tok < 0);
                if (is_array_expected) {
                    expected_tok = -expected_tok; // restore the real token
                }

                if (is_array_expected) {
                    if (GS_type(val) != OT_ARRAY) {
                        Raise_Error(_SC("type mismatch: expected an array"));
                        GS_THROW();
                    }

                    GSArray* arr_obj = _array(val);

                    if (arr_size > 0 && arr_obj->_max_capacity == -1) {
                        arr_obj->_max_capacity = arr_size;
                    }

                    if (arr_size > 0 && (GSInteger)arr_obj->Size() > arr_size) {
                        Raise_Error(_SC("type mismatch: array literal exceeds strict fixed size of %d"), arr_size);
                        GS_THROW();
                    }

                    if (arr_size > 0 && arr_obj->_max_capacity == -1) {
                        arr_obj->_max_capacity = arr_size;
                    }

                    if (arr_obj->_element_type == -1) {
                        arr_obj->_element_type = expected_tok;
                    }
                    
                    for (size_t i = 0; i < arr_obj->Size(); i++) {
                        GSObject &elem = arr_obj->_values[i];
                        bool elem_ok = false;
                        switch(expected_tok) {
                            case TK_INT:    elem_ok = (GS_type(elem) == OT_INTEGER); break;
                            case TK_FLOAT:  elem_ok = (GS_type(elem) == OT_FLOAT || GS_type(elem) == OT_INTEGER); break;
                            case TK_STRING: elem_ok = (GS_type(elem) == OT_STRING); break;
                            case TK_BOOL:   elem_ok = (GS_type(elem) == OT_BOOL); break;
                            case TK_IDENTIFIER: elem_ok = true; break; 
                        }
                        if (!elem_ok) {
                            Raise_Error(_SC("type mismatch: array element at index %d does not match expected type"), (GSInteger)i);
                            GS_THROW();
                        }
                    }
                    
                    continue;
                }

                bool ok = false;
                switch(expected_tok) {
                    case TK_INT:    ok = (GS_type(val) == OT_INTEGER); break;
                    case TK_FLOAT:  ok = (GS_type(val) == OT_FLOAT || GS_type(val) == OT_INTEGER); break;
                    case TK_STRING: ok = (GS_type(val) == OT_STRING); break;
                    case TK_BOOL:   ok = (GS_type(val) == OT_BOOL); break;
                    case TK_IDENTIFIER: ok = true; break; // custom classes handle their own validation later
                }

                if (!ok) {
                    Raise_Error(_SC("strict type mismatch: invalid assignment"));
                    GS_THROW();
                }
                continue;
            }
            case _OP_MATCHTYPES: {
                if (GS_type(STK(arg0)) != GS_type(STK(arg1))) {
                    Raise_Error(_SC("strict type mismatch: generic parameters bound to the same type must match"));
                    GS_THROW();
                }
                continue;
            }
            case _OP_COMPARITH: {
                GSInteger selfidx = (((GSUnsignedInteger)arg1&0xFFFF0000)>>16);
                _GUARD(DerefInc(arg3, TARGET, STK(selfidx), STK(arg2), STK(arg1&0x0000FFFF), false, selfidx));
                                }
                continue;
            case _OP_INC: {GSObjectPtr o(sarg3); _GUARD(DerefInc('+',TARGET, STK(arg1), STK(arg2), o, false, arg1));} continue;
            case _OP_INCL: {
                GSObjectPtr &a = STK(arg1);
                if(GS_type(a) == OT_INTEGER) {
                    a._unVal.nInteger = _integer(a) + sarg3;
                }
                else {
                    GSObjectPtr o(sarg3); //_GUARD(LOCAL_INC('+',TARGET, STK(arg1), o));
                    _ARITH_(+,a,a,o);
                }
                           } continue;
            case _OP_PINC: {GSObjectPtr o(sarg3); _GUARD(DerefInc('+',TARGET, STK(arg1), STK(arg2), o, true, arg1));} continue;
            case _OP_PINCL: {
                GSObjectPtr &a = STK(arg1);
                if(GS_type(a) == OT_INTEGER) {
                    TARGET = a;
                    a._unVal.nInteger = _integer(a) + sarg3;
                }
                else {
                    GSObjectPtr o(sarg3); _GUARD(PLOCAL_INC('+',TARGET, STK(arg1), o));
                }

                        } continue;
            case _OP_CMP:   _GUARD(CMP_OP((CmpOP)arg3,STK(arg2),STK(arg1),TARGET))  continue;
            case _OP_EXISTS: TARGET = Get(STK(arg1), STK(arg2), temp_reg, GET_FLAG_DO_NOT_RAISE_ERROR | GET_FLAG_RAW, DONT_FALL_BACK) ? true : false; continue;
            case _OP_INSTANCEOF:
                if(GS_type(STK(arg1)) != OT_CLASS)
                {Raise_Error(_SC("cannot apply instanceof between a %s and a %s"),GetTypeName(STK(arg1)),GetTypeName(STK(arg2))); GS_THROW();}
                TARGET = (GS_type(STK(arg2)) == OT_INSTANCE) ? (_instance(STK(arg2))->InstanceOf(_class(STK(arg1)))?true:false) : false;
                continue;
            case _OP_AND:
                if(IsFalse(STK(arg2))) {
                    TARGET = STK(arg2);
                    ci->_ip += (sarg1);
                }
                continue;
            case _OP_OR:
                if(!IsFalse(STK(arg2))) {
                    TARGET = STK(arg2);
                    ci->_ip += (sarg1);
                }
                continue;
            case _OP_NEG: _GUARD(NEG_OP(TARGET,STK(arg1))); continue;
            case _OP_NOT: TARGET = IsFalse(STK(arg1)); continue;
            case _OP_BWNOT:
                if(GS_type(STK(arg1)) == OT_INTEGER) {
                    GSInteger t = _integer(STK(arg1));
                    TARGET = GSInteger(~t);
                    continue;
                }
                Raise_Error(_SC("attempt to perform a bitwise op on a %s"), GetTypeName(STK(arg1)));
                GS_THROW();
            case _OP_CLOSURE: {
                GSClosure *c = ci->_closure._unVal.pClosure;
                GSFunctionProto *fp = c->_function;
                if(!CLOSURE_OP(TARGET,fp->_functions[arg1]._unVal.pFunctionProto,arg2)) { GS_THROW(); }
                continue;
            }
            case _OP_YIELD:{
                if(ci->_generator) {
                    if(sarg1 != MAX_FUNC_STACKSIZE) temp_reg = STK(arg1);
					if (_openouters) CloseOuters(&_stack._vals[_stackbase]);
                    _GUARD(ci->_generator->Yield(this,arg2));
					traps -= ci->_etraps;
                    if(sarg1 != MAX_FUNC_STACKSIZE) _Swap(STK(arg1),temp_reg);//STK(arg1) = temp_reg;
                }
                else { Raise_Error(_SC("trying to yield a '%s',only genenerator can be yielded"), GetTypeName(ci->_generator)); GS_THROW();}
                if(Return(arg0, arg1, temp_reg)){
                    assert(traps == 0);
                    outres = temp_reg;
                    return true;
                }

                }
                continue;
            case _OP_RESUME:
                if(GS_type(STK(arg1)) != OT_GENERATOR){ Raise_Error(_SC("trying to resume a '%s',only genenerator can be resumed"), GetTypeName(STK(arg1))); GS_THROW();}
                _GUARD(_generator(STK(arg1))->Resume(this, TARGET));
                traps += ci->_etraps;
                continue;
            case _OP_FOREACH:{ int tojump;
                _GUARD(FOREACH_OP(STK(arg0),STK(arg2),STK(arg2+1),STK(arg2+2),arg2,sarg1,tojump));
                ci->_ip += tojump; }
                continue;
            case _OP_POSTFOREACH:
                assert(GS_type(STK(arg0)) == OT_GENERATOR);
                if(_generator(STK(arg0))->_state == GSGenerator::eDead)
                    ci->_ip += (sarg1 - 1);
                continue;
            case _OP_CLONE: _GUARD(Clone(STK(arg1), TARGET)); continue;
            case _OP_TYPEOF: _GUARD(TypeOf(STK(arg1), TARGET)) continue;
            case _OP_PUSHTRAP:{
                GSInstruction *_iv = _closure(ci->_closure)->_function->_instructions;
                _etraps.push_back(GSExceptionTrap(_top,_stackbase, &_iv[(ci->_ip-_iv)+arg1], arg0)); traps++;
                ci->_etraps++;
                              }
                continue;
            case _OP_POPTRAP: {
                for(GSInteger i = 0; i < arg0; i++) {
                    _etraps.pop_back(); traps--;
                    ci->_etraps--;
                }
                              }
                continue;
            case _OP_THROW: Raise_Error(TARGET); GS_THROW(); continue;
            case _OP_NEWSLOTA:
                _GUARD(NewSlotA(STK(arg1),STK(arg2),STK(arg3),(arg0&NEW_SLOT_ATTRIBUTES_FLAG) ? STK(arg2-1) : GSObjectPtr(),(arg0&NEW_SLOT_STATIC_FLAG)?true:false,false));
                continue;
            case _OP_GETBASE:{
                GSClosure *clo = _closure(ci->_closure);
                if(clo->_base) {
                    TARGET = clo->_base;
                }
                else {
                    TARGET.Null();
                }
                continue;
            }
            case _OP_CLOSE:
                if(_openouters) CloseOuters(&(STK(arg1)));
                continue;
            }

        }
    }
exception_trap:
    {
        GSObjectPtr currerror = _lasterror;
//      dumpstack(_stackbase);
//      GSInteger n = 0;
        GSInteger last_top = _top;

        if(_ss(this)->_notifyallexceptions || (!traps && raiseerror)) CallErrorHandler(currerror);

        while( ci ) {
            if(ci->_etraps > 0) {
                GSExceptionTrap &et = _etraps.top();
                ci->_ip = et._ip;
                _top = et._stacksize;
                _stackbase = et._stackbase;
                _stack._vals[_stackbase + et._extarget] = currerror;
                _etraps.pop_back(); traps--; ci->_etraps--;
                while(last_top >= _top) _stack._vals[last_top--].Null();
                goto exception_restore;
            }
            else if (_debughook) {
                    //notify debugger of a "return"
                    //even if it really an exception unwinding the stack
                    for(GSInteger i = 0; i < ci->_ncalls; i++) {
                        CallDebugHook(_SC('r'));
                    }
            }
            if(ci->_generator) ci->_generator->Kill();
            bool mustbreak = ci && ci->_root;
            LeaveFrame();
            if(mustbreak) break;
        }

        _lasterror = currerror;
        return false;
    }
    assert(0);
    return false;
}

bool GSVM::CreateClassInstance(GSClass *theclass, GSObjectPtr &inst, GSObjectPtr &constructor)
{
    inst = theclass->CreateInstance();
    if(!theclass->GetConstructor(constructor)) {
        constructor.Null();
    }
    return true;
}

void GSVM::CallErrorHandler(GSObjectPtr &error)
{
    if(GS_type(_errorhandler) != OT_NULL) {
        GSObjectPtr out;
        Push(_roottable); Push(error);
        Call(_errorhandler, 2, _top-2, out,GSFalse);
        Pop(2);
    }
}


void GSVM::CallDebugHook(GSInteger type,GSInteger forcedline)
{
    _debughook = false;
    GSFunctionProto *func=_closure(ci->_closure)->_function;
    if(_debughook_native) {
        const GSChar *src = GS_type(func->_sourcename) == OT_STRING?_stringval(func->_sourcename):NULL;
        const GSChar *fname = GS_type(func->_name) == OT_STRING?_stringval(func->_name):NULL;
        GSInteger line = forcedline?forcedline:func->GetLine(ci->_ip);
        _debughook_native(this,type,src,line,fname);
    }
    else {
        GSObjectPtr temp_reg;
        GSInteger nparams=5;
        Push(_roottable); Push(type); Push(func->_sourcename); Push(forcedline?forcedline:func->GetLine(ci->_ip)); Push(func->_name);
        Call(_debughook_closure,nparams,_top-nparams,temp_reg,GSFalse);
        Pop(nparams);
    }
    _debughook = true;
}

bool GSVM::CallNative(GSNativeClosure *nclosure, GSInteger nargs, GSInteger newbase, GSObjectPtr &retval, GSInt32 target,bool &suspend, bool &tailcall)
{
    GSInteger nparamscheck = nclosure->_nparamscheck;
    GSInteger newtop = newbase + nargs + nclosure->_noutervalues;

    if (_nnativecalls + 1 > MAX_NATIVE_CALLS) {
        Raise_Error(_SC("Native stack overflow"));
        return false;
    }

    if(nparamscheck && (((nparamscheck > 0) && (nparamscheck != nargs)) ||
        ((nparamscheck < 0) && (nargs < (-nparamscheck)))))
    {
        Raise_Error(_SC("wrong number of parameters"));
        return false;
    }

    GSInteger tcs;
    GSIntVec &tc = nclosure->_typecheck;
    if((tcs = tc.size())) {
        for(GSInteger i = 0; i < nargs && i < tcs; i++) {
            if((tc._vals[i] != -1) && !(GS_type(_stack._vals[newbase+i]) & tc._vals[i])) {
                Raise_ParamTypeError(i,tc._vals[i], GS_type(_stack._vals[newbase+i]));
                return false;
            }
        }
    }

    if(!EnterFrame(newbase, newtop, false)) return false;
    ci->_closure  = nclosure;
	ci->_target = target;

    GSInteger outers = nclosure->_noutervalues;
    for (GSInteger i = 0; i < outers; i++) {
        _stack._vals[newbase+nargs+i] = nclosure->_outervalues[i];
    }
    if(nclosure->_env) {
        _stack._vals[newbase] = nclosure->_env->_obj;
    }

    _nnativecalls++;
    GSInteger ret = (nclosure->_function)(this);
    _nnativecalls--;

    suspend = false;
	tailcall = false;
	if (ret == GS_TAILCALL_FLAG) {
		tailcall = true;
		return true;
	}
    else if (ret == GS_SUSPEND_FLAG) {
        suspend = true;
    }
    else if (ret < 0) {
        LeaveFrame();
        Raise_Error(_lasterror);
        return false;
    }
    if(ret) {
        retval = _stack._vals[_top-1];
    }
    else {
        retval.Null();
    }
    //retval = ret ? _stack._vals[_top-1] : _null_;
    LeaveFrame();
    return true;
}

bool GSVM::TailCall(GSClosure *closure, GSInteger parambase,GSInteger nparams)
{
	GSInteger last_top = _top;
	GSObjectPtr clo = closure;
	if (ci->_root)
	{
		Raise_Error("root calls cannot invoke tailcalls");
		return false;
	}
	for (GSInteger i = 0; i < nparams; i++) STK(i) = STK(parambase + i);
	bool ret = StartCall(closure, ci->_target, nparams, _stackbase, true);
	if (last_top >= _top) {
		_top = last_top;
	}
	return ret;
}

#define FALLBACK_OK         0
#define FALLBACK_NO_MATCH   1
#define FALLBACK_ERROR      2

bool GSVM::Get(const GSObjectPtr &self, const GSObjectPtr &key, GSObjectPtr &dest, GSUnsignedInteger getflags, GSInteger selfidx)
{
    switch(GS_type(self)){
    case OT_TABLE:
        if(_table(self)->Get(key,dest))return true;
        break;
    case OT_ARRAY:
        if (GS_isnumeric(key)) { if (_array(self)->Get(tointeger(key), dest)) { return true; } if ((getflags & GET_FLAG_DO_NOT_RAISE_ERROR) == 0) Raise_IdxError(key); return false; }
        break;
    case OT_INSTANCE:
        if(_instance(self)->Get(key,dest)) return true;
        break;
    case OT_CLASS:
        if(_class(self)->Get(key,dest)) return true;
        break;
    case OT_STRING:
        if(GS_isnumeric(key)){
            GSInteger n = tointeger(key);
            GSInteger len = _string(self)->_len;
            if (n < 0) { n += len; }
            if (n >= 0 && n < len) {
                dest = GSInteger(_stringval(self)[n]);
                return true;
            }
            if ((getflags & GET_FLAG_DO_NOT_RAISE_ERROR) == 0) Raise_IdxError(key);
            return false;
        }
        break;
    default:break; //shut up compiler
    }
    if ((getflags & GET_FLAG_RAW) == 0) {
        switch(FallBackGet(self,key,dest)) {
            case FALLBACK_OK: return true; //okie
            case FALLBACK_NO_MATCH: break; //keep falling back
            case FALLBACK_ERROR: return false; // the metamethod failed
        }
        if(InvokeDefaultDelegate(self,key,dest)) {
            return true;
        }
    }
//#ifdef ROOT_FALLBACK
    if(selfidx == 0) {
        GSWeakRef *w = _closure(ci->_closure)->_root;
        if(GS_type(w->_obj) != OT_NULL)
        {
            if(Get(*((const GSObjectPtr *)&w->_obj),key,dest,0,DONT_FALL_BACK)) return true;
        }
    }
//#endif

    // NEW DYNAMIC IMPORT FALLBACK
    if(selfidx == 0) {
        GSClosure *cur_cls = _closure(ci->_closure);
        
        for(size_t i = 0; i < cur_cls->_imports.size(); i++) {
            GSObjectPtr import_str = cur_cls->_imports[i];
            GSObjectPtr target_table;
            
            if(!ResolveNamespaceTable(this, _roottable, import_str, target_table)) {
                
                GSChar filepath[1024];
                const GSChar* src = _stringval(import_str);
                int c = 0;
                while (*src && c < 1019) {
                    filepath[c++] = (*src == _SC('/')) ? _SC('/') : *src;
                    src++;
                }
                
                // append ".gs" extension
                filepath[c++] = _SC('.');
                filepath[c++] = _SC('g');
                filepath[c++] = _SC('s');
                filepath[c] = _SC('\0');
                
                FILE *f = NULL;
#ifdef GSUNICODE
                f = _wfopen(filepath, _SC("rb"));
#else
                f = fopen(filepath, _SC("rb"));
#endif
                
                if (f) {
                    GSObjectPtr closure_out;
                    if (Compile(this, ImportFileRead, f, filepath, closure_out, true, true)) {
                        GSClosure *closure = GSClosure::Create(_ss(this), _funcproto(closure_out), _table(_roottable)->GetWeakRef(OT_TABLE));
                        GSObjectPtr closure_obj = closure;

                        GSInteger oldtop = _top;
    
                        Push(_roottable); 
    
                        GSObjectPtr out;
                        Call(closure_obj, 1, oldtop, out, GSTrue);
    
                        _top = oldtop; // restore stack
                    }
                    fclose(f);
                } 
                else {
                    Raise_Error(_SC("cannot load imported module '%s' (file not found: %s)"), _stringval(import_str), filepath);
                    return false;
                }
            }
            
            if(ResolveNamespaceTable(this, _roottable, import_str, target_table)) {
                
                // re-check the root table! 
                GSWeakRef *w = _closure(ci->_closure)->_root;
                GSObjectPtr root_obj = (GS_type(w->_obj) != OT_NULL) ? *((const GSObjectPtr *)&w->_obj) : _roottable;
                
                if(Get(root_obj, key, dest, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
                    return true;
                }

                // check the target table! 
                if(Get(target_table, key, dest, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
                    return true;
                }
            } 
            else {
                Raise_Error(_SC("module '%s' was loaded but did not declare the expected namespace"), _stringval(import_str));
                return false;
            }
        }
    }

    if ((getflags & GET_FLAG_DO_NOT_RAISE_ERROR) == 0) Raise_IdxError(key);
    return false;
}

bool GSVM::InvokeDefaultDelegate(const GSObjectPtr &self,const GSObjectPtr &key,GSObjectPtr &dest)
{
    GSTable *ddel = NULL;
    switch(GS_type(self)) {
        case OT_CLASS: ddel = _class_ddel; break;
        case OT_TABLE: ddel = _table_ddel; break;
        case OT_ARRAY: ddel = _array_ddel; break;
        case OT_STRING: ddel = _string_ddel; break;
        case OT_INSTANCE: ddel = _instance_ddel; break;
        case OT_INTEGER:case OT_FLOAT:case OT_BOOL: ddel = _number_ddel; break;
        case OT_GENERATOR: ddel = _generator_ddel; break;
        case OT_CLOSURE: case OT_NATIVECLOSURE: ddel = _closure_ddel; break;
        case OT_THREAD: ddel = _thread_ddel; break;
        case OT_WEAKREF: ddel = _weakref_ddel; break;
        default: return false;
    }
    return  ddel->Get(key,dest);
}


GSInteger GSVM::FallBackGet(const GSObjectPtr &self,const GSObjectPtr &key,GSObjectPtr &dest)
{
    switch(GS_type(self)){
    case OT_TABLE:
    case OT_USERDATA:
        //delegation
        if(_delegable(self)->_delegate) {
            if(Get(GSObjectPtr(_delegable(self)->_delegate),key,dest, GET_FLAG_DO_NOT_RAISE_ERROR,DONT_FALL_BACK)) return FALLBACK_OK;
        }
        else {
            return FALLBACK_NO_MATCH;
        }
        //go through
    case OT_INSTANCE: {
        GSObjectPtr closure;
        if(_delegable(self)->GetMetaMethod(this, MT_GET, closure)) {
            Push(self);Push(key);
            _nmetamethodscall++;
            AutoDec ad(&_nmetamethodscall);
            if(Call(closure, 2, _top - 2, dest, GSFalse)) {
                Pop(2);
                return FALLBACK_OK;
            }
            else {
                Pop(2);
                if(GS_type(_lasterror) != OT_NULL) { //NULL means "clean failure" (not found)
                    return FALLBACK_ERROR;
                }
            }
        }
                      }
        break;
    default: break;//shutup GCC 4.x
    }
    // no metamethod or no fallback type
    return FALLBACK_NO_MATCH;
}

bool GSVM::Set(const GSObjectPtr &self,const GSObjectPtr &key,const GSObjectPtr &val,GSInteger selfidx)
{
    switch(GS_type(self)){
    case OT_TABLE:
        if(_table(self)->Set(key,val)) return true;
        break;
    case OT_INSTANCE:
        if(_instance(self)->Set(key,val)) return true;
        break;
    case OT_ARRAY:
        if(!GS_isnumeric(key)) { Raise_Error(_SC("indexing %s with %s"),GetTypeName(self),GetTypeName(key)); return false; }
        if(!_array(self)->Set(tointeger(key),val)) {
            Raise_IdxError(key);
            return false;
        }
        return true;
  	case OT_USERDATA: break; // must fall back
    default:
        Raise_Error(_SC("trying to set '%s'"),GetTypeName(self));
        return false;
    }

    switch(FallBackSet(self,key,val)) {
        case FALLBACK_OK: return true; //okie
        case FALLBACK_NO_MATCH: break; //keep falling back
        case FALLBACK_ERROR: return false; // the metamethod failed
    }
    if(selfidx == 0) {
        if(_table(_roottable)->Set(key,val))
            return true;
    }
    Raise_IdxError(key);
    return false;
}

GSInteger GSVM::FallBackSet(const GSObjectPtr &self,const GSObjectPtr &key,const GSObjectPtr &val)
{
    switch(GS_type(self)) {
    case OT_TABLE:
        if(_table(self)->_delegate) {
            if(Set(_table(self)->_delegate,key,val,DONT_FALL_BACK)) return FALLBACK_OK;
        }
        //keps on going
    case OT_INSTANCE:
    case OT_USERDATA:{
        GSObjectPtr closure;
        GSObjectPtr t;
        if(_delegable(self)->GetMetaMethod(this, MT_SET, closure)) {
            Push(self);Push(key);Push(val);
            _nmetamethodscall++;
            AutoDec ad(&_nmetamethodscall);
            if(Call(closure, 3, _top - 3, t, GSFalse)) {
                Pop(3);
                return FALLBACK_OK;
            }
            else {
                Pop(3);
                if(GS_type(_lasterror) != OT_NULL) { //NULL means "clean failure" (not found)
                    return FALLBACK_ERROR;
                }
            }
        }
                     }
        break;
        default: break;//shutup GCC 4.x
    }
    // no metamethod or no fallback type
    return FALLBACK_NO_MATCH;
}

bool GSVM::Clone(const GSObjectPtr &self,GSObjectPtr &target)
{
    GSObjectPtr temp_reg;
    GSObjectPtr newobj;
    switch(GS_type(self)){
    case OT_TABLE:
        newobj = _table(self)->Clone();
        goto cloned_mt;
    case OT_INSTANCE: {
        newobj = _instance(self)->Clone(_ss(this));
cloned_mt:
        GSObjectPtr closure;
        if(_delegable(newobj)->_delegate && _delegable(newobj)->GetMetaMethod(this,MT_CLONED,closure)) {
            Push(newobj);
            Push(self);
            if(!CallMetaMethod(closure,MT_CLONED,2,temp_reg))
                return false;
        }
        }
        target = newobj;
        return true;
    case OT_ARRAY:
        target = _array(self)->Clone();
        return true;
    default:
        Raise_Error(_SC("cloning a %s"), GetTypeName(self));
        return false;
    }
}

bool GSVM::NewSlotA(const GSObjectPtr &self,const GSObjectPtr &key,const GSObjectPtr &val,const GSObjectPtr &attrs,bool bstatic,bool raw)
{
    if(GS_type(self) != OT_CLASS) {
        Raise_Error(_SC("object must be a class"));
        return false;
    }
    GSClass *c = _class(self);
    if(!raw) {
        GSObjectPtr &mm = c->_metamethods[MT_NEWMEMBER];
        if(GS_type(mm) != OT_NULL ) {
            Push(self); Push(key); Push(val);
            Push(attrs);
            Push(bstatic);
            return CallMetaMethod(mm,MT_NEWMEMBER,5,temp_reg);
        }
    }
    if(!NewSlot(self, key, val,bstatic))
        return false;
    if(GS_type(attrs) != OT_NULL) {
        c->SetAttributes(key,attrs);
    }
    return true;
}

bool GSVM::NewSlot(const GSObjectPtr &self,const GSObjectPtr &key,const GSObjectPtr &val,bool bstatic)
{
    if(GS_type(key) == OT_NULL) { Raise_Error(_SC("null cannot be used as index")); return false; }
    switch(GS_type(self)) {
    case OT_TABLE: {
        bool rawcall = true;
        if(_table(self)->_delegate) {
            GSObjectPtr res;
            if(!_table(self)->Get(key,res)) {
                GSObjectPtr closure;
                if(_delegable(self)->_delegate && _delegable(self)->GetMetaMethod(this,MT_NEWSLOT,closure)) {
                    Push(self);Push(key);Push(val);
                    if(!CallMetaMethod(closure,MT_NEWSLOT,3,res)) {
                        return false;
                    }
                    rawcall = false;
                }
                else {
                    rawcall = true;
                }
            }
        }
        if(rawcall) _table(self)->NewSlot(key,val); //cannot fail

        break;}
    case OT_INSTANCE: {
        GSObjectPtr res;
        GSObjectPtr closure;
        if(_delegable(self)->_delegate && _delegable(self)->GetMetaMethod(this,MT_NEWSLOT,closure)) {
            Push(self);Push(key);Push(val);
            if(!CallMetaMethod(closure,MT_NEWSLOT,3,res)) {
                return false;
            }
            break;
        }
        Raise_Error(_SC("class instances do not support the new slot operator"));
        return false;
        break;}
    case OT_CLASS:
        if(!_class(self)->NewSlot(_ss(this),key,val,bstatic)) {
            if(_class(self)->_locked) {
                Raise_Error(_SC("trying to modify a class that has already been instantiated"));
                return false;
            }
            else {
                GSObjectPtr oval = PrintObjVal(key);
                Raise_Error(_SC("the property '%s' already exists"),_stringval(oval));
                return false;
            }
        }
        break;
    default:
        Raise_Error(_SC("indexing %s with %s"),GetTypeName(self),GetTypeName(key));
        return false;
        break;
    }
    return true;
}



bool GSVM::DeleteSlot(const GSObjectPtr &self,const GSObjectPtr &key,GSObjectPtr &res)
{
    switch(GS_type(self)) {
    case OT_TABLE:
    case OT_INSTANCE:
    case OT_USERDATA: {
        GSObjectPtr t;
        //bool handled = false;
        GSObjectPtr closure;
        if(_delegable(self)->_delegate && _delegable(self)->GetMetaMethod(this,MT_DELSLOT,closure)) {
            Push(self);Push(key);
            return CallMetaMethod(closure,MT_DELSLOT,2,res);
        }
        else {
            if(GS_type(self) == OT_TABLE) {
                if(_table(self)->Get(key,t)) {
                    _table(self)->Remove(key);
                }
                else {
                    Raise_IdxError((const GSObject &)key);
                    return false;
                }
            }
            else {
                Raise_Error(_SC("cannot delete a slot from %s"),GetTypeName(self));
                return false;
            }
        }
        res = t;
                }
        break;
    default:
        Raise_Error(_SC("attempt to delete a slot from a %s"),GetTypeName(self));
        return false;
    }
    return true;
}

bool GSVM::Call(GSObjectPtr &closure,GSInteger nparams,GSInteger stackbase,GSObjectPtr &outres,GSBool raiseerror)
{
#ifdef _DEBUG
GSInteger prevstackbase = _stackbase;
#endif
    switch(GS_type(closure)) {
    case OT_CLOSURE:
        return Execute(closure, nparams, stackbase, outres, raiseerror);
        break;
    case OT_NATIVECLOSURE:{
        bool dummy;
        return CallNative(_nativeclosure(closure), nparams, stackbase, outres, -1, dummy, dummy);

                          }
        break;
    case OT_CLASS: {
        GSObjectPtr constr;
        GSObjectPtr temp;
        CreateClassInstance(_class(closure),outres,constr);
        GSObjectType ctype = GS_type(constr);
        if (ctype == OT_NATIVECLOSURE || ctype == OT_CLOSURE) {
            _stack[stackbase] = outres;
            return Call(constr,nparams,stackbase,temp,raiseerror);
        }
        return true;
                   }
        break;
    default:
        Raise_Error(_SC("attempt to call '%s'"), GetTypeName(closure));
        return false;
    }
#ifdef _DEBUG
    if(!_suspended) {
        assert(_stackbase == prevstackbase);
    }
#endif
    return true;
}

bool GSVM::CallMetaMethod(GSObjectPtr &closure,GSMetaMethod GS_UNUSED_ARG(mm),GSInteger nparams,GSObjectPtr &outres)
{
    //GSObjectPtr closure;

    _nmetamethodscall++;
    if(Call(closure, nparams, _top - nparams, outres, GSFalse)) {
        _nmetamethodscall--;
        Pop(nparams);
        return true;
    }
    _nmetamethodscall--;
    //}
    Pop(nparams);
    return false;
}

void GSVM::FindOuter(GSObjectPtr &target, GSObjectPtr *stackindex)
{
    GSOuter **pp = &_openouters;
    GSOuter *p;
    GSOuter *otr;

    while ((p = *pp) != NULL && p->_valptr >= stackindex) {
        if (p->_valptr == stackindex) {
            target = GSObjectPtr(p);
            return;
        }
        pp = &p->_next;
    }
    otr = GSOuter::Create(_ss(this), stackindex);
    otr->_next = *pp;
    otr->_idx  = (stackindex - _stack._vals);
    __ObjAddRef(otr);
    *pp = otr;
    target = GSObjectPtr(otr);
}

bool GSVM::EnterFrame(GSInteger newbase, GSInteger newtop, bool tailcall)
{
    if( !tailcall ) {
        if( _callsstacksize == _alloccallsstacksize ) {
            GrowCallStack();
        }
        ci = &_callsstack[_callsstacksize++];
        ci->_prevstkbase = (GSInt32)(newbase - _stackbase);
        ci->_prevtop = (GSInt32)(_top - _stackbase);
        ci->_etraps = 0;
        ci->_ncalls = 1;
        ci->_generator = NULL;
        ci->_root = GSFalse;
    }
    else {
        ci->_ncalls++;
    }

    _stackbase = newbase;
    _top = newtop;
    if(newtop + MIN_STACK_OVERHEAD > (GSInteger)_stack.size()) {
        if(_nmetamethodscall) {
            Raise_Error(_SC("stack overflow, cannot resize stack while in a metamethod"));
            return false;
        }
        _stack.resize(newtop + (MIN_STACK_OVERHEAD << 2));
        RelocateOuters();
    }
    return true;
}

void GSVM::LeaveFrame() {
    GSInteger last_top = _top;
    GSInteger last_stackbase = _stackbase;
    GSInteger css = --_callsstacksize;

    /* First clean out the call stack frame */
    ci->_closure.Null();
    _stackbase -= ci->_prevstkbase;
    _top = _stackbase + ci->_prevtop;
    ci = (css) ? &_callsstack[css-1] : NULL;

    if(_openouters) CloseOuters(&(_stack._vals[last_stackbase]));
    while (last_top >= _top) {
        _stack._vals[last_top--].Null();
    }
}

void GSVM::RelocateOuters()
{
    GSOuter *p = _openouters;
    while (p) {
        p->_valptr = _stack._vals + p->_idx;
        p = p->_next;
    }
}

void GSVM::CloseOuters(GSObjectPtr *stackindex) {
  GSOuter *p;
  while ((p = _openouters) != NULL && p->_valptr >= stackindex) {
    p->_value = *(p->_valptr);
    p->_valptr = &p->_value;
    _openouters = p->_next;
    __ObjRelease(p);
  }
}

void GSVM::Remove(GSInteger n) {
    n = (n >= 0)?n + _stackbase - 1:_top + n;
    for(GSInteger i = n; i < _top; i++){
        _stack[i] = _stack[i+1];
    }
    _stack[_top].Null();
    _top--;
}

void GSVM::Pop() {
    _stack[--_top].Null();
}

void GSVM::Pop(GSInteger n) {
    for(GSInteger i = 0; i < n; i++){
        _stack[--_top].Null();
    }
}

void GSVM::PushNull() { _stack[_top++].Null(); }
void GSVM::Push(const GSObjectPtr &o) { _stack[_top++] = o; }
GSObjectPtr &GSVM::Top() { return _stack[_top-1]; }
GSObjectPtr &GSVM::PopGet() { return _stack[--_top]; }
GSObjectPtr &GSVM::GetUp(GSInteger n) { return _stack[_top+n]; }
GSObjectPtr &GSVM::GetAt(GSInteger n) { return _stack[n]; }

bool GSArray::Append(const GSObject &o, GSVM *vm) {
    if (_max_capacity != -1 && _values.size() >= (size_t)_max_capacity) {
        if (vm) {
            vm->Raise_Error(_SC("array bounds exceeded: attempted to push element %d into a fixed array of size %d"), 
                            (GSInteger)(_values.size() + 1), _max_capacity);
        }
        return false;
    }

    if (_element_type != -1) {
        bool elem_ok = false;
        switch(_element_type) {
            case TK_INT:    elem_ok = (GS_type(o) == OT_INTEGER); break;
            case TK_FLOAT:  elem_ok = (GS_type(o) == OT_FLOAT || GS_type(o) == OT_INTEGER); break;
            case TK_STRING: elem_ok = (GS_type(o) == OT_STRING); break;
            case TK_BOOL:   elem_ok = (GS_type(o) == OT_BOOL); break;
            case TK_IDENTIFIER: elem_ok = true; break; 
        }
        if (!elem_ok) {
            if (vm) {
                vm->Raise_Error(_SC("strict type mismatch: attempted to append invalid type to array"));
            }
            return false;
        }
    }
    
    _values.push_back(o);
    return true;
}

#ifdef _DEBUG_DUMP
void GSVM::dumpstack(GSInteger stackbase,bool dumpall)
{
    GSInteger size=dumpall?_stack.size():_top;
    GSInteger n=0;
    scprintf(_SC("\n>>>>stack dump<<<<\n"));
    CallInfo &ci=_callsstack[_callsstacksize-1];
    scprintf(_SC("IP: %p\n"),ci._ip);
    scprintf(_SC("prev stack base: %d\n"),ci._prevstkbase);
    scprintf(_SC("prev top: %d\n"),ci._prevtop);
    for(GSInteger i=0;i<size;i++){
        GSObjectPtr &obj=_stack[i];
        if(stackbase==i)scprintf(_SC(">"));else scprintf(_SC(" "));
        scprintf(_SC("[") _PRINT_INT_FMT _SC("]:"),n);
        switch(GS_type(obj)){
        case OT_FLOAT:          scprintf(_SC("FLOAT %.3f"),_float(obj));break;
        case OT_INTEGER:        scprintf(_SC("INTEGER ") _PRINT_INT_FMT,_integer(obj));break;
        case OT_BOOL:           scprintf(_SC("BOOL %s"),_integer(obj)?_SC("true"):_SC("false"));break;
        case OT_STRING:         scprintf(_SC("STRING %s"),_stringval(obj));break;
        case OT_NULL:           scprintf(_SC("NULL"));  break;
        case OT_TABLE:          scprintf(_SC("TABLE %p[%p]"),_table(obj),_table(obj)->_delegate);break;
        case OT_ARRAY:          scprintf(_SC("ARRAY %p"),_array(obj));break;
        case OT_CLOSURE:        scprintf(_SC("CLOSURE [%p]"),_closure(obj));break;
        case OT_NATIVECLOSURE:  scprintf(_SC("NATIVECLOSURE"));break;
        case OT_USERDATA:       scprintf(_SC("USERDATA %p[%p]"),_userdataval(obj),_userdata(obj)->_delegate);break;
        case OT_GENERATOR:      scprintf(_SC("GENERATOR %p"),_generator(obj));break;
        case OT_THREAD:         scprintf(_SC("THREAD [%p]"),_thread(obj));break;
        case OT_USERPOINTER:    scprintf(_SC("USERPOINTER %p"),_userpointer(obj));break;
        case OT_CLASS:          scprintf(_SC("CLASS %p"),_class(obj));break;
        case OT_INSTANCE:       scprintf(_SC("INSTANCE %p"),_instance(obj));break;
        case OT_WEAKREF:        scprintf(_SC("WEAKREF %p"),_weakref(obj));break;
        default:
            assert(0);
            break;
        };
        scprintf(_SC("\n"));
        ++n;
    }
}



#endif

