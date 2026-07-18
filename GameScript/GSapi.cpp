/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#include "GSvm.h"
#include "GSstring.h"
#include "GStable.h"
#include "GSarray.h"
#include "GSfuncproto.h"
#include "GSclosure.h"
#include "GSuserdata.h"
#include "GScompiler.h"
#include "GSfuncstate.h"
#include "GSclass.h"

static bool GS_aux_gettypedarg(HGameScriptVM v,GSInteger idx,GSObjectType type,GSObjectPtr **o)
{
    *o = &stack_get(v,idx);
    if(GS_type(**o) != type){
        GSObjectPtr oval = v->PrintObjVal(**o);
        v->Raise_Error(_SC("wrong argument type, expected '%s' got '%.50s'"),IdType2Name(type),_stringval(oval));
        return false;
    }
    return true;
}

#define _GETSAFE_OBJ(v,idx,type,o) { if(!GS_aux_gettypedarg(v,idx,type,&o)) return GS_ERROR; }

#define GS_aux_paramscheck(v,count) \
{ \
    if(GS_gettop(v) < count){ v->Raise_Error(_SC("not enough params in the stack")); return GS_ERROR; }\
}


GSInteger GS_aux_invalidtype(HGameScriptVM v,GSObjectType type)
{
    GSUnsignedInteger buf_size = 100 *sizeof(GSChar);
    scsprintf(_ss(v)->GetScratchPad(buf_size), buf_size, _SC("unexpected type %s"), IdType2Name(type));
    return GS_throwerror(v, _ss(v)->GetScratchPad(-1));
}

HGameScriptVM GS_open(GSInteger initialstacksize)
{
    GSSharedState *ss;
    GSVM *v;
    GS_new(ss, GSSharedState);
    ss->Init();
    v = (GSVM *)GS_MALLOC(sizeof(GSVM));
    new (v) GSVM(ss);
    ss->_root_vm = v;
    if(v->Init(NULL, initialstacksize)) {
        return v;
    } else {
        GS_delete(v, GSVM);
        return NULL;
    }
    return v;
}

HGameScriptVM GS_newthread(HGameScriptVM friendvm, GSInteger initialstacksize)
{
    GSSharedState *ss;
    GSVM *v;
    ss=_ss(friendvm);

    v= (GSVM *)GS_MALLOC(sizeof(GSVM));
    new (v) GSVM(ss);

    if(v->Init(friendvm, initialstacksize)) {
        friendvm->Push(v);
        return v;
    } else {
        GS_delete(v, GSVM);
        return NULL;
    }
}

GSInteger GS_getvmstate(HGameScriptVM v)
{
    if(v->_suspended)
        return GS_VMSTATE_SUSPENDED;
    else {
        if(v->_callsstacksize != 0) return GS_VMSTATE_RUNNING;
        else return GS_VMSTATE_IDLE;
    }
}

void GS_seterrorhandler(HGameScriptVM v)
{
    GSObject o = stack_get(v, -1);
    if(GS_isclosure(o) || GS_isnativeclosure(o) || GS_isnull(o)) {
        v->_errorhandler = o;
        v->Pop();
    }
}

void GS_setnativedebughook(HGameScriptVM v,GSDEBUGHOOK hook)
{
    v->_debughook_native = hook;
    v->_debughook_closure.Null();
    v->_debughook = hook?true:false;
}

void GS_setdebughook(HGameScriptVM v)
{
    GSObject o = stack_get(v,-1);
    if(GS_isclosure(o) || GS_isnativeclosure(o) || GS_isnull(o)) {
        v->_debughook_closure = o;
        v->_debughook_native = NULL;
        v->_debughook = !GS_isnull(o);
        v->Pop();
    }
}

void GS_close(HGameScriptVM v)
{
    GSSharedState *ss = _ss(v);
    _thread(ss->_root_vm)->Finalize();
    GS_delete(ss, GSSharedState);
}

GSInteger GS_getversion()
{
    return GameScript_VERSION_NUMBER;
}

GSRESULT GS_compile(HGameScriptVM v,GSLEXREADFUNC read,GSUserPointer p,const GSChar *sourcename,GSBool raiseerror)
{
    GSObjectPtr o;
#ifndef NO_COMPILER
    if(Compile(v, read, p, sourcename, o, raiseerror?true:false, _ss(v)->_debuginfo)) {
        v->Push(GSClosure::Create(_ss(v), _funcproto(o), _table(v->_roottable)->GetWeakRef(OT_TABLE)));
        return GS_OK;
    }
    return GS_ERROR;
#else
    return GS_throwerror(v,_SC("this is a no compiler build"));
#endif
}

void GS_enabledebuginfo(HGameScriptVM v, GSBool enable)
{
    _ss(v)->_debuginfo = enable?true:false;
}

void GS_notifyallexceptions(HGameScriptVM v, GSBool enable)
{
    _ss(v)->_notifyallexceptions = enable?true:false;
}

void GS_addref(HGameScriptVM v,HGSOBJECT *po)
{
    if(!ISREFCOUNTED(GS_type(*po))) return;
#ifdef NO_GARBAGE_COLLECTOR
    __AddRef(po->_type,po->_unVal);
#else
    _ss(v)->_refs_table.AddRef(*po);
#endif
}

GSUnsignedInteger GS_getrefcount(HGameScriptVM v,HGSOBJECT *po)
{
    if(!ISREFCOUNTED(GS_type(*po))) return 0;
#ifdef NO_GARBAGE_COLLECTOR
   return po->_unVal.pRefCounted->_uiRef;
#else
   return _ss(v)->_refs_table.GetRefCount(*po);
#endif
}

GSBool GS_release(HGameScriptVM v,HGSOBJECT *po)
{
    if(!ISREFCOUNTED(GS_type(*po))) return GSTrue;
#ifdef NO_GARBAGE_COLLECTOR
    bool ret = (po->_unVal.pRefCounted->_uiRef <= 1) ? GSTrue : GSFalse;
    __Release(po->_type,po->_unVal);
    return ret; //the ret val doesn't work(and cannot be fixed)
#else
    return _ss(v)->_refs_table.Release(*po);
#endif
}

GSUnsignedInteger GS_getvmrefcount(HGameScriptVM GS_UNUSED_ARG(v), const HGSOBJECT *po)
{
    if (!ISREFCOUNTED(GS_type(*po))) return 0;
    return po->_unVal.pRefCounted->_uiRef;
}

const GSChar *GS_objtostring(const HGSOBJECT *o)
{
    if(GS_type(*o) == OT_STRING) {
        return _stringval(*o);
    }
    return NULL;
}

GSInteger GS_objtointeger(const HGSOBJECT *o)
{
    if(GS_isnumeric(*o)) {
        return tointeger(*o);
    }
    return 0;
}

GSFloat GS_objtofloat(const HGSOBJECT *o)
{
    if(GS_isnumeric(*o)) {
        return tofloat(*o);
    }
    return 0;
}

GSBool GS_objtobool(const HGSOBJECT *o)
{
    if(GS_isbool(*o)) {
        return _integer(*o);
    }
    return GSFalse;
}

GSUserPointer GS_objtouserpointer(const HGSOBJECT *o)
{
    if(GS_isuserpointer(*o)) {
        return _userpointer(*o);
    }
    return 0;
}

void GS_pushnull(HGameScriptVM v)
{
    v->PushNull();
}

void GS_pushstring(HGameScriptVM v,const GSChar *s,GSInteger len)
{
    if(s)
        v->Push(GSObjectPtr(GSString::Create(_ss(v), s, len)));
    else v->PushNull();
}

void GS_pushinteger(HGameScriptVM v,GSInteger n)
{
    v->Push(n);
}

void GS_pushbool(HGameScriptVM v,GSBool b)
{
    v->Push(b?true:false);
}

void GS_pushfloat(HGameScriptVM v,GSFloat n)
{
    v->Push(n);
}

void GS_pushuserpointer(HGameScriptVM v,GSUserPointer p)
{
    v->Push(p);
}

void GS_pushthread(HGameScriptVM v, HGameScriptVM thread)
{
    v->Push(thread);
}

GSUserPointer GS_newuserdata(HGameScriptVM v,GSUnsignedInteger size)
{
    GSUserData *ud = GSUserData::Create(_ss(v), size + GS_ALIGNMENT);
    v->Push(ud);
    return (GSUserPointer)GS_aligning(ud + 1);
}

void GS_newtable(HGameScriptVM v)
{
    v->Push(GSTable::Create(_ss(v), 0));
}

void GS_newtableex(HGameScriptVM v,GSInteger initialcapacity)
{
    v->Push(GSTable::Create(_ss(v), initialcapacity));
}

void GS_newarray(HGameScriptVM v,GSInteger size)
{
    v->Push(GSArray::Create(_ss(v), size));
}

GSRESULT GS_newclass(HGameScriptVM v,GSBool hasbase)
{
    GSClass *baseclass = NULL;
    if(hasbase) {
        GSObjectPtr &base = stack_get(v,-1);
        if(GS_type(base) != OT_CLASS)
            return GS_throwerror(v,_SC("invalid base type"));
        baseclass = _class(base);
    }
    GSClass *newclass = GSClass::Create(_ss(v), baseclass);
    if(baseclass) v->Pop();
    v->Push(newclass);
    return GS_OK;
}

GSBool GS_instanceof(HGameScriptVM v)
{
    GSObjectPtr &inst = stack_get(v,-1);
    GSObjectPtr &cl = stack_get(v,-2);
    if(GS_type(inst) != OT_INSTANCE || GS_type(cl) != OT_CLASS)
        return GS_throwerror(v,_SC("invalid param type"));
    return _instance(inst)->InstanceOf(_class(cl))?GSTrue:GSFalse;
}

GSRESULT GS_arrayappend(HGameScriptVM v,GSInteger idx)
{
    GS_aux_paramscheck(v,2);
    GSObjectPtr *arr;
    _GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
    _array(*arr)->Append(v->GetUp(-1));
    v->Pop();
    return GS_OK;
}

GSRESULT GS_arraypop(HGameScriptVM v,GSInteger idx,GSBool pushval)
{
    GS_aux_paramscheck(v, 1);
    GSObjectPtr *arr;
    _GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
    if(_array(*arr)->Size() > 0) {
        if(pushval != 0){ v->Push(_array(*arr)->Top()); }
        _array(*arr)->Pop();
        return GS_OK;
    }
    return GS_throwerror(v, _SC("empty array"));
}

GSRESULT GS_arrayresize(HGameScriptVM v,GSInteger idx,GSInteger newsize)
{
    GS_aux_paramscheck(v,1);
    GSObjectPtr *arr;
    _GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
    if(newsize >= 0) {
        _array(*arr)->Resize(newsize);
        return GS_OK;
    }
    return GS_throwerror(v,_SC("negative size"));
}


GSRESULT GS_arrayreverse(HGameScriptVM v,GSInteger idx)
{
    GS_aux_paramscheck(v, 1);
    GSObjectPtr *o;
    _GETSAFE_OBJ(v, idx, OT_ARRAY,o);
    GSArray *arr = _array(*o);
    if(arr->Size() > 0) {
        GSObjectPtr t;
        GSInteger size = arr->Size();
        GSInteger n = size >> 1; size -= 1;
        for(GSInteger i = 0; i < n; i++) {
            t = arr->_values[i];
            arr->_values[i] = arr->_values[size-i];
            arr->_values[size-i] = t;
        }
        return GS_OK;
    }
    return GS_OK;
}

GSRESULT GS_arrayremove(HGameScriptVM v,GSInteger idx,GSInteger itemidx)
{
    GS_aux_paramscheck(v, 1);
    GSObjectPtr *arr;
    _GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
    return _array(*arr)->Remove(itemidx) ? GS_OK : GS_throwerror(v,_SC("index out of range"));
}

GSRESULT GS_arrayinsert(HGameScriptVM v,GSInteger idx,GSInteger destpos)
{
    GS_aux_paramscheck(v, 1);
    GSObjectPtr *arr;
    _GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
    GSRESULT ret = _array(*arr)->Insert(destpos, v->GetUp(-1)) ? GS_OK : GS_throwerror(v,_SC("index out of range"));
    v->Pop();
    return ret;
}

void GS_newclosure(HGameScriptVM v,GSFUNCTION func,GSUnsignedInteger nfreevars)
{
    GSNativeClosure *nc = GSNativeClosure::Create(_ss(v), func,nfreevars);
    nc->_nparamscheck = 0;
    for(GSUnsignedInteger i = 0; i < nfreevars; i++) {
        nc->_outervalues[i] = v->Top();
        v->Pop();
    }
    v->Push(GSObjectPtr(nc));
}

GSRESULT GS_getclosureinfo(HGameScriptVM v,GSInteger idx,GSInteger *nparams,GSInteger *nfreevars)
{
    GSObject o = stack_get(v, idx);
    if(GS_type(o) == OT_CLOSURE) {
        GSClosure *c = _closure(o);
        GSFunctionProto *proto = c->_function;
        *nparams = proto->_nparameters;
        *nfreevars = proto->_noutervalues;
        return GS_OK;
    }
    else if(GS_type(o) == OT_NATIVECLOSURE)
    {
        GSNativeClosure *c = _nativeclosure(o);
        *nparams = c->_nparamscheck;
        *nfreevars = (GSInteger)c->_noutervalues;
        return GS_OK;
    }
    return GS_throwerror(v,_SC("the object is not a closure"));
}

GSRESULT GS_setnativeclosurename(HGameScriptVM v,GSInteger idx,const GSChar *name)
{
    GSObject o = stack_get(v, idx);
    if(GS_isnativeclosure(o)) {
        GSNativeClosure *nc = _nativeclosure(o);
        nc->_name = GSString::Create(_ss(v),name);
        return GS_OK;
    }
    return GS_throwerror(v,_SC("the object is not a nativeclosure"));
}

GSRESULT GS_setparamscheck(HGameScriptVM v,GSInteger nparamscheck,const GSChar *typemask)
{
    GSObject o = stack_get(v, -1);
    if(!GS_isnativeclosure(o))
        return GS_throwerror(v, _SC("native closure expected"));
    GSNativeClosure *nc = _nativeclosure(o);
    nc->_nparamscheck = nparamscheck;
    if(typemask) {
        GSIntVec res;
        if(!CompileTypemask(res, typemask))
            return GS_throwerror(v, _SC("invalid typemask"));
        nc->_typecheck.copy(res);
    }
    else {
        nc->_typecheck.resize(0);
    }
    if(nparamscheck == GS_MATCHTYPEMASKSTRING) {
        nc->_nparamscheck = nc->_typecheck.size();
    }
    return GS_OK;
}

GSRESULT GS_bindenv(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &o = stack_get(v,idx);
    if(!GS_isnativeclosure(o) &&
        !GS_isclosure(o))
        return GS_throwerror(v,_SC("the target is not a closure"));
    GSObjectPtr &env = stack_get(v,-1);
    if(!GS_istable(env) &&
        !GS_isarray(env) &&
        !GS_isclass(env) &&
        !GS_isinstance(env))
        return GS_throwerror(v,_SC("invalid environment"));
    GSWeakRef *w = _refcounted(env)->GetWeakRef(GS_type(env));
    GSObjectPtr ret;
    if(GS_isclosure(o)) {
        GSClosure *c = _closure(o)->Clone();
        __ObjRelease(c->_env);
        c->_env = w;
        __ObjAddRef(c->_env);
        if(_closure(o)->_base) {
            c->_base = _closure(o)->_base;
            __ObjAddRef(c->_base);
        }
        ret = c;
    }
    else { //then must be a native closure
        GSNativeClosure *c = _nativeclosure(o)->Clone();
        __ObjRelease(c->_env);
        c->_env = w;
        __ObjAddRef(c->_env);
        ret = c;
    }
    v->Pop();
    v->Push(ret);
    return GS_OK;
}

GSRESULT GS_getclosurename(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &o = stack_get(v,idx);
    if(!GS_isnativeclosure(o) &&
        !GS_isclosure(o))
        return GS_throwerror(v,_SC("the target is not a closure"));
    if(GS_isnativeclosure(o))
    {
        v->Push(_nativeclosure(o)->_name);
    }
    else { //closure
        v->Push(_closure(o)->_function->_name);
    }
    return GS_OK;
}

GSRESULT GS_setclosureroot(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &c = stack_get(v,idx);
    GSObject o = stack_get(v, -1);
    if(!GS_isclosure(c)) return GS_throwerror(v, _SC("closure expected"));
    if(GS_istable(o)) {
        _closure(c)->SetRoot(_table(o)->GetWeakRef(OT_TABLE));
        v->Pop();
        return GS_OK;
    }
    return GS_throwerror(v, _SC("invalid type"));
}

GSRESULT GS_getclosureroot(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &c = stack_get(v,idx);
    if(!GS_isclosure(c)) return GS_throwerror(v, _SC("closure expected"));
    v->Push(_closure(c)->_root->_obj);
    return GS_OK;
}

GSRESULT GS_clear(HGameScriptVM v,GSInteger idx)
{
    GSObject &o=stack_get(v,idx);
    switch(GS_type(o)) {
        case OT_TABLE: _table(o)->Clear();  break;
        case OT_ARRAY: _array(o)->Resize(0); break;
        default:
            return GS_throwerror(v, _SC("clear only works on table and array"));
        break;

    }
    return GS_OK;
}

void GS_pushroottable(HGameScriptVM v)
{
    v->Push(v->_roottable);
}

void GS_pushregistrytable(HGameScriptVM v)
{
    v->Push(_ss(v)->_registry);
}

void GS_pushconsttable(HGameScriptVM v)
{
    v->Push(_ss(v)->_consts);
}

GSRESULT GS_setroottable(HGameScriptVM v)
{
    GSObject o = stack_get(v, -1);
    if(GS_istable(o) || GS_isnull(o)) {
        v->_roottable = o;
        v->Pop();
        return GS_OK;
    }
    return GS_throwerror(v, _SC("invalid type"));
}

GSRESULT GS_setconsttable(HGameScriptVM v)
{
    GSObject o = stack_get(v, -1);
    if(GS_istable(o)) {
        _ss(v)->_consts = o;
        v->Pop();
        return GS_OK;
    }
    return GS_throwerror(v, _SC("invalid type, expected table"));
}

void GS_setforeignptr(HGameScriptVM v,GSUserPointer p)
{
    v->_foreignptr = p;
}

GSUserPointer GS_getforeignptr(HGameScriptVM v)
{
    return v->_foreignptr;
}

void GS_setsharedforeignptr(HGameScriptVM v,GSUserPointer p)
{
    _ss(v)->_foreignptr = p;
}

GSUserPointer GS_getsharedforeignptr(HGameScriptVM v)
{
    return _ss(v)->_foreignptr;
}

void GS_setvmreleasehook(HGameScriptVM v,GSRELEASEHOOK hook)
{
    v->_releasehook = hook;
}

GSRELEASEHOOK GS_getvmreleasehook(HGameScriptVM v)
{
    return v->_releasehook;
}

void GS_setsharedreleasehook(HGameScriptVM v,GSRELEASEHOOK hook)
{
    _ss(v)->_releasehook = hook;
}

GSRELEASEHOOK GS_getsharedreleasehook(HGameScriptVM v)
{
    return _ss(v)->_releasehook;
}

void GS_push(HGameScriptVM v,GSInteger idx)
{
    v->Push(stack_get(v, idx));
}

GSObjectType GS_gettype(HGameScriptVM v,GSInteger idx)
{
    return GS_type(stack_get(v, idx));
}

GSRESULT GS_typeof(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &o = stack_get(v, idx);
    GSObjectPtr res;
    if(!v->TypeOf(o,res)) {
        return GS_ERROR;
    }
    v->Push(res);
    return GS_OK;
}

GSRESULT GS_tostring(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &o = stack_get(v, idx);
    GSObjectPtr res;
    if(!v->ToString(o,res)) {
        return GS_ERROR;
    }
    v->Push(res);
    return GS_OK;
}

void GS_tobool(HGameScriptVM v, GSInteger idx, GSBool *b)
{
    GSObjectPtr &o = stack_get(v, idx);
    *b = GSVM::IsFalse(o)?GSFalse:GSTrue;
}

GSRESULT GS_getinteger(HGameScriptVM v,GSInteger idx,GSInteger *i)
{
    GSObjectPtr &o = stack_get(v, idx);
    if(GS_isnumeric(o)) {
        *i = tointeger(o);
        return GS_OK;
    }
    if(GS_isbool(o)) {
        *i = GSVM::IsFalse(o)?GSFalse:GSTrue;
        return GS_OK;
    }
    return GS_ERROR;
}

GSRESULT GS_getfloat(HGameScriptVM v,GSInteger idx,GSFloat *f)
{
    GSObjectPtr &o = stack_get(v, idx);
    if(GS_isnumeric(o)) {
        *f = tofloat(o);
        return GS_OK;
    }
    return GS_ERROR;
}

GSRESULT GS_getbool(HGameScriptVM v,GSInteger idx,GSBool *b)
{
    GSObjectPtr &o = stack_get(v, idx);
    if(GS_isbool(o)) {
        *b = _integer(o);
        return GS_OK;
    }
    return GS_ERROR;
}

GSRESULT GS_getstringandsize(HGameScriptVM v,GSInteger idx,const GSChar **c,GSInteger *size)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_STRING,o);
    *c = _stringval(*o);
    *size = _string(*o)->_len;
    return GS_OK;
}

GSRESULT GS_getstring(HGameScriptVM v,GSInteger idx,const GSChar **c)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_STRING,o);
    *c = _stringval(*o);
    return GS_OK;
}

GSRESULT GS_getthread(HGameScriptVM v,GSInteger idx,HGameScriptVM *thread)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_THREAD,o);
    *thread = _thread(*o);
    return GS_OK;
}

GSRESULT GS_clone(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &o = stack_get(v,idx);
    v->PushNull();
    if(!v->Clone(o, stack_get(v, -1))){
        v->Pop();
        return GS_ERROR;
    }
    return GS_OK;
}

GSInteger GS_getsize(HGameScriptVM v, GSInteger idx)
{
    GSObjectPtr &o = stack_get(v, idx);
    GSObjectType type = GS_type(o);
    switch(type) {
    case OT_STRING:     return _string(o)->_len;
    case OT_TABLE:      return _table(o)->CountUsed();
    case OT_ARRAY:      return _array(o)->Size();
    case OT_USERDATA:   return _userdata(o)->_size;
    case OT_INSTANCE:   return _instance(o)->_class->_udsize;
    case OT_CLASS:      return _class(o)->_udsize;
    default:
        return GS_aux_invalidtype(v, type);
    }
}

GSHash GS_gethash(HGameScriptVM v, GSInteger idx)
{
    GSObjectPtr &o = stack_get(v, idx);
    return HashObj(o);
}

GSRESULT GS_getuserdata(HGameScriptVM v,GSInteger idx,GSUserPointer *p,GSUserPointer *typetag)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_USERDATA,o);
    (*p) = _userdataval(*o);
    if(typetag) *typetag = _userdata(*o)->_typetag;
    return GS_OK;
}

GSRESULT GS_settypetag(HGameScriptVM v,GSInteger idx,GSUserPointer typetag)
{
    GSObjectPtr &o = stack_get(v,idx);
    switch(GS_type(o)) {
        case OT_USERDATA:   _userdata(o)->_typetag = typetag;   break;
        case OT_CLASS:      _class(o)->_typetag = typetag;      break;
        default:            return GS_throwerror(v,_SC("invalid object type"));
    }
    return GS_OK;
}

GSRESULT GS_getobjtypetag(const HGSOBJECT *o,GSUserPointer * typetag)
{
  switch(GS_type(*o)) {
    case OT_INSTANCE: *typetag = _instance(*o)->_class->_typetag; break;
    case OT_USERDATA: *typetag = _userdata(*o)->_typetag; break;
    case OT_CLASS:    *typetag = _class(*o)->_typetag; break;
    default: return GS_ERROR;
  }
  return GS_OK;
}

GSRESULT GS_gettypetag(HGameScriptVM v,GSInteger idx,GSUserPointer *typetag)
{
    GSObjectPtr &o = stack_get(v,idx);
    if (GS_FAILED(GS_getobjtypetag(&o, typetag)))
        return GS_ERROR;// this is not an error it should be a bool but would break backward compatibility
    return GS_OK;
}

GSRESULT GS_getuserpointer(HGameScriptVM v, GSInteger idx, GSUserPointer *p)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_USERPOINTER,o);
    (*p) = _userpointer(*o);
    return GS_OK;
}

GSRESULT GS_setinstanceup(HGameScriptVM v, GSInteger idx, GSUserPointer p)
{
    GSObjectPtr &o = stack_get(v,idx);
    if(GS_type(o) != OT_INSTANCE) return GS_throwerror(v,_SC("the object is not a class instance"));
    _instance(o)->_userpointer = p;
    return GS_OK;
}

GSRESULT GS_setclassudsize(HGameScriptVM v, GSInteger idx, GSInteger udsize)
{
    GSObjectPtr &o = stack_get(v,idx);
    if(GS_type(o) != OT_CLASS) return GS_throwerror(v,_SC("the object is not a class"));
    if(_class(o)->_locked) return GS_throwerror(v,_SC("the class is locked"));
    _class(o)->_udsize = udsize;
    return GS_OK;
}


GSRESULT GS_getinstanceup(HGameScriptVM v, GSInteger idx, GSUserPointer *p, GSUserPointer typetag, GSBool throwerror)
{
	GSObjectPtr &o = stack_get(v, idx);
	if (GS_type(o) != OT_INSTANCE) return throwerror ? GS_throwerror(v, _SC("the object is not a class instance")) : GS_ERROR;
	(*p) = _instance(o)->_userpointer;
	if (typetag != 0) {
		GSClass *cl = _instance(o)->_class;
		do {
			if (cl->_typetag == typetag)
				return GS_OK;
			cl = cl->_base;
		} while (cl != NULL);
		return throwerror ? GS_throwerror(v, _SC("invalid type tag")) : GS_ERROR;
	}
	return GS_OK;
}

GSInteger GS_gettop(HGameScriptVM v)
{
    return (v->_top) - v->_stackbase;
}

void GS_settop(HGameScriptVM v, GSInteger newtop)
{
    GSInteger top = GS_gettop(v);
    if(top > newtop)
        GS_pop(v, top - newtop);
    else
        while(top++ < newtop) GS_pushnull(v);
}

void GS_pop(HGameScriptVM v, GSInteger nelemstopop)
{
    assert(v->_top >= nelemstopop);
    v->Pop(nelemstopop);
}

void GS_poptop(HGameScriptVM v)
{
    assert(v->_top >= 1);
    v->Pop();
}


void GS_remove(HGameScriptVM v, GSInteger idx)
{
    v->Remove(idx);
}

GSInteger GS_cmp(HGameScriptVM v)
{
    GSInteger res;
    v->ObjCmp(stack_get(v, -1), stack_get(v, -2),res);
    return res;
}

GSRESULT GS_newslot(HGameScriptVM v, GSInteger idx, GSBool bstatic)
{
    GS_aux_paramscheck(v, 3);
    GSObjectPtr &self = stack_get(v, idx);
    if(GS_type(self) == OT_TABLE || GS_type(self) == OT_CLASS) {
        GSObjectPtr &key = v->GetUp(-2);
        if(GS_type(key) == OT_NULL) return GS_throwerror(v, _SC("null is not a valid key"));
        v->NewSlot(self, key, v->GetUp(-1),bstatic?true:false);
        v->Pop(2);
    }
    return GS_OK;
}

GSRESULT GS_deleteslot(HGameScriptVM v,GSInteger idx,GSBool pushval)
{
    GS_aux_paramscheck(v, 2);
    GSObjectPtr *self;
    _GETSAFE_OBJ(v, idx, OT_TABLE,self);
    GSObjectPtr &key = v->GetUp(-1);
    if(GS_type(key) == OT_NULL) return GS_throwerror(v, _SC("null is not a valid key"));
    GSObjectPtr res;
    if(!v->DeleteSlot(*self, key, res)){
        v->Pop();
        return GS_ERROR;
    }
    if(pushval) v->GetUp(-1) = res;
    else v->Pop();
    return GS_OK;
}

GSRESULT GS_set(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &self = stack_get(v, idx);
    if(v->Set(self, v->GetUp(-2), v->GetUp(-1),DONT_FALL_BACK)) {
        v->Pop(2);
        return GS_OK;
    }
    return GS_ERROR;
}

GSRESULT GS_rawset(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &self = stack_get(v, idx);
    GSObjectPtr &key = v->GetUp(-2);
    if(GS_type(key) == OT_NULL) {
        v->Pop(2);
        return GS_throwerror(v, _SC("null key"));
    }
    switch(GS_type(self)) {
    case OT_TABLE:
        _table(self)->NewSlot(key, v->GetUp(-1));
        v->Pop(2);
        return GS_OK;
    break;
    case OT_CLASS:
        _class(self)->NewSlot(_ss(v), key, v->GetUp(-1),false);
        v->Pop(2);
        return GS_OK;
    break;
    case OT_INSTANCE:
        if(_instance(self)->Set(key, v->GetUp(-1))) {
            v->Pop(2);
            return GS_OK;
        }
    break;
    case OT_ARRAY:
        if(v->Set(self, key, v->GetUp(-1),false)) {
            v->Pop(2);
            return GS_OK;
        }
    break;
    default:
        v->Pop(2);
        return GS_throwerror(v, _SC("rawset works only on array/table/class and instance"));
    }
    v->Raise_IdxError(v->GetUp(-2));return GS_ERROR;
}

GSRESULT GS_newmember(HGameScriptVM v,GSInteger idx,GSBool bstatic)
{
    GSObjectPtr &self = stack_get(v, idx);
    if(GS_type(self) != OT_CLASS) return GS_throwerror(v, _SC("new member only works with classes"));
    GSObjectPtr &key = v->GetUp(-3);
    if(GS_type(key) == OT_NULL) return GS_throwerror(v, _SC("null key"));
    if(!v->NewSlotA(self,key,v->GetUp(-2),v->GetUp(-1),bstatic?true:false,false)) {
        v->Pop(3);
        return GS_ERROR;
    }
    v->Pop(3);
    return GS_OK;
}

GSRESULT GS_rawnewmember(HGameScriptVM v,GSInteger idx,GSBool bstatic)
{
    GSObjectPtr &self = stack_get(v, idx);
    if(GS_type(self) != OT_CLASS) return GS_throwerror(v, _SC("new member only works with classes"));
    GSObjectPtr &key = v->GetUp(-3);
    if(GS_type(key) == OT_NULL) return GS_throwerror(v, _SC("null key"));
    if(!v->NewSlotA(self,key,v->GetUp(-2),v->GetUp(-1),bstatic?true:false,true)) {
        v->Pop(3);
        return GS_ERROR;
    }
    v->Pop(3);
    return GS_OK;
}

GSRESULT GS_setdelegate(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &self = stack_get(v, idx);
    GSObjectPtr &mt = v->GetUp(-1);
    GSObjectType type = GS_type(self);
    switch(type) {
    case OT_TABLE:
        if(GS_type(mt) == OT_TABLE) {
            if(!_table(self)->SetDelegate(_table(mt))) {
                return GS_throwerror(v, _SC("delegate cycle"));
            }
            v->Pop();
        }
        else if(GS_type(mt)==OT_NULL) {
            _table(self)->SetDelegate(NULL); v->Pop(); }
        else return GS_aux_invalidtype(v,type);
        break;
    case OT_USERDATA:
        if(GS_type(mt)==OT_TABLE) {
            _userdata(self)->SetDelegate(_table(mt)); v->Pop(); }
        else if(GS_type(mt)==OT_NULL) {
            _userdata(self)->SetDelegate(NULL); v->Pop(); }
        else return GS_aux_invalidtype(v, type);
        break;
    default:
            return GS_aux_invalidtype(v, type);
        break;
    }
    return GS_OK;
}

GSRESULT GS_rawdeleteslot(HGameScriptVM v,GSInteger idx,GSBool pushval)
{
    GS_aux_paramscheck(v, 2);
    GSObjectPtr *self;
    _GETSAFE_OBJ(v, idx, OT_TABLE,self);
    GSObjectPtr &key = v->GetUp(-1);
    GSObjectPtr t;
    if(_table(*self)->Get(key,t)) {
        _table(*self)->Remove(key);
    }
    if(pushval != 0)
        v->GetUp(-1) = t;
    else
        v->Pop();
    return GS_OK;
}

GSRESULT GS_getdelegate(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &self=stack_get(v,idx);
    switch(GS_type(self)){
    case OT_TABLE:
    case OT_USERDATA:
        if(!_delegable(self)->_delegate){
            v->PushNull();
            break;
        }
        v->Push(GSObjectPtr(_delegable(self)->_delegate));
        break;
    default: return GS_throwerror(v,_SC("wrong type")); break;
    }
    return GS_OK;

}

GSRESULT GS_get(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &self=stack_get(v,idx);
    GSObjectPtr &obj = v->GetUp(-1);
    if(v->Get(self,obj,obj,false,DONT_FALL_BACK))
        return GS_OK;
    v->Pop();
    return GS_ERROR;
}

GSRESULT GS_rawget(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &self=stack_get(v,idx);
    GSObjectPtr &obj = v->GetUp(-1);
    switch(GS_type(self)) {
    case OT_TABLE:
        if(_table(self)->Get(obj,obj))
            return GS_OK;
        break;
    case OT_CLASS:
        if(_class(self)->Get(obj,obj))
            return GS_OK;
        break;
    case OT_INSTANCE:
        if(_instance(self)->Get(obj,obj))
            return GS_OK;
        break;
    case OT_ARRAY:{
        if(GS_isnumeric(obj)){
            if(_array(self)->Get(tointeger(obj),obj)) {
                return GS_OK;
            }
        }
        else {
            v->Pop();
            return GS_throwerror(v,_SC("invalid index type for an array"));
        }
                  }
        break;
    default:
        v->Pop();
        return GS_throwerror(v,_SC("rawget works only on array/table/instance and class"));
    }
    v->Pop();
    return GS_throwerror(v,_SC("the index doesn't exist"));
}

GSRESULT GS_getstackobj(HGameScriptVM v,GSInteger idx,HGSOBJECT *po)
{
    *po=stack_get(v,idx);
    return GS_OK;
}

const GSChar *GS_getlocal(HGameScriptVM v,GSUnsignedInteger level,GSUnsignedInteger idx)
{
    GSUnsignedInteger cstksize=v->_callsstacksize;
    GSUnsignedInteger lvl=(cstksize-level)-1;
    GSInteger stackbase=v->_stackbase;
    if(lvl<cstksize){
        for(GSUnsignedInteger i=0;i<level;i++){
            GSVM::CallInfo &ci=v->_callsstack[(cstksize-i)-1];
            stackbase-=ci._prevstkbase;
        }
        GSVM::CallInfo &ci=v->_callsstack[lvl];
        if(GS_type(ci._closure)!=OT_CLOSURE)
            return NULL;
        GSClosure *c=_closure(ci._closure);
        GSFunctionProto *func=c->_function;
        if(func->_noutervalues > (GSInteger)idx) {
            v->Push(*_outer(c->_outervalues[idx])->_valptr);
            return _stringval(func->_outervalues[idx]._name);
        }
        idx -= func->_noutervalues;
        return func->GetLocal(v,stackbase,idx,(GSInteger)(ci._ip-func->_instructions)-1);
    }
    return NULL;
}

void GS_pushobject(HGameScriptVM v,HGSOBJECT obj)
{
    v->Push(GSObjectPtr(obj));
}

void GS_resetobject(HGSOBJECT *po)
{
    po->_unVal.pUserPointer=NULL;po->_type=OT_NULL;
}

GSRESULT GS_throwerror(HGameScriptVM v,const GSChar *err)
{
    v->_lasterror=GSString::Create(_ss(v),err);
    return GS_ERROR;
}

GSRESULT GS_throwobject(HGameScriptVM v)
{
    v->_lasterror = v->GetUp(-1);
    v->Pop();
    return GS_ERROR;
}


void GS_reseterror(HGameScriptVM v)
{
    v->_lasterror.Null();
}

void GS_getlasterror(HGameScriptVM v)
{
    v->Push(v->_lasterror);
}

GSRESULT GS_reservestack(HGameScriptVM v,GSInteger nsize)
{
    if (((GSUnsignedInteger)v->_top + nsize) > v->_stack.size()) {
        if(v->_nmetamethodscall) {
            return GS_throwerror(v,_SC("cannot resize stack while in a metamethod"));
        }
        v->_stack.resize(v->_stack.size() + ((v->_top + nsize) - v->_stack.size()));
    }
    return GS_OK;
}

GSRESULT GS_resume(HGameScriptVM v,GSBool retval,GSBool raiseerror)
{
    if (GS_type(v->GetUp(-1)) == OT_GENERATOR)
    {
        v->PushNull(); //retval
        if (!v->Execute(v->GetUp(-2), 0, v->_top, v->GetUp(-1), raiseerror, GSVM::ET_RESUME_GENERATOR))
        {v->Raise_Error(v->_lasterror); return GS_ERROR;}
        if(!retval)
            v->Pop();
        return GS_OK;
    }
    return GS_throwerror(v,_SC("only generators can be resumed"));
}

GSRESULT GS_call(HGameScriptVM v,GSInteger params,GSBool retval,GSBool raiseerror)
{
    GSObjectPtr res;
    if(!v->Call(v->GetUp(-(params+1)),params,v->_top-params,res,raiseerror?true:false)){
        v->Pop(params); //pop args
        return GS_ERROR;
    }
    if(!v->_suspended)
        v->Pop(params); //pop args
    if(retval)
        v->Push(res); // push result
    return GS_OK;
}

GSRESULT GS_tailcall(HGameScriptVM v, GSInteger nparams)
{
	GSObjectPtr &res = v->GetUp(-(nparams + 1));
	if (GS_type(res) != OT_CLOSURE) {
		return GS_throwerror(v, _SC("only closure can be tail called"));
	}
	GSClosure *clo = _closure(res);
	if (clo->_function->_bgenerator)
	{
		return GS_throwerror(v, _SC("generators cannot be tail called"));
	}
	
	GSInteger stackbase = (v->_top - nparams) - v->_stackbase;
	if (!v->TailCall(clo, stackbase, nparams)) {
		return GS_ERROR;
	}
	return GS_TAILCALL_FLAG;
}

GSRESULT GS_suspendvm(HGameScriptVM v)
{
    return v->Suspend();
}

GSRESULT GS_wakeupvm(HGameScriptVM v,GSBool wakeupret,GSBool retval,GSBool raiseerror,GSBool throwerror)
{
    GSObjectPtr ret;
    if(!v->_suspended)
        return GS_throwerror(v,_SC("cannot resume a vm that is not running any code"));
    GSInteger target = v->_suspended_target;
    if(wakeupret) {
        if(target != -1) {
            v->GetAt(v->_stackbase+v->_suspended_target)=v->GetUp(-1); //retval
        }
        v->Pop();
    } else if(target != -1) { v->GetAt(v->_stackbase+v->_suspended_target).Null(); }
    GSObjectPtr dummy;
    if(!v->Execute(dummy,-1,-1,ret,raiseerror,throwerror?GSVM::ET_RESUME_THROW_VM : GSVM::ET_RESUME_VM)) {
        return GS_ERROR;
    }
    if(retval)
        v->Push(ret);
    return GS_OK;
}

void GS_setreleasehook(HGameScriptVM v,GSInteger idx,GSRELEASEHOOK hook)
{
    GSObjectPtr &ud=stack_get(v,idx);
    switch(GS_type(ud) ) {
    case OT_USERDATA:   _userdata(ud)->_hook = hook;    break;
    case OT_INSTANCE:   _instance(ud)->_hook = hook;    break;
    case OT_CLASS:      _class(ud)->_hook = hook;       break;
    default: return;
    }
}

GSRELEASEHOOK GS_getreleasehook(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &ud=stack_get(v,idx);
    switch(GS_type(ud) ) {
    case OT_USERDATA:   return _userdata(ud)->_hook;    break;
    case OT_INSTANCE:   return _instance(ud)->_hook;    break;
    case OT_CLASS:      return _class(ud)->_hook;       break;
    default: return NULL;
    }
}

void GS_setcompilererrorhandler(HGameScriptVM v,GSCOMPILERERROR f)
{
    _ss(v)->_compilererrorhandler = f;
}

GSRESULT GS_writeclosure(HGameScriptVM v,GSWRITEFUNC w,GSUserPointer up)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, -1, OT_CLOSURE,o);
    unsigned short tag = GS_BYTECODE_STREAM_TAG;
    if(_closure(*o)->_function->_noutervalues)
        return GS_throwerror(v,_SC("a closure with free variables bound cannot be serialized"));
    if(w(up,&tag,2) != 2)
        return GS_throwerror(v,_SC("io error"));
    if(!_closure(*o)->Save(v,up,w))
        return GS_ERROR;
    return GS_OK;
}

GSRESULT GS_readclosure(HGameScriptVM v,GSREADFUNC r,GSUserPointer up)
{
    GSObjectPtr closure;

    unsigned short tag;
    if(r(up,&tag,2) != 2)
        return GS_throwerror(v,_SC("io error"));
    if(tag != GS_BYTECODE_STREAM_TAG)
        return GS_throwerror(v,_SC("invalid stream"));
    if(!GSClosure::Load(v,up,r,closure))
        return GS_ERROR;
    v->Push(closure);
    return GS_OK;
}

GSChar *GS_getscratchpad(HGameScriptVM v,GSInteger minsize)
{
    return _ss(v)->GetScratchPad(minsize);
}

GSRESULT GS_resurrectunreachable(HGameScriptVM v)
{
#ifndef NO_GARBAGE_COLLECTOR
    _ss(v)->ResurrectUnreachable(v);
    return GS_OK;
#else
    return GS_throwerror(v,_SC("GS_resurrectunreachable requires a garbage collector build"));
#endif
}

GSInteger GS_collectgarbage(HGameScriptVM v)
{
#ifndef NO_GARBAGE_COLLECTOR
    return _ss(v)->CollectGarbage(v);
#else
    return -1;
#endif
}

GSRESULT GS_getcallee(HGameScriptVM v)
{
    if(v->_callsstacksize > 1)
    {
        v->Push(v->_callsstack[v->_callsstacksize - 2]._closure);
        return GS_OK;
    }
    return GS_throwerror(v,_SC("no closure in the calls stack"));
}

const GSChar *GS_getfreevariable(HGameScriptVM v,GSInteger idx,GSUnsignedInteger nval)
{
    GSObjectPtr &self=stack_get(v,idx);
    const GSChar *name = NULL;
    switch(GS_type(self))
    {
    case OT_CLOSURE:{
        GSClosure *clo = _closure(self);
        GSFunctionProto *fp = clo->_function;
        if(((GSUnsignedInteger)fp->_noutervalues) > nval) {
            v->Push(*(_outer(clo->_outervalues[nval])->_valptr));
            GSOuterVar &ov = fp->_outervalues[nval];
            name = _stringval(ov._name);
        }
                    }
        break;
    case OT_NATIVECLOSURE:{
        GSNativeClosure *clo = _nativeclosure(self);
        if(clo->_noutervalues > nval) {
            v->Push(clo->_outervalues[nval]);
            name = _SC("@NATIVE");
        }
                          }
        break;
    default: break; //shutup compiler
    }
    return name;
}

GSRESULT GS_setfreevariable(HGameScriptVM v,GSInteger idx,GSUnsignedInteger nval)
{
    GSObjectPtr &self=stack_get(v,idx);
    switch(GS_type(self))
    {
    case OT_CLOSURE:{
        GSFunctionProto *fp = _closure(self)->_function;
        if(((GSUnsignedInteger)fp->_noutervalues) > nval){
            *(_outer(_closure(self)->_outervalues[nval])->_valptr) = stack_get(v,-1);
        }
        else return GS_throwerror(v,_SC("invalid free var index"));
                    }
        break;
    case OT_NATIVECLOSURE:
        if(_nativeclosure(self)->_noutervalues > nval){
            _nativeclosure(self)->_outervalues[nval] = stack_get(v,-1);
        }
        else return GS_throwerror(v,_SC("invalid free var index"));
        break;
    default:
        return GS_aux_invalidtype(v, GS_type(self));
    }
    v->Pop();
    return GS_OK;
}

GSRESULT GS_setattributes(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_CLASS,o);
    GSObjectPtr &key = stack_get(v,-2);
    GSObjectPtr &val = stack_get(v,-1);
    GSObjectPtr attrs;
    if(GS_type(key) == OT_NULL) {
        attrs = _class(*o)->_attributes;
        _class(*o)->_attributes = val;
        v->Pop(2);
        v->Push(attrs);
        return GS_OK;
    }else if(_class(*o)->GetAttributes(key,attrs)) {
        _class(*o)->SetAttributes(key,val);
        v->Pop(2);
        v->Push(attrs);
        return GS_OK;
    }
    return GS_throwerror(v,_SC("wrong index"));
}

GSRESULT GS_getattributes(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_CLASS,o);
    GSObjectPtr &key = stack_get(v,-1);
    GSObjectPtr attrs;
    if(GS_type(key) == OT_NULL) {
        attrs = _class(*o)->_attributes;
        v->Pop();
        v->Push(attrs);
        return GS_OK;
    }
    else if(_class(*o)->GetAttributes(key,attrs)) {
        v->Pop();
        v->Push(attrs);
        return GS_OK;
    }
    return GS_throwerror(v,_SC("wrong index"));
}

GSRESULT GS_getmemberhandle(HGameScriptVM v,GSInteger idx,HGSMEMBERHANDLE *handle)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_CLASS,o);
    GSObjectPtr &key = stack_get(v,-1);
    GSTable *m = _class(*o)->_members;
    GSObjectPtr val;
    if(m->Get(key,val)) {
        handle->_static = _isfield(val) ? GSFalse : GSTrue;
        handle->_index = _member_idx(val);
        v->Pop();
        return GS_OK;
    }
    return GS_throwerror(v,_SC("wrong index"));
}

GSRESULT _getmemberbyhandle(HGameScriptVM v,GSObjectPtr &self,const HGSMEMBERHANDLE *handle,GSObjectPtr *&val)
{
    switch(GS_type(self)) {
        case OT_INSTANCE: {
                GSInstance *i = _instance(self);
                if(handle->_static) {
                    GSClass *c = i->_class;
                    val = &c->_methods[handle->_index].val;
                }
                else {
                    val = &i->_values[handle->_index];

                }
            }
            break;
        case OT_CLASS: {
                GSClass *c = _class(self);
                if(handle->_static) {
                    val = &c->_methods[handle->_index].val;
                }
                else {
                    val = &c->_defaultvalues[handle->_index].val;
                }
            }
            break;
        default:
            return GS_throwerror(v,_SC("wrong type(expected class or instance)"));
    }
    return GS_OK;
}

GSRESULT GS_getbyhandle(HGameScriptVM v,GSInteger idx,const HGSMEMBERHANDLE *handle)
{
    GSObjectPtr &self = stack_get(v,idx);
    GSObjectPtr *val = NULL;
    if(GS_FAILED(_getmemberbyhandle(v,self,handle,val))) {
        return GS_ERROR;
    }
    v->Push(_realval(*val));
    return GS_OK;
}

GSRESULT GS_setbyhandle(HGameScriptVM v,GSInteger idx,const HGSMEMBERHANDLE *handle)
{
    GSObjectPtr &self = stack_get(v,idx);
    GSObjectPtr &newval = stack_get(v,-1);
    GSObjectPtr *val = NULL;
    if(GS_FAILED(_getmemberbyhandle(v,self,handle,val))) {
        return GS_ERROR;
    }
    *val = newval;
    v->Pop();
    return GS_OK;
}

GSRESULT GS_getbase(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_CLASS,o);
    if(_class(*o)->_base)
        v->Push(GSObjectPtr(_class(*o)->_base));
    else
        v->PushNull();
    return GS_OK;
}

GSRESULT GS_getclass(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_INSTANCE,o);
    v->Push(GSObjectPtr(_instance(*o)->_class));
    return GS_OK;
}

GSRESULT GS_createinstance(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr *o = NULL;
    _GETSAFE_OBJ(v, idx, OT_CLASS,o);
    v->Push(_class(*o)->CreateInstance());
    return GS_OK;
}

void GS_weakref(HGameScriptVM v,GSInteger idx)
{
    GSObject &o=stack_get(v,idx);
    if(ISREFCOUNTED(GS_type(o))) {
        v->Push(_refcounted(o)->GetWeakRef(GS_type(o)));
        return;
    }
    v->Push(o);
}

GSRESULT GS_getweakrefval(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr &o = stack_get(v,idx);
    if(GS_type(o) != OT_WEAKREF) {
        return GS_throwerror(v,_SC("the object must be a weakref"));
    }
    v->Push(_weakref(o)->_obj);
    return GS_OK;
}

GSRESULT GS_getdefaultdelegate(HGameScriptVM v,GSObjectType t)
{
    GSSharedState *ss = _ss(v);
    switch(t) {
    case OT_TABLE: v->Push(ss->_table_default_delegate); break;
    case OT_ARRAY: v->Push(ss->_array_default_delegate); break;
    case OT_STRING: v->Push(ss->_string_default_delegate); break;
    case OT_INTEGER: case OT_FLOAT: v->Push(ss->_number_default_delegate); break;
    case OT_GENERATOR: v->Push(ss->_generator_default_delegate); break;
    case OT_CLOSURE: case OT_NATIVECLOSURE: v->Push(ss->_closure_default_delegate); break;
    case OT_THREAD: v->Push(ss->_thread_default_delegate); break;
    case OT_CLASS: v->Push(ss->_class_default_delegate); break;
    case OT_INSTANCE: v->Push(ss->_instance_default_delegate); break;
    case OT_WEAKREF: v->Push(ss->_weakref_default_delegate); break;
    default: return GS_throwerror(v,_SC("the type doesn't have a default delegate"));
    }
    return GS_OK;
}

GSRESULT GS_next(HGameScriptVM v,GSInteger idx)
{
    GSObjectPtr o=stack_get(v,idx),&refpos = stack_get(v,-1),realkey,val;
    if(GS_type(o) == OT_GENERATOR) {
        return GS_throwerror(v,_SC("cannot iterate a generator"));
    }
    int faketojump;
    if(!v->FOREACH_OP(o,realkey,val,refpos,0,666,faketojump))
        return GS_ERROR;
    if(faketojump != 666) {
        v->Push(realkey);
        v->Push(val);
        return GS_OK;
    }
    return GS_ERROR;
}

struct BufState{
    const GSChar *buf;
    GSInteger ptr;
    GSInteger size;
};

GSInteger buf_lexfeed(GSUserPointer file)
{
    BufState *buf=(BufState*)file;
    if(buf->size<(buf->ptr+1))
        return 0;
    return buf->buf[buf->ptr++];
}

GSRESULT GS_compilebuffer(HGameScriptVM v,const GSChar *s,GSInteger size,const GSChar *sourcename,GSBool raiseerror) {
    BufState buf;
    buf.buf = s;
    buf.size = size;
    buf.ptr = 0;
    return GS_compile(v, buf_lexfeed, &buf, sourcename, raiseerror);
}

void GS_move(HGameScriptVM dest,HGameScriptVM src,GSInteger idx)
{
    dest->Push(stack_get(src,idx));
}

void GS_setprintfunc(HGameScriptVM v, GSPRINTFUNCTION printfunc,GSPRINTFUNCTION errfunc)
{
    _ss(v)->_printfunc = printfunc;
    _ss(v)->_errorfunc = errfunc;
}

GSPRINTFUNCTION GS_getprintfunc(HGameScriptVM v)
{
    return _ss(v)->_printfunc;
}

GSPRINTFUNCTION GS_geterrorfunc(HGameScriptVM v)
{
    return _ss(v)->_errorfunc;
}

void *GS_malloc(GSUnsignedInteger size)
{
    return GS_MALLOC(size);
}

void *GS_realloc(void* p,GSUnsignedInteger oldsize,GSUnsignedInteger newsize)
{
    return GS_REALLOC(p,oldsize,newsize);
}

void GS_free(void *p,GSUnsignedInteger size)
{
    GS_FREE(p,size);
}

