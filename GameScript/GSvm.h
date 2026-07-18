/*  see copyright notice in GameScript.h */
#ifndef _GSVM_H_
#define _GSVM_H_

#include "GSopcodes.h"
#include "GSobject.h"
#define MAX_NATIVE_CALLS 100
#define MIN_STACK_OVERHEAD 15

#define GS_SUSPEND_FLAG -666
#define GS_TAILCALL_FLAG -777
#define DONT_FALL_BACK 666
//#define EXISTS_FALL_BACK -1

#define GET_FLAG_RAW                0x00000001
#define GET_FLAG_DO_NOT_RAISE_ERROR 0x00000002
//base lib
void GS_base_register(HGameScriptVM v);

struct GSExceptionTrap{
    GSExceptionTrap() {}
    GSExceptionTrap(GSInteger ss, GSInteger stackbase,GSInstruction *ip, GSInteger ex_target){ _stacksize = ss; _stackbase = stackbase; _ip = ip; _extarget = ex_target;}
    GSExceptionTrap(const GSExceptionTrap &et) { (*this) = et;  }
    GSInteger _stackbase;
    GSInteger _stacksize;
    GSInstruction *_ip;
    GSInteger _extarget;
};

#define _INLINE

typedef GSvector<GSExceptionTrap> ExceptionsTraps;

struct GSVM : public CHAINABLE_OBJ
{
    struct CallInfo{
        //CallInfo() { _generator = NULL;}
        GSInstruction *_ip;
        GSObjectPtr *_literals;
        GSObjectPtr _closure;
        GSGenerator *_generator;
        GSInt32 _etraps;
        GSInt32 _prevstkbase;
        GSInt32 _prevtop;
        GSInt32 _target;
        GSInt32 _ncalls;
        GSBool _root;
    };

typedef GSvector<CallInfo> CallInfoVec;
public:
    void DebugHookProxy(GSInteger type, const GSChar * sourcename, GSInteger line, const GSChar * funcname);
    static void _DebugHookProxy(HGameScriptVM v, GSInteger type, const GSChar * sourcename, GSInteger line, const GSChar * funcname);
    enum ExecutionType { ET_CALL, ET_RESUME_GENERATOR, ET_RESUME_VM,ET_RESUME_THROW_VM };
    GSVM(GSSharedState *ss);
    ~GSVM();
    bool Init(GSVM *friendvm, GSInteger stacksize);
    bool Execute(GSObjectPtr &func, GSInteger nargs, GSInteger stackbase, GSObjectPtr &outres, GSBool raiseerror, ExecutionType et = ET_CALL);
    //starts a native call return when the NATIVE closure returns
    bool CallNative(GSNativeClosure *nclosure, GSInteger nargs, GSInteger newbase, GSObjectPtr &retval, GSInt32 target, bool &suspend,bool &tailcall);
	bool TailCall(GSClosure *closure, GSInteger firstparam, GSInteger nparams);
    //starts a GameScript call in the same "Execution loop"
    bool StartCall(GSClosure *closure, GSInteger target, GSInteger nargs, GSInteger stackbase, bool tailcall);
    bool CreateClassInstance(GSClass *theclass, GSObjectPtr &inst, GSObjectPtr &constructor);
    //call a generic closure pure GameScript or NATIVE
    bool Call(GSObjectPtr &closure, GSInteger nparams, GSInteger stackbase, GSObjectPtr &outres,GSBool raiseerror);
    GSRESULT Suspend();

    void CallDebugHook(GSInteger type,GSInteger forcedline=0);
    void CallErrorHandler(GSObjectPtr &e);
    bool Get(const GSObjectPtr &self, const GSObjectPtr &key, GSObjectPtr &dest, GSUnsignedInteger getflags, GSInteger selfidx);
    GSInteger FallBackGet(const GSObjectPtr &self,const GSObjectPtr &key,GSObjectPtr &dest);
    bool InvokeDefaultDelegate(const GSObjectPtr &self,const GSObjectPtr &key,GSObjectPtr &dest);
    bool Set(const GSObjectPtr &self, const GSObjectPtr &key, const GSObjectPtr &val, GSInteger selfidx);
    GSInteger FallBackSet(const GSObjectPtr &self,const GSObjectPtr &key,const GSObjectPtr &val);
    bool NewSlot(const GSObjectPtr &self, const GSObjectPtr &key, const GSObjectPtr &val,bool bstatic);
    bool NewSlotA(const GSObjectPtr &self,const GSObjectPtr &key,const GSObjectPtr &val,const GSObjectPtr &attrs,bool bstatic,bool raw);
    bool DeleteSlot(const GSObjectPtr &self, const GSObjectPtr &key, GSObjectPtr &res);
    bool Clone(const GSObjectPtr &self, GSObjectPtr &target);
    bool ObjCmp(const GSObjectPtr &o1, const GSObjectPtr &o2,GSInteger &res);
    bool StringCat(const GSObjectPtr &str, const GSObjectPtr &obj, GSObjectPtr &dest);
    static bool IsEqual(const GSObjectPtr &o1,const GSObjectPtr &o2,bool &res);
    bool ToString(const GSObjectPtr &o,GSObjectPtr &res);
    GSString *PrintObjVal(const GSObjectPtr &o);


    void Raise_Error(const GSChar *s, ...);
    void Raise_Error(const GSObjectPtr &desc);
    void Raise_IdxError(const GSObjectPtr &o);
    void Raise_CompareError(const GSObject &o1, const GSObject &o2);
    void Raise_ParamTypeError(GSInteger nparam,GSInteger typemask,GSInteger type);

    void FindOuter(GSObjectPtr &target, GSObjectPtr *stackindex);
    void RelocateOuters();
    void CloseOuters(GSObjectPtr *stackindex);

    bool TypeOf(const GSObjectPtr &obj1, GSObjectPtr &dest);
    bool CallMetaMethod(GSObjectPtr &closure, GSMetaMethod mm, GSInteger nparams, GSObjectPtr &outres);
    bool ArithMetaMethod(GSInteger op, const GSObjectPtr &o1, const GSObjectPtr &o2, GSObjectPtr &dest);
    bool Return(GSInteger _arg0, GSInteger _arg1, GSObjectPtr &retval);
    //new stuff
    _INLINE bool ARITH_OP(GSUnsignedInteger op,GSObjectPtr &trg,const GSObjectPtr &o1,const GSObjectPtr &o2);
    _INLINE bool BW_OP(GSUnsignedInteger op,GSObjectPtr &trg,const GSObjectPtr &o1,const GSObjectPtr &o2);
    _INLINE bool NEG_OP(GSObjectPtr &trg,const GSObjectPtr &o1);
    _INLINE bool CMP_OP(CmpOP op, const GSObjectPtr &o1,const GSObjectPtr &o2,GSObjectPtr &res);
    bool CLOSURE_OP(GSObjectPtr &target, GSFunctionProto *func, GSInteger boundtarget);
    bool CLASS_OP(GSObjectPtr &target,GSInteger base,GSInteger attrs);
    //return true if the loop is finished
    bool FOREACH_OP(GSObjectPtr &o1,GSObjectPtr &o2,GSObjectPtr &o3,GSObjectPtr &o4,GSInteger arg_2,int exitpos,int &jump);
    //_INLINE bool LOCAL_INC(GSInteger op,GSObjectPtr &target, GSObjectPtr &a, GSObjectPtr &incr);
    _INLINE bool PLOCAL_INC(GSInteger op,GSObjectPtr &target, GSObjectPtr &a, GSObjectPtr &incr);
    _INLINE bool DerefInc(GSInteger op,GSObjectPtr &target, GSObjectPtr &self, GSObjectPtr &key, GSObjectPtr &incr, bool postfix,GSInteger arg0);
#ifdef _DEBUG_DUMP
    void dumpstack(GSInteger stackbase=-1, bool dumpall = false);
#endif

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    GSObjectType GetType() {return OT_THREAD;}
#endif
    void Finalize();
    void GrowCallStack() {
        GSInteger newsize = _alloccallsstacksize*2;
        _callstackdata.resize(newsize);
        _callsstack = &_callstackdata[0];
        _alloccallsstacksize = newsize;
    }
    bool EnterFrame(GSInteger newbase, GSInteger newtop, bool tailcall);
    void LeaveFrame();
    void Release(){ GS_delete(this,GSVM); }
////////////////////////////////////////////////////////////////////////////
    //stack functions for the api
    void Remove(GSInteger n);

    static bool IsFalse(GSObjectPtr &o);

    void Pop();
    void Pop(GSInteger n);
    void Push(const GSObjectPtr &o);
    void PushNull();
    GSObjectPtr &Top();
    GSObjectPtr &PopGet();
    GSObjectPtr &GetUp(GSInteger n);
    GSObjectPtr &GetAt(GSInteger n);

    GSObjectPtrVec _stack;

    GSInteger _top;
    GSInteger _stackbase;
    GSOuter *_openouters;
    GSObjectPtr _roottable;
    GSObjectPtr _lasterror;
    GSObjectPtr _errorhandler;

    bool _debughook;
    GSDEBUGHOOK _debughook_native;
    GSObjectPtr _debughook_closure;

    GSObjectPtr temp_reg;


    CallInfo* _callsstack;
    GSInteger _callsstacksize;
    GSInteger _alloccallsstacksize;
    GSvector<CallInfo>  _callstackdata;

    ExceptionsTraps _etraps;
    CallInfo *ci;
    GSUserPointer _foreignptr;
    //VMs sharing the same state
    GSSharedState *_sharedstate;
    GSInteger _nnativecalls;
    GSInteger _nmetamethodscall;
    GSRELEASEHOOK _releasehook;
    //suspend infos
    GSBool _suspended;
    GSBool _suspended_root;
    GSInteger _suspended_target;
    GSInteger _suspended_traps;
};

struct AutoDec{
    AutoDec(GSInteger *n) { _n = n; }
    ~AutoDec() { (*_n)--; }
    GSInteger *_n;
};

inline GSObjectPtr &stack_get(HGameScriptVM v,GSInteger idx){return ((idx>=0)?(v->GetAt(idx+v->_stackbase-1)):(v->GetUp(idx)));}

#define _ss(_vm_) (_vm_)->_sharedstate

#ifndef NO_GARBAGE_COLLECTOR
#define _opt_ss(_vm_) (_vm_)->_sharedstate
#else
#define _opt_ss(_vm_) NULL
#endif

#define PUSH_CALLINFO(v,nci){ \
    GSInteger css = v->_callsstacksize; \
    if(css == v->_alloccallsstacksize) { \
        v->GrowCallStack(); \
    } \
    v->ci = &v->_callsstack[css]; \
    *(v->ci) = nci; \
    v->_callsstacksize++; \
}

#define POP_CALLINFO(v){ \
    GSInteger css = --v->_callsstacksize; \
    v->ci->_closure.Null(); \
    v->ci = css?&v->_callsstack[css-1]:NULL;    \
}
#endif //_GSVM_H_

