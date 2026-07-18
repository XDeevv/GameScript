/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#include "GSvm.h"
#include "GSstring.h"
#include "GSarray.h"
#include "GStable.h"
#include "GSuserdata.h"
#include "GSfuncproto.h"
#include "GSclass.h"
#include "GSclosure.h"


const GSChar *IdType2Name(GSObjectType type)
{
    switch(_RAW_TYPE(type))
    {
    case _RT_NULL:return _SC("null");
    case _RT_INTEGER:return _SC("integer");
    case _RT_FLOAT:return _SC("float");
    case _RT_BOOL:return _SC("bool");
    case _RT_STRING:return _SC("string");
    case _RT_TABLE:return _SC("table");
    case _RT_ARRAY:return _SC("array");
    case _RT_GENERATOR:return _SC("generator");
    case _RT_CLOSURE:
    case _RT_NATIVECLOSURE:
        return _SC("function");
    case _RT_USERDATA:
    case _RT_USERPOINTER:
        return _SC("userdata");
    case _RT_THREAD: return _SC("thread");
    case _RT_FUNCPROTO: return _SC("function");
    case _RT_CLASS: return _SC("class");
    case _RT_INSTANCE: return _SC("instance");
    case _RT_WEAKREF: return _SC("weakref");
    case _RT_OUTER: return _SC("outer");
    default:
        return NULL;
    }
}

const GSChar *GetTypeName(const GSObjectPtr &obj1)
{
    return IdType2Name(GS_type(obj1));
}

GSString *GSString::Create(GSSharedState *ss,const GSChar *s,GSInteger len)
{
    GSString *str=ADD_STRING(ss,s,len);
    return str;
}

GSString* GSString::Concat(GSSharedState* ss, const GSChar* a, GSInteger alen, const GSChar* b, GSInteger blen)
{
    GSString* str = ss->_stringtable->Concat(a, alen, b, blen);
    return str;
}

void GSString::Release()
{
    REMOVE_STRING(_sharedstate,this);
}

GSInteger GSString::Next(const GSObjectPtr &refpos, GSObjectPtr &outkey, GSObjectPtr &outval)
{
    GSInteger idx = (GSInteger)TranslateIndex(refpos);
    while(idx < _len){
        outkey = (GSInteger)idx;
        outval = (GSInteger)((GSUnsignedInteger)_val[idx]);
        //return idx for the next iteration
        return ++idx;
    }
    //nothing to iterate anymore
    return -1;
}

GSUnsignedInteger TranslateIndex(const GSObjectPtr &idx)
{
    switch(GS_type(idx)){
        case OT_NULL:
            return 0;
        case OT_INTEGER:
            return (GSUnsignedInteger)_integer(idx);
        default: assert(0); break;
    }
    return 0;
}

GSWeakRef *GSRefCounted::GetWeakRef(GSObjectType type)
{
    if(!_weakref) {
        GS_new(_weakref,GSWeakRef);
#if defined(GSUSEDOUBLE) && !defined(_GS64)
        _weakref->_obj._unVal.raw = 0; //clean the whole union on 32 bits with double
#endif
        _weakref->_obj._type = type;
        _weakref->_obj._unVal.pRefCounted = this;
    }
    return _weakref;
}

GSRefCounted::~GSRefCounted()
{
    if(_weakref) {
        _weakref->_obj._type = OT_NULL;
        _weakref->_obj._unVal.pRefCounted = NULL;
    }
}

void GSWeakRef::Release() {
    if(ISREFCOUNTED(_obj._type)) {
        _obj._unVal.pRefCounted->_weakref = NULL;
    }
    GS_delete(this,GSWeakRef);
}

bool GSDelegable::GetMetaMethod(GSVM *v,GSMetaMethod mm,GSObjectPtr &res) {
    if(_delegate) {
        return _delegate->Get((*_ss(v)->_metamethods)[mm],res);
    }
    return false;
}

bool GSDelegable::SetDelegate(GSTable *mt)
{
    GSTable *temp = mt;
    if(temp == this) return false;
    while (temp) {
        if (temp->_delegate == this) return false; //cycle detected
        temp = temp->_delegate;
    }
    if (mt) __ObjAddRef(mt);
    __ObjRelease(_delegate);
    _delegate = mt;
    return true;
}

bool GSGenerator::Yield(GSVM *v,GSInteger target)
{
    if(_state==eSuspended) { v->Raise_Error(_SC("internal vm error, yielding dead generator"));  return false;}
    if(_state==eDead) { v->Raise_Error(_SC("internal vm error, yielding a dead generator")); return false; }
    GSInteger size = v->_top-v->_stackbase;

    _stack.resize(size);
    GSObject _this = v->_stack[v->_stackbase];
    _stack._vals[0] = ISREFCOUNTED(GS_type(_this)) ? GSObjectPtr(_refcounted(_this)->GetWeakRef(GS_type(_this))) : _this;
    for(GSInteger n =1; n<target; n++) {
        _stack._vals[n] = v->_stack[v->_stackbase+n];
    }
    for(GSInteger j =0; j < size; j++)
    {
        v->_stack[v->_stackbase+j].Null();
    }

    _ci = *v->ci;
    _ci._generator=NULL;
    for(GSInteger i=0;i<_ci._etraps;i++) {
        _etraps.push_back(v->_etraps.top());
        v->_etraps.pop_back();
        // store relative stack base and size in case of resume to other _top
        GSExceptionTrap &et = _etraps.back();
        et._stackbase -= v->_stackbase;
        et._stacksize -= v->_stackbase;
    }
    _state=eSuspended;
    return true;
}

bool GSGenerator::Resume(GSVM *v,GSObjectPtr &dest)
{
    if(_state==eDead){ v->Raise_Error(_SC("resuming dead generator")); return false; }
    if(_state==eRunning){ v->Raise_Error(_SC("resuming active generator")); return false; }
    GSInteger size = _stack.size();
    GSInteger target = &dest - &(v->_stack._vals[v->_stackbase]);
    assert(target>=0 && target<=255);
    GSInteger newbase = v->_top;
    if(!v->EnterFrame(v->_top, v->_top + size, false))
        return false;
    v->ci->_generator   = this;
    v->ci->_target      = (GSInt32)target;
    v->ci->_closure     = _ci._closure;
    v->ci->_ip          = _ci._ip;
    v->ci->_literals    = _ci._literals;
    v->ci->_ncalls      = _ci._ncalls;
    v->ci->_etraps      = _ci._etraps;
    v->ci->_root        = _ci._root;


    for(GSInteger i=0;i<_ci._etraps;i++) {
        v->_etraps.push_back(_etraps.top());
        _etraps.pop_back();
        GSExceptionTrap &et = v->_etraps.back();
        // restore absolute stack base and size
        et._stackbase += newbase;
        et._stacksize += newbase;
    }
    GSObject _this = _stack._vals[0];
    v->_stack[v->_stackbase] = GS_type(_this) == OT_WEAKREF ? _weakref(_this)->_obj : _this;

    for(GSInteger n = 1; n<size; n++) {
        v->_stack[v->_stackbase+n] = _stack._vals[n];
        _stack._vals[n].Null();
    }

    _state=eRunning;
    if (v->_debughook)
        v->CallDebugHook(_SC('c'));

    return true;
}

void GSArray::Extend(const GSArray *a){
    GSInteger xlen;
    if((xlen=a->Size()))
        for(GSInteger i=0;i<xlen;i++)
            Append(a->_values[i]);
}

const GSChar* GSFunctionProto::GetLocal(GSVM *vm,GSUnsignedInteger stackbase,GSUnsignedInteger nseq,GSUnsignedInteger nop)
{
    GSUnsignedInteger nvars=_nlocalvarinfos;
    const GSChar *res=NULL;
    if(nvars>=nseq){
        for(GSUnsignedInteger i=0;i<nvars;i++){
            if(_localvarinfos[i]._start_op<=nop && _localvarinfos[i]._end_op>=nop)
            {
                if(nseq==0){
                    vm->Push(vm->_stack[stackbase+_localvarinfos[i]._pos]);
                    res=_stringval(_localvarinfos[i]._name);
                    break;
                }
                nseq--;
            }
        }
    }
    return res;
}


GSInteger GSFunctionProto::GetLine(GSInstruction *curr)
{
    GSInteger op = (GSInteger)(curr-_instructions);
    GSInteger line=_lineinfos[0]._line;
    GSInteger low = 0;
    GSInteger high = _nlineinfos - 1;
    GSInteger mid = 0;
    while(low <= high)
    {
        mid = low + ((high - low) >> 1);
        GSInteger curop = _lineinfos[mid]._op;
        if(curop > op)
        {
            high = mid - 1;
        }
        else if(curop < op) {
            if(mid < (_nlineinfos - 1)
                && _lineinfos[mid + 1]._op >= op) {
                break;
            }
            low = mid + 1;
        }
        else { //equal
            break;
        }
    }

    while(mid > 0 && _lineinfos[mid]._op >= op) mid--;

    line = _lineinfos[mid]._line;

    return line;
}

GSClosure::~GSClosure()
{
    __ObjRelease(_root);
    __ObjRelease(_env);
    __ObjRelease(_base);
    REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
}

#define _CHECK_IO(exp)  { if(!exp)return false; }
bool SafeWrite(HGameScriptVM v,GSWRITEFUNC write,GSUserPointer up,GSUserPointer dest,GSInteger size)
{
    if(write(up,dest,size) != size) {
        v->Raise_Error(_SC("io error (write function failure)"));
        return false;
    }
    return true;
}

bool SafeRead(HGameScriptVM v,GSREADFUNC read,GSUserPointer up,GSUserPointer dest,GSInteger size)
{
    if(size && read(up,dest,size) != size) {
        v->Raise_Error(_SC("io error, read function failure, the origin stream could be corrupted/trucated"));
        return false;
    }
    return true;
}

bool WriteTag(HGameScriptVM v,GSWRITEFUNC write,GSUserPointer up,GSUnsignedInteger32 tag)
{
    return SafeWrite(v,write,up,&tag,sizeof(tag));
}

bool CheckTag(HGameScriptVM v,GSREADFUNC read,GSUserPointer up,GSUnsignedInteger32 tag)
{
    GSUnsignedInteger32 t;
    _CHECK_IO(SafeRead(v,read,up,&t,sizeof(t)));
    if(t != tag){
        v->Raise_Error(_SC("invalid or corrupted closure stream"));
        return false;
    }
    return true;
}

bool WriteObject(HGameScriptVM v,GSUserPointer up,GSWRITEFUNC write,GSObjectPtr &o)
{
    GSUnsignedInteger32 _type = (GSUnsignedInteger32)GS_type(o);
    _CHECK_IO(SafeWrite(v,write,up,&_type,sizeof(_type)));
    switch(GS_type(o)){
    case OT_STRING:
        _CHECK_IO(SafeWrite(v,write,up,&_string(o)->_len,sizeof(GSInteger)));
        _CHECK_IO(SafeWrite(v,write,up,_stringval(o),GS_rsl(_string(o)->_len)));
        break;
    case OT_BOOL:
    case OT_INTEGER:
        _CHECK_IO(SafeWrite(v,write,up,&_integer(o),sizeof(GSInteger)));break;
    case OT_FLOAT:
        _CHECK_IO(SafeWrite(v,write,up,&_float(o),sizeof(GSFloat)));break;
    case OT_NULL:
        break;
    default:
        v->Raise_Error(_SC("cannot serialize a %s"),GetTypeName(o));
        return false;
    }
    return true;
}

bool ReadObject(HGameScriptVM v,GSUserPointer up,GSREADFUNC read,GSObjectPtr &o)
{
    GSUnsignedInteger32 _type;
    _CHECK_IO(SafeRead(v,read,up,&_type,sizeof(_type)));
    GSObjectType t = (GSObjectType)_type;
    switch(t){
    case OT_STRING:{
        GSInteger len;
        _CHECK_IO(SafeRead(v,read,up,&len,sizeof(GSInteger)));
        _CHECK_IO(SafeRead(v,read,up,_ss(v)->GetScratchPad(GS_rsl(len)),GS_rsl(len)));
        o=GSString::Create(_ss(v),_ss(v)->GetScratchPad(-1),len);
                   }
        break;
    case OT_INTEGER:{
        GSInteger i;
        _CHECK_IO(SafeRead(v,read,up,&i,sizeof(GSInteger))); o = i; break;
                    }
    case OT_BOOL:{
        GSInteger i;
        _CHECK_IO(SafeRead(v,read,up,&i,sizeof(GSInteger))); o._type = OT_BOOL; o._unVal.nInteger = i; break;
                    }
    case OT_FLOAT:{
        GSFloat f;
        _CHECK_IO(SafeRead(v,read,up,&f,sizeof(GSFloat))); o = f; break;
                  }
    case OT_NULL:
        o.Null();
        break;
    default:
        v->Raise_Error(_SC("cannot serialize a %s"),IdType2Name(t));
        return false;
    }
    return true;
}

bool GSClosure::Save(GSVM *v,GSUserPointer up,GSWRITEFUNC write)
{
    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_HEAD));
    _CHECK_IO(WriteTag(v,write,up,sizeof(GSChar)));
    _CHECK_IO(WriteTag(v,write,up,sizeof(GSInteger)));
    _CHECK_IO(WriteTag(v,write,up,sizeof(GSFloat)));
    _CHECK_IO(_function->Save(v,up,write));
    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_TAIL));
    return true;
}

bool GSClosure::Load(GSVM *v,GSUserPointer up,GSREADFUNC read,GSObjectPtr &ret)
{
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_HEAD));
    _CHECK_IO(CheckTag(v,read,up,sizeof(GSChar)));
    _CHECK_IO(CheckTag(v,read,up,sizeof(GSInteger)));
    _CHECK_IO(CheckTag(v,read,up,sizeof(GSFloat)));
    GSObjectPtr func;
    _CHECK_IO(GSFunctionProto::Load(v,up,read,func));
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_TAIL));
    ret = GSClosure::Create(_ss(v),_funcproto(func),_table(v->_roottable)->GetWeakRef(OT_TABLE));
    //FIXME: load an root for this closure
    return true;
}

GSFunctionProto::GSFunctionProto(GSSharedState *ss)
{
    _stacksize=0;
    _bgenerator=false;
    INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);
}

GSFunctionProto::~GSFunctionProto()
{
    REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
}

bool GSFunctionProto::Save(GSVM *v,GSUserPointer up,GSWRITEFUNC write)
{
    GSInteger i,nliterals = _nliterals,nparameters = _nparameters;
    GSInteger noutervalues = _noutervalues,nlocalvarinfos = _nlocalvarinfos;
    GSInteger nlineinfos=_nlineinfos,ninstructions = _ninstructions,nfunctions=_nfunctions;
    GSInteger ndefaultparams = _ndefaultparams;
    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(WriteObject(v,up,write,_sourcename));
    _CHECK_IO(WriteObject(v,up,write,_name));
    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,&nliterals,sizeof(nliterals)));
    _CHECK_IO(SafeWrite(v,write,up,&nparameters,sizeof(nparameters)));
    _CHECK_IO(SafeWrite(v,write,up,&noutervalues,sizeof(noutervalues)));
    _CHECK_IO(SafeWrite(v,write,up,&nlocalvarinfos,sizeof(nlocalvarinfos)));
    _CHECK_IO(SafeWrite(v,write,up,&nlineinfos,sizeof(nlineinfos)));
    _CHECK_IO(SafeWrite(v,write,up,&ndefaultparams,sizeof(ndefaultparams)));
    _CHECK_IO(SafeWrite(v,write,up,&ninstructions,sizeof(ninstructions)));
    _CHECK_IO(SafeWrite(v,write,up,&nfunctions,sizeof(nfunctions)));
    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    for(i=0;i<nliterals;i++){
        _CHECK_IO(WriteObject(v,up,write,_literals[i]));
    }

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    for(i=0;i<nparameters;i++){
        _CHECK_IO(WriteObject(v,up,write,_parameters[i]));
    }

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    for(i=0;i<noutervalues;i++){
        _CHECK_IO(SafeWrite(v,write,up,&_outervalues[i]._type,sizeof(GSUnsignedInteger)));
        _CHECK_IO(WriteObject(v,up,write,_outervalues[i]._src));
        _CHECK_IO(WriteObject(v,up,write,_outervalues[i]._name));
    }

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    for(i=0;i<nlocalvarinfos;i++){
        GSLocalVarInfo &lvi=_localvarinfos[i];
        _CHECK_IO(WriteObject(v,up,write,lvi._name));
        _CHECK_IO(SafeWrite(v,write,up,&lvi._pos,sizeof(GSUnsignedInteger)));
        _CHECK_IO(SafeWrite(v,write,up,&lvi._start_op,sizeof(GSUnsignedInteger)));
        _CHECK_IO(SafeWrite(v,write,up,&lvi._end_op,sizeof(GSUnsignedInteger)));
    }

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,_lineinfos,sizeof(GSLineInfo)*nlineinfos));

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,_defaultparams,sizeof(GSInteger)*ndefaultparams));

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,_instructions,sizeof(GSInstruction)*ninstructions));

    _CHECK_IO(WriteTag(v,write,up,GS_CLOSURESTREAM_PART));
    for(i=0;i<nfunctions;i++){
        _CHECK_IO(_funcproto(_functions[i])->Save(v,up,write));
    }
    _CHECK_IO(SafeWrite(v,write,up,&_stacksize,sizeof(_stacksize)));
    _CHECK_IO(SafeWrite(v,write,up,&_bgenerator,sizeof(_bgenerator)));
    _CHECK_IO(SafeWrite(v,write,up,&_varparams,sizeof(_varparams)));
    return true;
}

bool GSFunctionProto::Load(GSVM *v,GSUserPointer up,GSREADFUNC read,GSObjectPtr &ret)
{
    GSInteger i, nliterals,nparameters;
    GSInteger noutervalues ,nlocalvarinfos ;
    GSInteger nlineinfos,ninstructions ,nfunctions,ndefaultparams ;
    GSObjectPtr sourcename, name;
    GSObjectPtr o;
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(ReadObject(v, up, read, sourcename));
    _CHECK_IO(ReadObject(v, up, read, name));

    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, &nliterals, sizeof(nliterals)));
    _CHECK_IO(SafeRead(v,read,up, &nparameters, sizeof(nparameters)));
    _CHECK_IO(SafeRead(v,read,up, &noutervalues, sizeof(noutervalues)));
    _CHECK_IO(SafeRead(v,read,up, &nlocalvarinfos, sizeof(nlocalvarinfos)));
    _CHECK_IO(SafeRead(v,read,up, &nlineinfos, sizeof(nlineinfos)));
    _CHECK_IO(SafeRead(v,read,up, &ndefaultparams, sizeof(ndefaultparams)));
    _CHECK_IO(SafeRead(v,read,up, &ninstructions, sizeof(ninstructions)));
    _CHECK_IO(SafeRead(v,read,up, &nfunctions, sizeof(nfunctions)));


    GSFunctionProto *f = GSFunctionProto::Create(_opt_ss(v),ninstructions,nliterals,nparameters,
            nfunctions,noutervalues,nlineinfos,nlocalvarinfos,ndefaultparams);
    GSObjectPtr proto = f; //gets a ref in case of failure
    f->_sourcename = sourcename;
    f->_name = name;

    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));

    for(i = 0;i < nliterals; i++){
        _CHECK_IO(ReadObject(v, up, read, o));
        f->_literals[i] = o;
    }
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));

    for(i = 0; i < nparameters; i++){
        _CHECK_IO(ReadObject(v, up, read, o));
        f->_parameters[i] = o;
    }
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));

    for(i = 0; i < noutervalues; i++){
        GSUnsignedInteger type;
        GSObjectPtr name;
        _CHECK_IO(SafeRead(v,read,up, &type, sizeof(GSUnsignedInteger)));
        _CHECK_IO(ReadObject(v, up, read, o));
        _CHECK_IO(ReadObject(v, up, read, name));
        f->_outervalues[i] = GSOuterVar(name,o, (GSOuterType)type);
    }
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));

    for(i = 0; i < nlocalvarinfos; i++){
        GSLocalVarInfo lvi;
        _CHECK_IO(ReadObject(v, up, read, lvi._name));
        _CHECK_IO(SafeRead(v,read,up, &lvi._pos, sizeof(GSUnsignedInteger)));
        _CHECK_IO(SafeRead(v,read,up, &lvi._start_op, sizeof(GSUnsignedInteger)));
        _CHECK_IO(SafeRead(v,read,up, &lvi._end_op, sizeof(GSUnsignedInteger)));
        f->_localvarinfos[i] = lvi;
    }
    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, f->_lineinfos, sizeof(GSLineInfo)*nlineinfos));

    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, f->_defaultparams, sizeof(GSInteger)*ndefaultparams));

    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, f->_instructions, sizeof(GSInstruction)*ninstructions));

    _CHECK_IO(CheckTag(v,read,up,GS_CLOSURESTREAM_PART));
    for(i = 0; i < nfunctions; i++){
        _CHECK_IO(_funcproto(o)->Load(v, up, read, o));
        f->_functions[i] = o;
    }
    _CHECK_IO(SafeRead(v,read,up, &f->_stacksize, sizeof(f->_stacksize)));
    _CHECK_IO(SafeRead(v,read,up, &f->_bgenerator, sizeof(f->_bgenerator)));
    _CHECK_IO(SafeRead(v,read,up, &f->_varparams, sizeof(f->_varparams)));

    ret = f;
    return true;
}

#ifndef NO_GARBAGE_COLLECTOR

#define START_MARK()    if(!(_uiRef&MARK_FLAG)){ \
        _uiRef|=MARK_FLAG;

#define END_MARK() RemoveFromChain(&_sharedstate->_gc_chain, this); \
        AddToChain(chain, this); }

void GSVM::Mark(GSCollectable **chain)
{
    START_MARK()
        GSSharedState::MarkObject(_lasterror,chain);
        GSSharedState::MarkObject(_errorhandler,chain);
        GSSharedState::MarkObject(_debughook_closure,chain);
        GSSharedState::MarkObject(_roottable, chain);
        GSSharedState::MarkObject(temp_reg, chain);
        for(GSUnsignedInteger i = 0; i < _stack.size(); i++) GSSharedState::MarkObject(_stack[i], chain);
        for(GSInteger k = 0; k < _callsstacksize; k++) GSSharedState::MarkObject(_callsstack[k]._closure, chain);
    END_MARK()
}

void GSArray::Mark(GSCollectable **chain)
{
    START_MARK()
        GSInteger len = _values.size();
        for(GSInteger i = 0;i < len; i++) GSSharedState::MarkObject(_values[i], chain);
    END_MARK()
}
void GSTable::Mark(GSCollectable **chain)
{
    START_MARK()
        if(_delegate) _delegate->Mark(chain);
        GSInteger len = _numofnodes;
        for(GSInteger i = 0; i < len; i++){
            GSSharedState::MarkObject(_nodes[i].key, chain);
            GSSharedState::MarkObject(_nodes[i].val, chain);
        }
    END_MARK()
}

void GSClass::Mark(GSCollectable **chain)
{
    START_MARK()
        _members->Mark(chain);
        if(_base) _base->Mark(chain);
        GSSharedState::MarkObject(_attributes, chain);
        for(GSUnsignedInteger i =0; i< _defaultvalues.size(); i++) {
            GSSharedState::MarkObject(_defaultvalues[i].val, chain);
            GSSharedState::MarkObject(_defaultvalues[i].attrs, chain);
        }
        for(GSUnsignedInteger j =0; j< _methods.size(); j++) {
            GSSharedState::MarkObject(_methods[j].val, chain);
            GSSharedState::MarkObject(_methods[j].attrs, chain);
        }
        for(GSUnsignedInteger k =0; k< MT_LAST; k++) {
            GSSharedState::MarkObject(_metamethods[k], chain);
        }
    END_MARK()
}

void GSInstance::Mark(GSCollectable **chain)
{
    START_MARK()
        _class->Mark(chain);
        GSUnsignedInteger nvalues = _class->_defaultvalues.size();
        for(GSUnsignedInteger i =0; i< nvalues; i++) {
            GSSharedState::MarkObject(_values[i], chain);
        }
    END_MARK()
}

void GSGenerator::Mark(GSCollectable **chain)
{
    START_MARK()
        for(GSUnsignedInteger i = 0; i < _stack.size(); i++) GSSharedState::MarkObject(_stack[i], chain);
        GSSharedState::MarkObject(_closure, chain);
    END_MARK()
}

void GSFunctionProto::Mark(GSCollectable **chain)
{
    START_MARK()
        for(GSInteger i = 0; i < _nliterals; i++) GSSharedState::MarkObject(_literals[i], chain);
        for(GSInteger k = 0; k < _nfunctions; k++) GSSharedState::MarkObject(_functions[k], chain);
    END_MARK()
}

void GSClosure::Mark(GSCollectable **chain)
{
    START_MARK()
        if(_base) _base->Mark(chain);
        GSFunctionProto *fp = _function;
        fp->Mark(chain);
        for(GSInteger i = 0; i < fp->_noutervalues; i++) GSSharedState::MarkObject(_outervalues[i], chain);
        for(GSInteger k = 0; k < fp->_ndefaultparams; k++) GSSharedState::MarkObject(_defaultparams[k], chain);
    END_MARK()
}

void GSNativeClosure::Mark(GSCollectable **chain)
{
    START_MARK()
        for(GSUnsignedInteger i = 0; i < _noutervalues; i++) GSSharedState::MarkObject(_outervalues[i], chain);
    END_MARK()
}

void GSOuter::Mark(GSCollectable **chain)
{
    START_MARK()
    /* If the valptr points to a closed value, that value is alive */
    if(_valptr == &_value) {
      GSSharedState::MarkObject(_value, chain);
    }
    END_MARK()
}

void GSUserData::Mark(GSCollectable **chain){
    START_MARK()
        if(_delegate) _delegate->Mark(chain);
    END_MARK()
}

void GSCollectable::UnMark() { _uiRef&=~MARK_FLAG; }

#endif


