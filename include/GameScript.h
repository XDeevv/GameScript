/*
Copyright (c) 2003-2024 Alberto Demichelis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef _GameScript_H_
#define _GameScript_H_

#ifdef _GS_CONFIG_INCLUDE
#include _GS_CONFIG_INCLUDE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GameScript_API
#define GameScript_API extern
#endif

#if (defined(_WIN64) || defined(_LP64))
#ifndef _GS64
#define _GS64
#endif
#endif


#define GSTrue  (1)
#define GSFalse (0)

struct GSVM;
struct GSTable;
struct GSArray;
struct GSString;
struct GSClosure;
struct GSGenerator;
struct GSNativeClosure;
struct GSUserData;
struct GSFunctionProto;
struct GSRefCounted;
struct GSClass;
struct GSInstance;
struct GSDelegable;
struct GSOuter;

#ifdef _UNICODE
#define GSUNICODE
#endif

#include "GSconfig.h"

#define GameScript_VERSION    _SC("GameScript 3.2 stable")
#define GameScript_COPYRIGHT  _SC("Copyright (C) 2003-2024 Alberto Demichelis")
#define GameScript_AUTHOR     _SC("Alberto Demichelis")
#define GameScript_VERSION_NUMBER 320

#define GS_VMSTATE_IDLE         0
#define GS_VMSTATE_RUNNING      1
#define GS_VMSTATE_SUSPENDED    2

#define GameScript_EOB 0
#define GS_BYTECODE_STREAM_TAG  0xFAFA

#define GSOBJECT_REF_COUNTED    0x08000000
#define GSOBJECT_NUMERIC        0x04000000
#define GSOBJECT_DELEGABLE      0x02000000
#define GSOBJECT_CANBEFALSE     0x01000000

#define GS_MATCHTYPEMASKSTRING (-99999)

#define _RT_MASK 0x00FFFFFF
#define _RAW_TYPE(type) (type&_RT_MASK)

#define _RT_NULL            0x00000001
#define _RT_INTEGER         0x00000002
#define _RT_FLOAT           0x00000004
#define _RT_BOOL            0x00000008
#define _RT_STRING          0x00000010
#define _RT_TABLE           0x00000020
#define _RT_ARRAY           0x00000040
#define _RT_USERDATA        0x00000080
#define _RT_CLOSURE         0x00000100
#define _RT_NATIVECLOSURE   0x00000200
#define _RT_GENERATOR       0x00000400
#define _RT_USERPOINTER     0x00000800
#define _RT_THREAD          0x00001000
#define _RT_FUNCPROTO       0x00002000
#define _RT_CLASS           0x00004000
#define _RT_INSTANCE        0x00008000
#define _RT_WEAKREF         0x00010000
#define _RT_OUTER           0x00020000

typedef enum tagGSObjectType{
    OT_NULL =           (_RT_NULL|GSOBJECT_CANBEFALSE),
    OT_INTEGER =        (_RT_INTEGER|GSOBJECT_NUMERIC|GSOBJECT_CANBEFALSE),
    OT_FLOAT =          (_RT_FLOAT|GSOBJECT_NUMERIC|GSOBJECT_CANBEFALSE),
    OT_BOOL =           (_RT_BOOL|GSOBJECT_CANBEFALSE),
    OT_STRING =         (_RT_STRING|GSOBJECT_REF_COUNTED),
    OT_TABLE =          (_RT_TABLE|GSOBJECT_REF_COUNTED|GSOBJECT_DELEGABLE),
    OT_ARRAY =          (_RT_ARRAY|GSOBJECT_REF_COUNTED),
    OT_USERDATA =       (_RT_USERDATA|GSOBJECT_REF_COUNTED|GSOBJECT_DELEGABLE),
    OT_CLOSURE =        (_RT_CLOSURE|GSOBJECT_REF_COUNTED),
    OT_NATIVECLOSURE =  (_RT_NATIVECLOSURE|GSOBJECT_REF_COUNTED),
    OT_GENERATOR =      (_RT_GENERATOR|GSOBJECT_REF_COUNTED),
    OT_USERPOINTER =    _RT_USERPOINTER,
    OT_THREAD =         (_RT_THREAD|GSOBJECT_REF_COUNTED) ,
    OT_FUNCPROTO =      (_RT_FUNCPROTO|GSOBJECT_REF_COUNTED), //internal usage only
    OT_CLASS =          (_RT_CLASS|GSOBJECT_REF_COUNTED),
    OT_INSTANCE =       (_RT_INSTANCE|GSOBJECT_REF_COUNTED|GSOBJECT_DELEGABLE),
    OT_WEAKREF =        (_RT_WEAKREF|GSOBJECT_REF_COUNTED),
    OT_OUTER =          (_RT_OUTER|GSOBJECT_REF_COUNTED) //internal usage only
}GSObjectType;

#define ISREFCOUNTED(t) (t&GSOBJECT_REF_COUNTED)


typedef union tagGSObjectValue
{
    struct GSTable *pTable;
    struct GSArray *pArray;
    struct GSClosure *pClosure;
    struct GSOuter *pOuter;
    struct GSGenerator *pGenerator;
    struct GSNativeClosure *pNativeClosure;
    struct GSString *pString;
    struct GSUserData *pUserData;
    GSInteger nInteger;
    GSFloat fFloat;
    GSUserPointer pUserPointer;
    struct GSFunctionProto *pFunctionProto;
    struct GSRefCounted *pRefCounted;
    struct GSDelegable *pDelegable;
    struct GSVM *pThread;
    struct GSClass *pClass;
    struct GSInstance *pInstance;
    struct GSWeakRef *pWeakRef;
    GSRawObjectVal raw;
}GSObjectValue;


typedef struct tagGSObject
{
    GSObjectType _type;
    GSObjectValue _unVal;
}GSObject;

typedef struct  tagGSMemberHandle{
    GSBool _static;
    GSInteger _index;
}GSMemberHandle;

typedef struct tagGSStackInfos{
    const GSChar* funcname;
    const GSChar* source;
    GSInteger line;
}GSStackInfos;

typedef struct GSVM* HGameScriptVM;
typedef GSObject HGSOBJECT;
typedef GSMemberHandle HGSMEMBERHANDLE;
typedef GSInteger (*GSFUNCTION)(HGameScriptVM);
typedef GSInteger (*GSRELEASEHOOK)(GSUserPointer,GSInteger size);
typedef void (*GSCOMPILERERROR)(HGameScriptVM,const GSChar * /*desc*/,const GSChar * /*source*/,GSInteger /*line*/,GSInteger /*column*/);
typedef void (*GSPRINTFUNCTION)(HGameScriptVM,const GSChar * ,...);
typedef void (*GSDEBUGHOOK)(HGameScriptVM /*v*/, GSInteger /*type*/, const GSChar * /*sourcename*/, GSInteger /*line*/, const GSChar * /*funcname*/);
typedef GSInteger (*GSWRITEFUNC)(GSUserPointer,GSUserPointer,GSInteger);
typedef GSInteger (*GSREADFUNC)(GSUserPointer,GSUserPointer,GSInteger);

typedef GSInteger (*GSLEXREADFUNC)(GSUserPointer);

typedef struct tagGSRegFunction{
    const GSChar *name;
    GSFUNCTION f;
    GSInteger nparamscheck;
    const GSChar *typemask;
}GSRegFunction;

typedef struct tagGSFunctionInfo {
    GSUserPointer funcid;
    const GSChar *name;
    const GSChar *source;
    GSInteger line;
}GSFunctionInfo;

/*vm*/
GameScript_API HGameScriptVM GS_open(GSInteger initialstacksize);
GameScript_API HGameScriptVM GS_newthread(HGameScriptVM friendvm, GSInteger initialstacksize);
GameScript_API void GS_seterrorhandler(HGameScriptVM v);
GameScript_API void GS_close(HGameScriptVM v);
GameScript_API void GS_setforeignptr(HGameScriptVM v,GSUserPointer p);
GameScript_API GSUserPointer GS_getforeignptr(HGameScriptVM v);
GameScript_API void GS_setsharedforeignptr(HGameScriptVM v,GSUserPointer p);
GameScript_API GSUserPointer GS_getsharedforeignptr(HGameScriptVM v);
GameScript_API void GS_setvmreleasehook(HGameScriptVM v,GSRELEASEHOOK hook);
GameScript_API GSRELEASEHOOK GS_getvmreleasehook(HGameScriptVM v);
GameScript_API void GS_setsharedreleasehook(HGameScriptVM v,GSRELEASEHOOK hook);
GameScript_API GSRELEASEHOOK GS_getsharedreleasehook(HGameScriptVM v);
GameScript_API void GS_setprintfunc(HGameScriptVM v, GSPRINTFUNCTION printfunc,GSPRINTFUNCTION errfunc);
GameScript_API GSPRINTFUNCTION GS_getprintfunc(HGameScriptVM v);
GameScript_API GSPRINTFUNCTION GS_geterrorfunc(HGameScriptVM v);
GameScript_API GSRESULT GS_suspendvm(HGameScriptVM v);
GameScript_API GSRESULT GS_wakeupvm(HGameScriptVM v,GSBool resumedret,GSBool retval,GSBool raiseerror,GSBool throwerror);
GameScript_API GSInteger GS_getvmstate(HGameScriptVM v);
GameScript_API GSInteger GS_getversion();

/*compiler*/
GameScript_API GSRESULT GS_compile(HGameScriptVM v,GSLEXREADFUNC read,GSUserPointer p,const GSChar *sourcename,GSBool raiseerror);
GameScript_API GSRESULT GS_compilebuffer(HGameScriptVM v,const GSChar *s,GSInteger size,const GSChar *sourcename,GSBool raiseerror);
GameScript_API void GS_enabledebuginfo(HGameScriptVM v, GSBool enable);
GameScript_API void GS_notifyallexceptions(HGameScriptVM v, GSBool enable);
GameScript_API void GS_setcompilererrorhandler(HGameScriptVM v,GSCOMPILERERROR f);

/*stack operations*/
GameScript_API void GS_push(HGameScriptVM v,GSInteger idx);
GameScript_API void GS_pop(HGameScriptVM v,GSInteger nelemstopop);
GameScript_API void GS_poptop(HGameScriptVM v);
GameScript_API void GS_remove(HGameScriptVM v,GSInteger idx);
GameScript_API GSInteger GS_gettop(HGameScriptVM v);
GameScript_API void GS_settop(HGameScriptVM v,GSInteger newtop);
GameScript_API GSRESULT GS_reservestack(HGameScriptVM v,GSInteger nsize);
GameScript_API GSInteger GS_cmp(HGameScriptVM v);
GameScript_API void GS_move(HGameScriptVM dest,HGameScriptVM src,GSInteger idx);

/*object creation handling*/
GameScript_API GSUserPointer GS_newuserdata(HGameScriptVM v,GSUnsignedInteger size);
GameScript_API void GS_newtable(HGameScriptVM v);
GameScript_API void GS_newtableex(HGameScriptVM v,GSInteger initialcapacity);
GameScript_API void GS_newarray(HGameScriptVM v,GSInteger size);
GameScript_API void GS_newclosure(HGameScriptVM v,GSFUNCTION func,GSUnsignedInteger nfreevars);
GameScript_API GSRESULT GS_setparamscheck(HGameScriptVM v,GSInteger nparamscheck,const GSChar *typemask);
GameScript_API GSRESULT GS_bindenv(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_setclosureroot(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_getclosureroot(HGameScriptVM v,GSInteger idx);
GameScript_API void GS_pushstring(HGameScriptVM v,const GSChar *s,GSInteger len);
GameScript_API void GS_pushfloat(HGameScriptVM v,GSFloat f);
GameScript_API void GS_pushinteger(HGameScriptVM v,GSInteger n);
GameScript_API void GS_pushbool(HGameScriptVM v,GSBool b);
GameScript_API void GS_pushuserpointer(HGameScriptVM v,GSUserPointer p);
GameScript_API void GS_pushnull(HGameScriptVM v);
GameScript_API void GS_pushthread(HGameScriptVM v, HGameScriptVM thread);
GameScript_API GSObjectType GS_gettype(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_typeof(HGameScriptVM v,GSInteger idx);
GameScript_API GSInteger GS_getsize(HGameScriptVM v,GSInteger idx);
GameScript_API GSHash GS_gethash(HGameScriptVM v, GSInteger idx);
GameScript_API GSRESULT GS_getbase(HGameScriptVM v,GSInteger idx);
GameScript_API GSBool GS_instanceof(HGameScriptVM v);
GameScript_API GSRESULT GS_tostring(HGameScriptVM v,GSInteger idx);
GameScript_API void GS_tobool(HGameScriptVM v, GSInteger idx, GSBool *b);
GameScript_API GSRESULT GS_getstringandsize(HGameScriptVM v,GSInteger idx,const GSChar **c,GSInteger *size);
GameScript_API GSRESULT GS_getstring(HGameScriptVM v,GSInteger idx,const GSChar **c);
GameScript_API GSRESULT GS_getinteger(HGameScriptVM v,GSInteger idx,GSInteger *i);
GameScript_API GSRESULT GS_getfloat(HGameScriptVM v,GSInteger idx,GSFloat *f);
GameScript_API GSRESULT GS_getbool(HGameScriptVM v,GSInteger idx,GSBool *b);
GameScript_API GSRESULT GS_getthread(HGameScriptVM v,GSInteger idx,HGameScriptVM *thread);
GameScript_API GSRESULT GS_getuserpointer(HGameScriptVM v,GSInteger idx,GSUserPointer *p);
GameScript_API GSRESULT GS_getuserdata(HGameScriptVM v,GSInteger idx,GSUserPointer *p,GSUserPointer *typetag);
GameScript_API GSRESULT GS_settypetag(HGameScriptVM v,GSInteger idx,GSUserPointer typetag);
GameScript_API GSRESULT GS_gettypetag(HGameScriptVM v,GSInteger idx,GSUserPointer *typetag);
GameScript_API void GS_setreleasehook(HGameScriptVM v,GSInteger idx,GSRELEASEHOOK hook);
GameScript_API GSRELEASEHOOK GS_getreleasehook(HGameScriptVM v,GSInteger idx);
GameScript_API GSChar *GS_getscratchpad(HGameScriptVM v,GSInteger minsize);
GameScript_API GSRESULT GS_getfunctioninfo(HGameScriptVM v,GSInteger level,GSFunctionInfo *fi);
GameScript_API GSRESULT GS_getclosureinfo(HGameScriptVM v,GSInteger idx,GSInteger *nparams,GSInteger *nfreevars);
GameScript_API GSRESULT GS_getclosurename(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_setnativeclosurename(HGameScriptVM v,GSInteger idx,const GSChar *name);
GameScript_API GSRESULT GS_setinstanceup(HGameScriptVM v, GSInteger idx, GSUserPointer p);
GameScript_API GSRESULT GS_getinstanceup(HGameScriptVM v, GSInteger idx, GSUserPointer *p,GSUserPointer typetag,GSBool throwerror);
GameScript_API GSRESULT GS_setclassudsize(HGameScriptVM v, GSInteger idx, GSInteger udsize);
GameScript_API GSRESULT GS_newclass(HGameScriptVM v,GSBool hasbase);
GameScript_API GSRESULT GS_createinstance(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_setattributes(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_getattributes(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_getclass(HGameScriptVM v,GSInteger idx);
GameScript_API void GS_weakref(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_getdefaultdelegate(HGameScriptVM v,GSObjectType t);
GameScript_API GSRESULT GS_getmemberhandle(HGameScriptVM v,GSInteger idx,HGSMEMBERHANDLE *handle);
GameScript_API GSRESULT GS_getbyhandle(HGameScriptVM v,GSInteger idx,const HGSMEMBERHANDLE *handle);
GameScript_API GSRESULT GS_setbyhandle(HGameScriptVM v,GSInteger idx,const HGSMEMBERHANDLE *handle);

/*object manipulation*/
GameScript_API void GS_pushroottable(HGameScriptVM v);
GameScript_API void GS_pushregistrytable(HGameScriptVM v);
GameScript_API void GS_pushconsttable(HGameScriptVM v);
GameScript_API GSRESULT GS_setroottable(HGameScriptVM v);
GameScript_API GSRESULT GS_setconsttable(HGameScriptVM v);
GameScript_API GSRESULT GS_newslot(HGameScriptVM v, GSInteger idx, GSBool bstatic);
GameScript_API GSRESULT GS_deleteslot(HGameScriptVM v,GSInteger idx,GSBool pushval);
GameScript_API GSRESULT GS_set(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_get(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_rawget(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_rawset(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_rawdeleteslot(HGameScriptVM v,GSInteger idx,GSBool pushval);
GameScript_API GSRESULT GS_newmember(HGameScriptVM v,GSInteger idx,GSBool bstatic);
GameScript_API GSRESULT GS_rawnewmember(HGameScriptVM v,GSInteger idx,GSBool bstatic);
GameScript_API GSRESULT GS_arrayappend(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_arraypop(HGameScriptVM v,GSInteger idx,GSBool pushval);
GameScript_API GSRESULT GS_arrayresize(HGameScriptVM v,GSInteger idx,GSInteger newsize);
GameScript_API GSRESULT GS_arrayreverse(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_arrayremove(HGameScriptVM v,GSInteger idx,GSInteger itemidx);
GameScript_API GSRESULT GS_arrayinsert(HGameScriptVM v,GSInteger idx,GSInteger destpos);
GameScript_API GSRESULT GS_setdelegate(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_getdelegate(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_clone(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_setfreevariable(HGameScriptVM v,GSInteger idx,GSUnsignedInteger nval);
GameScript_API GSRESULT GS_next(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_getweakrefval(HGameScriptVM v,GSInteger idx);
GameScript_API GSRESULT GS_clear(HGameScriptVM v,GSInteger idx);

/*calls*/
GameScript_API GSRESULT GS_call(HGameScriptVM v,GSInteger params,GSBool retval,GSBool raiseerror);
GameScript_API GSRESULT GS_resume(HGameScriptVM v,GSBool retval,GSBool raiseerror);
GameScript_API const GSChar *GS_getlocal(HGameScriptVM v,GSUnsignedInteger level,GSUnsignedInteger idx);
GameScript_API GSRESULT GS_getcallee(HGameScriptVM v);
GameScript_API const GSChar *GS_getfreevariable(HGameScriptVM v,GSInteger idx,GSUnsignedInteger nval);
GameScript_API GSRESULT GS_throwerror(HGameScriptVM v,const GSChar *err);
GameScript_API GSRESULT GS_throwobject(HGameScriptVM v);
GameScript_API void GS_reseterror(HGameScriptVM v);
GameScript_API void GS_getlasterror(HGameScriptVM v);
GameScript_API GSRESULT GS_tailcall(HGameScriptVM v, GSInteger nparams);

/*raw object handling*/
GameScript_API GSRESULT GS_getstackobj(HGameScriptVM v,GSInteger idx,HGSOBJECT *po);
GameScript_API void GS_pushobject(HGameScriptVM v,HGSOBJECT obj);
GameScript_API void GS_addref(HGameScriptVM v,HGSOBJECT *po);
GameScript_API GSBool GS_release(HGameScriptVM v,HGSOBJECT *po);
GameScript_API GSUnsignedInteger GS_getrefcount(HGameScriptVM v,HGSOBJECT *po);
GameScript_API void GS_resetobject(HGSOBJECT *po);
GameScript_API const GSChar *GS_objtostring(const HGSOBJECT *o);
GameScript_API GSBool GS_objtobool(const HGSOBJECT *o);
GameScript_API GSInteger GS_objtointeger(const HGSOBJECT *o);
GameScript_API GSFloat GS_objtofloat(const HGSOBJECT *o);
GameScript_API GSUserPointer GS_objtouserpointer(const HGSOBJECT *o);
GameScript_API GSRESULT GS_getobjtypetag(const HGSOBJECT *o,GSUserPointer * typetag);
GameScript_API GSUnsignedInteger GS_getvmrefcount(HGameScriptVM v, const HGSOBJECT *po);


/*GC*/
GameScript_API GSInteger GS_collectgarbage(HGameScriptVM v);
GameScript_API GSRESULT GS_resurrectunreachable(HGameScriptVM v);

/*serialization*/
GameScript_API GSRESULT GS_writeclosure(HGameScriptVM vm,GSWRITEFUNC writef,GSUserPointer up);
GameScript_API GSRESULT GS_readclosure(HGameScriptVM vm,GSREADFUNC readf,GSUserPointer up);

/*mem allocation*/
GameScript_API void *GS_malloc(GSUnsignedInteger size);
GameScript_API void *GS_realloc(void* p,GSUnsignedInteger oldsize,GSUnsignedInteger newsize);
GameScript_API void GS_free(void *p,GSUnsignedInteger size);

/*debug*/
GameScript_API GSRESULT GS_stackinfos(HGameScriptVM v,GSInteger level,GSStackInfos *si);
GameScript_API void GS_setdebughook(HGameScriptVM v);
GameScript_API void GS_setnativedebughook(HGameScriptVM v,GSDEBUGHOOK hook);

/*UTILITY MACRO*/
#define GS_isnumeric(o) ((o)._type&GSOBJECT_NUMERIC)
#define GS_istable(o) ((o)._type==OT_TABLE)
#define GS_isarray(o) ((o)._type==OT_ARRAY)
#define GS_isfunction(o) ((o)._type==OT_FUNCPROTO)
#define GS_isclosure(o) ((o)._type==OT_CLOSURE)
#define GS_isgenerator(o) ((o)._type==OT_GENERATOR)
#define GS_isnativeclosure(o) ((o)._type==OT_NATIVECLOSURE)
#define GS_isstring(o) ((o)._type==OT_STRING)
#define GS_isinteger(o) ((o)._type==OT_INTEGER)
#define GS_isfloat(o) ((o)._type==OT_FLOAT)
#define GS_isuserpointer(o) ((o)._type==OT_USERPOINTER)
#define GS_isuserdata(o) ((o)._type==OT_USERDATA)
#define GS_isthread(o) ((o)._type==OT_THREAD)
#define GS_isnull(o) ((o)._type==OT_NULL)
#define GS_isclass(o) ((o)._type==OT_CLASS)
#define GS_isinstance(o) ((o)._type==OT_INSTANCE)
#define GS_isbool(o) ((o)._type==OT_BOOL)
#define GS_isweakref(o) ((o)._type==OT_WEAKREF)
#define GS_type(o) ((o)._type)

/* deprecated */
#define GS_createslot(v,n) GS_newslot(v,n,GSFalse)

#define GS_OK (0)
#define GS_ERROR (-1)

#define GS_FAILED(res) (res<0)
#define GS_SUCCEEDED(res) (res>=0)

#ifdef __GNUC__
# define GS_UNUSED_ARG(x) x __attribute__((__unused__))
#else
# define GS_UNUSED_ARG(x) x
#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*_GameScript_H_*/

