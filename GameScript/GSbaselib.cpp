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
#include "GSclass.h"
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "version.h"

GSInteger GS_getversion()
{
    return (GAMESCRIPT_VERSION_MAJOR << 16) |
           (GAMESCRIPT_VERSION_MINOR << 8) |
            GAMESCRIPT_VERSION_PATCH;
}

static const GSChar* GetVersionString() {
    static GSChar versionBuf[16];
    scsprintf(versionBuf, 16, _SC("%d.%d.%d"), 
              GAMESCRIPT_VERSION_MAJOR, 
              GAMESCRIPT_VERSION_MINOR, 
              GAMESCRIPT_VERSION_PATCH);
    return versionBuf;
}

static bool str2num(const GSChar *s,GSObjectPtr &res,GSInteger base)
{
    GSChar *end;
    const GSChar *e = s;
    bool iseintbase = base > 13; //to fix error converting hexadecimals with e like 56f0791e
    bool isfloat = false;
    GSChar c;
    while((c = *e) != _SC('\0'))
    {
        if (c == _SC('.') || (!iseintbase && (c == _SC('E') || c == _SC('e')))) { //e and E is for scientific notation
            isfloat = true;
            break;
        }
        e++;
    }
    if(isfloat){
        GSFloat r = GSFloat(scstrtod(s,&end));
        if(s == end) return false;
        res = r;
    }
    else{
        GSInteger r = GSInteger(scstrtol(s,&end,(int)base));
        if(s == end) return false;
        res = r;
    }
    return true;
}

static GSInteger base_dummy(HGameScriptVM GS_UNUSED_ARG(v))
{
    return 0;
}

#ifndef NO_GARBAGE_COLLECTOR
static GSInteger base_collectgarbage(HGameScriptVM v)
{
    GS_pushinteger(v, GS_collectgarbage(v));
    return 1;
}
static GSInteger base_resurectureachable(HGameScriptVM v)
{
    GS_resurrectunreachable(v);
    return 1;
}
#endif

static GSInteger base_getroottable(HGameScriptVM v)
{
    v->Push(v->_roottable);
    return 1;
}

static GSInteger base_getconsttable(HGameScriptVM v)
{
    v->Push(_ss(v)->_consts);
    return 1;
}


static GSInteger base_setroottable(HGameScriptVM v)
{
    GSObjectPtr o = v->_roottable;
    if(GS_FAILED(GS_setroottable(v))) return GS_ERROR;
    v->Push(o);
    return 1;
}

static GSInteger base_setconsttable(HGameScriptVM v)
{
    GSObjectPtr o = _ss(v)->_consts;
    if(GS_FAILED(GS_setconsttable(v))) return GS_ERROR;
    v->Push(o);
    return 1;
}

static GSInteger base_seterrorhandler(HGameScriptVM v)
{
    GS_seterrorhandler(v);
    return 0;
}

static GSInteger base_setdebughook(HGameScriptVM v)
{
    GS_setdebughook(v);
    return 0;
}

static GSInteger base_enabledebuginfo(HGameScriptVM v)
{
    GSObjectPtr &o=stack_get(v,2);

    GS_enabledebuginfo(v,GSVM::IsFalse(o)?GSFalse:GSTrue);
    return 0;
}

static GSInteger __getcallstackinfos(HGameScriptVM v,GSInteger level)
{
    GSStackInfos si;
    GSInteger seq = 0;
    const GSChar *name = NULL;

    if (GS_SUCCEEDED(GS_stackinfos(v, level, &si)))
    {
        const GSChar *fn = _SC("unknown");
        const GSChar *src = _SC("unknown");
        if(si.funcname)fn = si.funcname;
        if(si.source)src = si.source;
        GS_newtable(v);
        GS_pushstring(v, _SC("func"), -1);
        GS_pushstring(v, fn, -1);
        GS_newslot(v, -3, GSFalse);
        GS_pushstring(v, _SC("src"), -1);
        GS_pushstring(v, src, -1);
        GS_newslot(v, -3, GSFalse);
        GS_pushstring(v, _SC("line"), -1);
        GS_pushinteger(v, si.line);
        GS_newslot(v, -3, GSFalse);
        GS_pushstring(v, _SC("locals"), -1);
        GS_newtable(v);
        seq=0;
        while ((name = GS_getlocal(v, level, seq))) {
            GS_pushstring(v, name, -1);
            GS_push(v, -2);
            GS_newslot(v, -4, GSFalse);
            GS_pop(v, 1);
            seq++;
        }
        GS_newslot(v, -3, GSFalse);
        return 1;
    }

    return 0;
}
static GSInteger base_getstackinfos(HGameScriptVM v)
{
    GSInteger level;
    GS_getinteger(v, -1, &level);
    return __getcallstackinfos(v,level);
}

static GSInteger base_assert(HGameScriptVM v)
{
    if(GSVM::IsFalse(stack_get(v,2))){
        GSInteger top = GS_gettop(v);
        if (top>2 && GS_SUCCEEDED(GS_tostring(v,3))) {
            const GSChar *str = 0;
            if (GS_SUCCEEDED(GS_getstring(v,-1,&str))) {
                return GS_throwerror(v, str);
            }
        }
        return GS_throwerror(v, _SC("assertion failed"));
    }
    return 0;
}

static GSInteger get_slice_params(HGameScriptVM v,GSInteger &sidx,GSInteger &eidx,GSObjectPtr &o)
{
    GSInteger top = GS_gettop(v);
    sidx=0;
    eidx=0;
    o=stack_get(v,1);
    if(top>1){
        GSObjectPtr &start=stack_get(v,2);
        if(GS_type(start)!=OT_NULL && GS_isnumeric(start)){
            sidx=tointeger(start);
        }
    }
    if(top>2){
        GSObjectPtr &end=stack_get(v,3);
        if(GS_isnumeric(end)){
            eidx=tointeger(end);
        }
    }
    else {
        eidx = GS_getsize(v,1);
    }
    return 1;
}

static GSInteger base_print(HGameScriptVM v)
{
    const GSChar *str;
    if(GS_SUCCEEDED(GS_tostring(v,2)))
    {
        if(GS_SUCCEEDED(GS_getstring(v,-1,&str))) {
            if(_ss(v)->_printfunc) _ss(v)->_printfunc(v,_SC("%s"),str);
            return 0;
        }
    }
    return GS_ERROR;
}

static GSInteger base_error(HGameScriptVM v)
{
    const GSChar *str;
    if(GS_SUCCEEDED(GS_tostring(v,2)))
    {
        if(GS_SUCCEEDED(GS_getstring(v,-1,&str))) {
            if(_ss(v)->_errorfunc) _ss(v)->_errorfunc(v,_SC("%s"),str);
            return 0;
        }
    }
    return GS_ERROR;
}

static GSInteger base_compilestring(HGameScriptVM v)
{
    GSInteger nargs=GS_gettop(v);
    const GSChar *src=NULL,*name=_SC("unnamedbuffer");
    GSInteger size;
    GS_getstring(v,2,&src);
    size=GS_getsize(v,2);
    if(nargs>2){
        GS_getstring(v,3,&name);
    }
    if(GS_SUCCEEDED(GS_compilebuffer(v,src,size,name,GSFalse)))
        return 1;
    else
        return GS_ERROR;
}

static GSInteger base_newthread(HGameScriptVM v)
{
    GSObjectPtr &func = stack_get(v,2);
    GSInteger stksize = (_closure(func)->_function->_stacksize << 1) +2;
    HGameScriptVM newv = GS_newthread(v, (stksize < MIN_STACK_OVERHEAD + 2)? MIN_STACK_OVERHEAD + 2 : stksize);
    GS_move(newv,v,-2);
    return 1;
}

static GSInteger base_suspend(HGameScriptVM v)
{
    return GS_suspendvm(v);
}

static GSInteger base_array(HGameScriptVM v)
{
    GSArray *a;
    GSObject &size = stack_get(v,2);
    if(GS_gettop(v) > 2) {
        a = GSArray::Create(_ss(v),0);
        a->Resize(tointeger(size),stack_get(v,3));
    }
    else {
        a = GSArray::Create(_ss(v),tointeger(size));
    }
    v->Push(a);
    return 1;
}

static GSInteger base_type(HGameScriptVM v)
{
    GSObjectPtr &o = stack_get(v,2);
    v->Push(GSString::Create(_ss(v),GetTypeName(o),-1));
    return 1;
}

static GSInteger base_callee(HGameScriptVM v)
{
    if(v->_callsstacksize > 1)
    {
        v->Push(v->_callsstack[v->_callsstacksize - 2]._closure);
        return 1;
    }
    return GS_throwerror(v,_SC("no closure in the calls stack"));
}

static const GSRegFunction base_funcs[]={
    //generic
    {_SC("seterrorhandler"),base_seterrorhandler,2, NULL},
    {_SC("setdebughook"),base_setdebughook,2, NULL},
    {_SC("enabledebuginfo"),base_enabledebuginfo,2, NULL},
    {_SC("getstackinfos"),base_getstackinfos,2, _SC(".n")},
    {_SC("getroottable"),base_getroottable,1, NULL},
    {_SC("setroottable"),base_setroottable,2, NULL},
    {_SC("getconsttable"),base_getconsttable,1, NULL},
    {_SC("setconsttable"),base_setconsttable,2, NULL},
    {_SC("assert"),base_assert,-2, NULL},
    {_SC("print"),base_print,2, NULL},
    {_SC("error"),base_error,2, NULL},
    {_SC("compilestring"),base_compilestring,-2, _SC(".ss")},
    {_SC("newthread"),base_newthread,2, _SC(".c")},
    {_SC("suspend"),base_suspend,-1, NULL},
    {_SC("array"),base_array,-2, _SC(".n")},
    {_SC("type"),base_type,2, NULL},
    {_SC("callee"),base_callee,0,NULL},
    {_SC("dummy"),base_dummy,0,NULL},
#ifndef NO_GARBAGE_COLLECTOR
    {_SC("collectgarbage"),base_collectgarbage,0, NULL},
    {_SC("resurrectunreachable"),base_resurectureachable,0, NULL},
#endif
    {NULL,(GSFUNCTION)0,0,NULL}
};

void GS_base_register(HGameScriptVM v)
{
    GSInteger i=0;
    GS_pushroottable(v);
    while(base_funcs[i].name!=0) {
        GS_pushstring(v,base_funcs[i].name,-1);
        GS_newclosure(v,base_funcs[i].f,0);
        GS_setnativeclosurename(v,-1,base_funcs[i].name);
        GS_setparamscheck(v,base_funcs[i].nparamscheck,base_funcs[i].typemask);
        GS_newslot(v,-3, GSFalse);
        i++;
    }
    GS_pushstring(v,_SC("_versionnumber_"),-1);
    GS_pushinteger(v,GS_getversion());
    GS_newslot(v,-3, GSFalse);
    GS_pushstring(v,_SC("_version_"),-1);
    GS_pushstring(v,GetVersionString(),-1);
    GS_newslot(v,-3, GSFalse);
    GS_pushstring(v,_SC("_charsize_"),-1);
    GS_pushinteger(v,sizeof(GSChar));
    GS_newslot(v,-3, GSFalse);
    GS_pushstring(v,_SC("_intsize_"),-1);
    GS_pushinteger(v,sizeof(GSInteger));
    GS_newslot(v,-3, GSFalse);
    GS_pushstring(v,_SC("_floatsize_"),-1);
    GS_pushinteger(v,sizeof(GSFloat));
    GS_newslot(v,-3, GSFalse);
    GS_pop(v,1);
}

static GSInteger default_delegate_len(HGameScriptVM v)
{
    v->Push(GSInteger(GS_getsize(v,1)));
    return 1;
}

static GSInteger default_delegate_tofloat(HGameScriptVM v)
{
    GSObjectPtr &o=stack_get(v,1);
    switch(GS_type(o)){
    case OT_STRING:{
        GSObjectPtr res;
        if(str2num(_stringval(o),res,10)){
            v->Push(GSObjectPtr(tofloat(res)));
            break;
        }}
        return GS_throwerror(v, _SC("cannot convert the string"));
        break;
    case OT_INTEGER:case OT_FLOAT:
        v->Push(GSObjectPtr(tofloat(o)));
        break;
    case OT_BOOL:
        v->Push(GSObjectPtr((GSFloat)(_integer(o)?1:0)));
        break;
    default:
        v->PushNull();
        break;
    }
    return 1;
}

static GSInteger default_delegate_tointeger(HGameScriptVM v)
{
    GSObjectPtr &o=stack_get(v,1);
    GSInteger base = 10;
    if(GS_gettop(v) > 1) {
        GS_getinteger(v,2,&base);
    }
    switch(GS_type(o)){
    case OT_STRING:{
        GSObjectPtr res;
        if(str2num(_stringval(o),res,base)){
            v->Push(GSObjectPtr(tointeger(res)));
            break;
        }}
        return GS_throwerror(v, _SC("cannot convert the string"));
        break;
    case OT_INTEGER:case OT_FLOAT:
        v->Push(GSObjectPtr(tointeger(o)));
        break;
    case OT_BOOL:
        v->Push(GSObjectPtr(_integer(o)?(GSInteger)1:(GSInteger)0));
        break;
    default:
        v->PushNull();
        break;
    }
    return 1;
}

static GSInteger default_delegate_tostring(HGameScriptVM v)
{
    if(GS_FAILED(GS_tostring(v,1)))
        return GS_ERROR;
    return 1;
}

static GSInteger obj_delegate_weakref(HGameScriptVM v)
{
    GS_weakref(v,1);
    return 1;
}

static GSInteger obj_clear(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_clear(v,-1)) ? 1 : GS_ERROR;
}


static GSInteger number_delegate_tochar(HGameScriptVM v)
{
    GSObject &o=stack_get(v,1);
    GSChar c = (GSChar)tointeger(o);
    v->Push(GSString::Create(_ss(v),(const GSChar *)&c,1));
    return 1;
}



/////////////////////////////////////////////////////////////////
//TABLE DEFAULT DELEGATE

static GSInteger table_rawdelete(HGameScriptVM v)
{
    if(GS_FAILED(GS_rawdeleteslot(v,1,GSTrue)))
        return GS_ERROR;
    return 1;
}


static GSInteger container_rawexists(HGameScriptVM v)
{
    if(GS_SUCCEEDED(GS_rawget(v,-2))) {
        GS_pushbool(v,GSTrue);
        return 1;
    }
    GS_pushbool(v,GSFalse);
    return 1;
}

static GSInteger container_rawset(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_rawset(v,-3)) ? 1 : GS_ERROR;
}


static GSInteger container_rawget(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_rawget(v,-2))?1:GS_ERROR;
}

static GSInteger table_setdelegate(HGameScriptVM v)
{
    if(GS_FAILED(GS_setdelegate(v,-2)))
        return GS_ERROR;
    GS_push(v,-1); // -1 because GS_setdelegate pops 1
    return 1;
}

static GSInteger table_getdelegate(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_getdelegate(v,-1))?1:GS_ERROR;
}

static GSInteger table_filter(HGameScriptVM v)
{
    GSObject &o = stack_get(v,1);
    GSTable *tbl = _table(o);
    GSObjectPtr ret = GSTable::Create(_ss(v),0);

    GSObjectPtr itr, key, val;
    GSInteger nitr;
    while((nitr = tbl->Next(false, itr, key, val)) != -1) {
        itr = (GSInteger)nitr;

        v->Push(o);
        v->Push(key);
        v->Push(val);
        if(GS_FAILED(GS_call(v,3,GSTrue,GSFalse))) {
            return GS_ERROR;
        }
        if(!GSVM::IsFalse(v->GetUp(-1))) {
            _table(ret)->NewSlot(key, val);
        }
        v->Pop();
    }

    v->Push(ret);
    return 1;
}

static GSInteger table_map(HGameScriptVM v)
{
	GSObject &o = stack_get(v, 1);
	GSTable *tbl = _table(o);
	GSInteger nitr, n = 0;
	GSInteger nitems = tbl->CountUsed();
	GSObjectPtr ret = GSArray::Create(_ss(v), nitems);
	GSObjectPtr itr, key, val;
	while ((nitr = tbl->Next(false, itr, key, val)) != -1) {
		itr = (GSInteger)nitr;

		v->Push(o);
		v->Push(key);
		v->Push(val);
		if (GS_FAILED(GS_call(v, 3, GSTrue, GSFalse))) {
			return GS_ERROR;
		}
		_array(ret)->Set(n, v->GetUp(-1));
		v->Pop();
		n++;
	}

	v->Push(ret);
	return 1;
}

#define TABLE_TO_ARRAY_FUNC(_funcname_,_valname_) static GSInteger _funcname_(HGameScriptVM v) \
{ \
	GSObject &o = stack_get(v, 1); \
	GSTable *t = _table(o); \
	GSObjectPtr itr, key, val; \
	GSObjectPtr _null; \
	GSInteger nitr, n = 0; \
	GSInteger nitems = t->CountUsed(); \
	GSArray *a = GSArray::Create(_ss(v), nitems); \
	a->Resize(nitems, _null); \
	if (nitems) { \
		while ((nitr = t->Next(false, itr, key, val)) != -1) { \
			itr = (GSInteger)nitr; \
			a->Set(n, _valname_); \
			n++; \
		} \
	} \
	v->Push(a); \
	return 1; \
}

TABLE_TO_ARRAY_FUNC(table_keys, key)
TABLE_TO_ARRAY_FUNC(table_values, val)


const GSRegFunction GSSharedState::_table_default_delegate_funcz[]={
    {_SC("len"),default_delegate_len,1, _SC("t")},
    {_SC("rawget"),container_rawget,2, _SC("t")},
    {_SC("rawset"),container_rawset,3, _SC("t")},
    {_SC("rawdelete"),table_rawdelete,2, _SC("t")},
    {_SC("rawin"),container_rawexists,2, _SC("t")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {_SC("clear"),obj_clear,1, _SC(".")},
    {_SC("setdelegate"),table_setdelegate,2, _SC(".t|o")},
    {_SC("getdelegate"),table_getdelegate,1, _SC(".")},
    {_SC("filter"),table_filter,2, _SC("tc")},
	{_SC("map"),table_map,2, _SC("tc") },
	{_SC("keys"),table_keys,1, _SC("t") },
	{_SC("values"),table_values,1, _SC("t") },
    {NULL,(GSFUNCTION)0,0,NULL}
};

//ARRAY DEFAULT DELEGATE///////////////////////////////////////

static GSInteger array_append(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_arrayappend(v,-2)) ? 1 : GS_ERROR;
}

static GSInteger array_extend(HGameScriptVM v)
{
    _array(stack_get(v,1))->Extend(_array(stack_get(v,2)));
    GS_pop(v,1);
    return 1;
}

static GSInteger array_reverse(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_arrayreverse(v,-1)) ? 1 : GS_ERROR;
}

static GSInteger array_pop(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_arraypop(v,1,GSTrue))?1:GS_ERROR;
}

static GSInteger array_top(HGameScriptVM v)
{
    GSObject &o=stack_get(v,1);
    if(_array(o)->Size()>0){
        v->Push(_array(o)->Top());
        return 1;
    }
    else return GS_throwerror(v,_SC("top() on a empty array"));
}

static GSInteger array_insert(HGameScriptVM v)
{
    GSObject &o=stack_get(v,1);
    GSObject &idx=stack_get(v,2);
    GSObject &val=stack_get(v,3);
    if(!_array(o)->Insert(tointeger(idx),val))
        return GS_throwerror(v,_SC("index out of range"));
    GS_pop(v,2);
    return 1;
}

static GSInteger array_remove(HGameScriptVM v)
{
    GSObject &o = stack_get(v, 1);
    GSObject &idx = stack_get(v, 2);
    if(!GS_isnumeric(idx)) return GS_throwerror(v, _SC("wrong type"));
    GSObjectPtr val;
    if(_array(o)->Get(tointeger(idx), val)) {
        _array(o)->Remove(tointeger(idx));
        v->Push(val);
        return 1;
    }
    return GS_throwerror(v, _SC("idx out of range"));
}

static GSInteger array_resize(HGameScriptVM v)
{
    GSObject &o = stack_get(v, 1);
    GSObject &nsize = stack_get(v, 2);
    GSObjectPtr fill;
    if(GS_isnumeric(nsize)) {
        GSInteger sz = tointeger(nsize);
        if (sz<0)
          return GS_throwerror(v, _SC("resizing to negative length"));

        if(GS_gettop(v) > 2)
            fill = stack_get(v, 3);
        _array(o)->Resize(sz,fill);
        GS_settop(v, 1);
        return 1;
    }
    return GS_throwerror(v, _SC("size must be a number"));
}

static GSInteger __map_array(GSArray *dest,GSArray *src,HGameScriptVM v) {
    GSObjectPtr temp;
    GSInteger size = src->Size();
    GSObject &closure = stack_get(v, 2);
    v->Push(closure);

    GSInteger nArgs = 0;
    if(GS_type(closure) == OT_CLOSURE) {
        nArgs = _closure(closure)->_function->_nparameters;
    }
    else if (GS_type(closure) == OT_NATIVECLOSURE) {
        GSInteger nParamsCheck = _nativeclosure(closure)->_nparamscheck;
        if (nParamsCheck > 0)
            nArgs = nParamsCheck;
        else // push all params when there is no check or only minimal count set
            nArgs = 4;
    }

    for(GSInteger n = 0; n < size; n++) {
        src->Get(n,temp);
        v->Push(src);
        v->Push(temp);
        if (nArgs >= 3)
            v->Push(GSObjectPtr(n));
        if (nArgs >= 4)
            v->Push(src);
        if(GS_FAILED(GS_call(v,nArgs,GSTrue,GSFalse))) {
            return GS_ERROR;
        }
        dest->Set(n,v->GetUp(-1));
        v->Pop();
    }
    v->Pop();
    return 0;
}

static GSInteger array_map(HGameScriptVM v)
{
    GSObject &o = stack_get(v,1);
    GSInteger size = _array(o)->Size();
    GSObjectPtr ret = GSArray::Create(_ss(v),size);
    if(GS_FAILED(__map_array(_array(ret),_array(o),v)))
        return GS_ERROR;
    v->Push(ret);
    return 1;
}

static GSInteger array_apply(HGameScriptVM v)
{
    GSObject &o = stack_get(v,1);
    if(GS_FAILED(__map_array(_array(o),_array(o),v)))
        return GS_ERROR;
    GS_pop(v,1);
    return 1;
}

static GSInteger array_reduce(HGameScriptVM v)
{
    GSObject &o = stack_get(v,1);
    GSArray *a = _array(o);
    GSInteger size = a->Size();
    GSObjectPtr res;
    GSInteger iterStart;
    if (GS_gettop(v)>2) {
        res = stack_get(v,3);
        iterStart = 0;
    } else if (size==0) {
        return 0;
    } else {
        a->Get(0,res);
        iterStart = 1;
    }
    if (size > iterStart) {
        GSObjectPtr other;
        v->Push(stack_get(v,2));
        for (GSInteger n = iterStart; n < size; n++) {
            a->Get(n,other);
            v->Push(o);
            v->Push(res);
            v->Push(other);
            if(GS_FAILED(GS_call(v,3,GSTrue,GSFalse))) {
                return GS_ERROR;
            }
            res = v->GetUp(-1);
            v->Pop();
        }
        v->Pop();
    }
    v->Push(res);
    return 1;
}

static GSInteger array_filter(HGameScriptVM v)
{
    GSObject &o = stack_get(v,1);
    GSArray *a = _array(o);
    GSObjectPtr ret = GSArray::Create(_ss(v),0);
    GSInteger size = a->Size();
    GSObjectPtr val;
    for(GSInteger n = 0; n < size; n++) {
        a->Get(n,val);
        v->Push(o);
        v->Push(n);
        v->Push(val);
        if(GS_FAILED(GS_call(v,3,GSTrue,GSFalse))) {
            return GS_ERROR;
        }
        if(!GSVM::IsFalse(v->GetUp(-1))) {
            _array(ret)->Append(val);
        }
        v->Pop();
    }
    v->Push(ret);
    return 1;
}

static GSInteger array_find(HGameScriptVM v)
{
    GSObject &o = stack_get(v,1);
    GSObjectPtr &val = stack_get(v,2);
    GSArray *a = _array(o);
    GSInteger size = a->Size();
    GSObjectPtr temp;
    for(GSInteger n = 0; n < size; n++) {
        bool res = false;
        a->Get(n,temp);
        if(GSVM::IsEqual(temp,val,res) && res) {
            v->Push(n);
            return 1;
        }
    }
    return 0;
}


static bool _sort_compare(HGameScriptVM v, GSArray *arr, GSObjectPtr &a,GSObjectPtr &b,GSInteger func,GSInteger &ret)
{
    if(func < 0) {
        if(!v->ObjCmp(a,b,ret)) return false;
    }
    else {
        GSInteger top = GS_gettop(v);
        GS_push(v, func);
        GS_pushroottable(v);
        v->Push(a);
        v->Push(b);
		GSObjectPtr *valptr = arr->_values._vals;
		GSUnsignedInteger precallsize = arr->_values.size();
        if(GS_FAILED(GS_call(v, 3, GSTrue, GSFalse))) {
            if(!GS_isstring( v->_lasterror))
                v->Raise_Error(_SC("compare func failed"));
            return false;
        }
		if(GS_FAILED(GS_getinteger(v, -1, &ret))) {
            v->Raise_Error(_SC("numeric value expected as return value of the compare function"));
            return false;
        }
		if (precallsize != arr->_values.size() || valptr != arr->_values._vals) {
			v->Raise_Error(_SC("array resized during sort operation"));
			return false;
		}
        GS_settop(v, top);
        return true;
    }
    return true;
}

static bool _hsort_sift_down(HGameScriptVM v,GSArray *arr, GSInteger root, GSInteger bottom, GSInteger func)
{
    GSInteger maxChild;
    GSInteger done = 0;
    GSInteger ret;
    GSInteger root2;
    while (((root2 = root * 2) <= bottom) && (!done))
    {
        if (root2 == bottom) {
            maxChild = root2;
        }
        else {
            if(!_sort_compare(v,arr,arr->_values[root2],arr->_values[root2 + 1],func,ret))
                return false;
            if (ret > 0) {
                maxChild = root2;
            }
            else {
                maxChild = root2 + 1;
            }
        }

        if(!_sort_compare(v,arr,arr->_values[root],arr->_values[maxChild],func,ret))
            return false;
        if (ret < 0) {
            if (root == maxChild) {
                v->Raise_Error(_SC("inconsistent compare function"));
                return false; // We'd be swapping ourselve. The compare function is incorrect
            }

            _Swap(arr->_values[root],arr->_values[maxChild]);
            root = maxChild;
        }
        else {
            done = 1;
        }
    }
    return true;
}

static bool _hsort(HGameScriptVM v,GSObjectPtr &arr, GSInteger GS_UNUSED_ARG(l), GSInteger GS_UNUSED_ARG(r),GSInteger func)
{
    GSArray *a = _array(arr);
    GSInteger i;
    GSInteger array_size = a->Size();
    for (i = (array_size / 2); i >= 0; i--) {
        if(!_hsort_sift_down(v,a, i, array_size - 1,func)) return false;
    }

    for (i = array_size-1; i >= 1; i--)
    {
        _Swap(a->_values[0],a->_values[i]);
        if(!_hsort_sift_down(v,a, 0, i-1,func)) return false;
    }
    return true;
}

static GSInteger array_sort(HGameScriptVM v)
{
    GSInteger func = -1;
    GSObjectPtr &o = stack_get(v,1);
    if(_array(o)->Size() > 1) {
        if(GS_gettop(v) == 2) func = 2;
        if(!_hsort(v, o, 0, _array(o)->Size()-1, func))
            return GS_ERROR;

    }
    GS_settop(v,1);
    return 1;
}

static GSInteger array_slice(HGameScriptVM v)
{
    GSInteger sidx,eidx;
    GSObjectPtr o;
    if(get_slice_params(v,sidx,eidx,o)==-1)return -1;
    GSInteger alen = _array(o)->Size();
    if(sidx < 0)sidx = alen + sidx;
    if(eidx < 0)eidx = alen + eidx;
    if(eidx < sidx)return GS_throwerror(v,_SC("wrong indexes"));
    if(eidx > alen || sidx < 0)return GS_throwerror(v, _SC("slice out of range"));
    GSArray *arr=GSArray::Create(_ss(v),eidx-sidx);
    GSObjectPtr t;
    GSInteger count=0;
    for(GSInteger i=sidx;i<eidx;i++){
        _array(o)->Get(i,t);
        arr->Set(count++,t);
    }
    v->Push(arr);
    return 1;

}

const GSRegFunction GSSharedState::_array_default_delegate_funcz[]={
    {_SC("len"),default_delegate_len,1, _SC("a")},
    {_SC("append"),array_append,2, _SC("a")},
    {_SC("extend"),array_extend,2, _SC("aa")},
    {_SC("push"),array_append,2, _SC("a")},
    {_SC("pop"),array_pop,1, _SC("a")},
    {_SC("top"),array_top,1, _SC("a")},
    {_SC("insert"),array_insert,3, _SC("an")},
    {_SC("remove"),array_remove,2, _SC("an")},
    {_SC("resize"),array_resize,-2, _SC("an")},
    {_SC("reverse"),array_reverse,1, _SC("a")},
    {_SC("sort"),array_sort,-1, _SC("ac")},
    {_SC("slice"),array_slice,-1, _SC("ann")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {_SC("clear"),obj_clear,1, _SC(".")},
    {_SC("map"),array_map,2, _SC("ac")},
    {_SC("apply"),array_apply,2, _SC("ac")},
    {_SC("reduce"),array_reduce,-2, _SC("ac.")},
    {_SC("filter"),array_filter,2, _SC("ac")},
    {_SC("find"),array_find,2, _SC("a.")},
    {NULL,(GSFUNCTION)0,0,NULL}
};

//STRING DEFAULT DELEGATE//////////////////////////
static GSInteger string_slice(HGameScriptVM v)
{
    GSInteger sidx,eidx;
    GSObjectPtr o;
    if(GS_FAILED(get_slice_params(v,sidx,eidx,o)))return -1;
    GSInteger slen = _string(o)->_len;
    if(sidx < 0)sidx = slen + sidx;
    if(eidx < 0)eidx = slen + eidx;
    if(eidx < sidx) return GS_throwerror(v,_SC("wrong indexes"));
    if(eidx > slen || sidx < 0) return GS_throwerror(v, _SC("slice out of range"));
    v->Push(GSString::Create(_ss(v),&_stringval(o)[sidx],eidx-sidx));
    return 1;
}

static GSInteger string_find(HGameScriptVM v)
{
    GSInteger top,start_idx=0;
    const GSChar *str,*substr,*ret;
    if(((top=GS_gettop(v))>1) && GS_SUCCEEDED(GS_getstring(v,1,&str)) && GS_SUCCEEDED(GS_getstring(v,2,&substr))){
        if(top>2)GS_getinteger(v,3,&start_idx);
        if((GS_getsize(v,1)>start_idx) && (start_idx>=0)){
            ret=scstrstr(&str[start_idx],substr);
            if(ret){
                GS_pushinteger(v,(GSInteger)(ret-str));
                return 1;
            }
        }
        return 0;
    }
    return GS_throwerror(v,_SC("invalid param"));
}

#define STRING_TOFUNCZ(func) static GSInteger string_##func(HGameScriptVM v) \
{\
    GSInteger sidx,eidx; \
    GSObjectPtr str; \
    if(GS_FAILED(get_slice_params(v,sidx,eidx,str)))return -1; \
    GSInteger slen = _string(str)->_len; \
    if(sidx < 0)sidx = slen + sidx; \
    if(eidx < 0)eidx = slen + eidx; \
    if(eidx < sidx) return GS_throwerror(v,_SC("wrong indexes")); \
    if(eidx > slen || sidx < 0) return GS_throwerror(v,_SC("slice out of range")); \
    GSInteger len=_string(str)->_len; \
    const GSChar *sthis=_stringval(str); \
    GSChar *snew=(_ss(v)->GetScratchPad(GS_rsl(len))); \
    memcpy(snew,sthis,GS_rsl(len));\
    for(GSInteger i=sidx;i<eidx;i++) snew[i] = func(sthis[i]); \
    v->Push(GSString::Create(_ss(v),snew,len)); \
    return 1; \
}


STRING_TOFUNCZ(tolower)
STRING_TOFUNCZ(toupper)

const GSRegFunction GSSharedState::_string_default_delegate_funcz[]={
    {_SC("len"),default_delegate_len,1, _SC("s")},
    {_SC("tointeger"),default_delegate_tointeger,-1, _SC("sn")},
    {_SC("tofloat"),default_delegate_tofloat,1, _SC("s")},
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {_SC("slice"),string_slice,-1, _SC("s n  n")},
    {_SC("find"),string_find,-2, _SC("s s n")},
    {_SC("tolower"),string_tolower,-1, _SC("s n n")},
    {_SC("toupper"),string_toupper,-1, _SC("s n n")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {NULL,(GSFUNCTION)0,0,NULL}
};

//INTEGER DEFAULT DELEGATE//////////////////////////
const GSRegFunction GSSharedState::_number_default_delegate_funcz[]={
    {_SC("tointeger"),default_delegate_tointeger,1, _SC("n|b")},
    {_SC("tofloat"),default_delegate_tofloat,1, _SC("n|b")},
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {_SC("tochar"),number_delegate_tochar,1, _SC("n|b")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {NULL,(GSFUNCTION)0,0,NULL}
};

//CLOSURE DEFAULT DELEGATE//////////////////////////
static GSInteger closure_pcall(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_call(v,GS_gettop(v)-1,GSTrue,GSFalse))?1:GS_ERROR;
}

static GSInteger closure_call(HGameScriptVM v)
{
	GSObjectPtr &c = stack_get(v, -1);
	if (GS_type(c) == OT_CLOSURE && (_closure(c)->_function->_bgenerator == false))
	{
		return GS_tailcall(v, GS_gettop(v) - 1);
	}
	return GS_SUCCEEDED(GS_call(v, GS_gettop(v) - 1, GSTrue, GSTrue)) ? 1 : GS_ERROR;
}

static GSInteger _closure_acall(HGameScriptVM v,GSBool raiseerror)
{
    GSArray *aparams=_array(stack_get(v,2));
    GSInteger nparams=aparams->Size();
    v->Push(stack_get(v,1));
    for(GSInteger i=0;i<nparams;i++)v->Push(aparams->_values[i]);
    return GS_SUCCEEDED(GS_call(v,nparams,GSTrue,raiseerror))?1:GS_ERROR;
}

static GSInteger closure_acall(HGameScriptVM v)
{
    return _closure_acall(v,GSTrue);
}

static GSInteger closure_pacall(HGameScriptVM v)
{
    return _closure_acall(v,GSFalse);
}

static GSInteger closure_bindenv(HGameScriptVM v)
{
    if(GS_FAILED(GS_bindenv(v,1)))
        return GS_ERROR;
    return 1;
}

static GSInteger closure_getroot(HGameScriptVM v)
{
    if(GS_FAILED(GS_getclosureroot(v,-1)))
        return GS_ERROR;
    return 1;
}

static GSInteger closure_setroot(HGameScriptVM v)
{
    if(GS_FAILED(GS_setclosureroot(v,-2)))
        return GS_ERROR;
    return 1;
}

static GSInteger closure_getinfos(HGameScriptVM v) {
    GSObject o = stack_get(v,1);
    GSTable *res = GSTable::Create(_ss(v),4);
    if(GS_type(o) == OT_CLOSURE) {
        GSFunctionProto *f = _closure(o)->_function;
        GSInteger nparams = f->_nparameters + (f->_varparams?1:0);
        GSObjectPtr params = GSArray::Create(_ss(v),nparams);
    GSObjectPtr defparams = GSArray::Create(_ss(v),f->_ndefaultparams);
        for(GSInteger n = 0; n<f->_nparameters; n++) {
            _array(params)->Set((GSInteger)n,f->_parameters[n]);
        }
    for(GSInteger j = 0; j<f->_ndefaultparams; j++) {
            _array(defparams)->Set((GSInteger)j,_closure(o)->_defaultparams[j]);
        }
        if(f->_varparams) {
            _array(params)->Set(nparams-1,GSString::Create(_ss(v),_SC("..."),-1));
        }
        res->NewSlot(GSString::Create(_ss(v),_SC("native"),-1),false);
        res->NewSlot(GSString::Create(_ss(v),_SC("name"),-1),f->_name);
        res->NewSlot(GSString::Create(_ss(v),_SC("src"),-1),f->_sourcename);
        res->NewSlot(GSString::Create(_ss(v),_SC("parameters"),-1),params);
        res->NewSlot(GSString::Create(_ss(v),_SC("varargs"),-1),f->_varparams);
    res->NewSlot(GSString::Create(_ss(v),_SC("defparams"),-1),defparams);
    }
    else { //OT_NATIVECLOSURE
        GSNativeClosure *nc = _nativeclosure(o);
        res->NewSlot(GSString::Create(_ss(v),_SC("native"),-1),true);
        res->NewSlot(GSString::Create(_ss(v),_SC("name"),-1),nc->_name);
        res->NewSlot(GSString::Create(_ss(v),_SC("paramscheck"),-1),nc->_nparamscheck);
        GSObjectPtr typecheck;
        if(nc->_typecheck.size() > 0) {
            typecheck =
                GSArray::Create(_ss(v), nc->_typecheck.size());
            for(GSUnsignedInteger n = 0; n<nc->_typecheck.size(); n++) {
                    _array(typecheck)->Set((GSInteger)n,nc->_typecheck[n]);
            }
        }
        res->NewSlot(GSString::Create(_ss(v),_SC("typecheck"),-1),typecheck);
    }
    v->Push(res);
    return 1;
}



const GSRegFunction GSSharedState::_closure_default_delegate_funcz[]={
    {_SC("call"),closure_call,-1, _SC("c")},
    {_SC("pcall"),closure_pcall,-1, _SC("c")},
    {_SC("acall"),closure_acall,2, _SC("ca")},
    {_SC("pacall"),closure_pacall,2, _SC("ca")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {_SC("bindenv"),closure_bindenv,2, _SC("c x|y|t")},
    {_SC("getinfos"),closure_getinfos,1, _SC("c")},
    {_SC("getroot"),closure_getroot,1, _SC("c")},
    {_SC("setroot"),closure_setroot,2, _SC("ct")},
    {NULL,(GSFUNCTION)0,0,NULL}
};

//GENERATOR DEFAULT DELEGATE
static GSInteger generator_getstatus(HGameScriptVM v)
{
    GSObject &o=stack_get(v,1);
    switch(_generator(o)->_state){
        case GSGenerator::eSuspended:v->Push(GSString::Create(_ss(v),_SC("suspended")));break;
        case GSGenerator::eRunning:v->Push(GSString::Create(_ss(v),_SC("running")));break;
        case GSGenerator::eDead:v->Push(GSString::Create(_ss(v),_SC("dead")));break;
    }
    return 1;
}

const GSRegFunction GSSharedState::_generator_default_delegate_funcz[]={
    {_SC("getstatus"),generator_getstatus,1, _SC("g")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {NULL,(GSFUNCTION)0,0,NULL}
};

//THREAD DEFAULT DELEGATE
static GSInteger thread_call(HGameScriptVM v)
{
    GSObjectPtr o = stack_get(v,1);
    if(GS_type(o) == OT_THREAD) {
        GSInteger nparams = GS_gettop(v);
        GS_reservestack(_thread(o), nparams + 3);
        _thread(o)->Push(_thread(o)->_roottable);
        for(GSInteger i = 2; i<(nparams+1); i++)
            GS_move(_thread(o),v,i);
        if(GS_SUCCEEDED(GS_call(_thread(o),nparams,GSTrue,GSTrue))) {
            GS_move(v,_thread(o),-1);
            GS_pop(_thread(o),1);
            return 1;
        }
        v->_lasterror = _thread(o)->_lasterror;
        return GS_ERROR;
    }
    return GS_throwerror(v,_SC("wrong parameter"));
}

static GSInteger thread_wakeup(HGameScriptVM v)
{
    GSObjectPtr o = stack_get(v,1);
    if(GS_type(o) == OT_THREAD) {
        GSVM *thread = _thread(o);
        GSInteger state = GS_getvmstate(thread);
        if(state != GS_VMSTATE_SUSPENDED) {
            switch(state) {
                case GS_VMSTATE_IDLE:
                    return GS_throwerror(v,_SC("cannot wakeup a idle thread"));
                break;
                case GS_VMSTATE_RUNNING:
                    return GS_throwerror(v,_SC("cannot wakeup a running thread"));
                break;
            }
        }

        GSInteger wakeupret = GS_gettop(v)>1?GSTrue:GSFalse;
        if(wakeupret) {
            GS_move(thread,v,2);
        }
        if(GS_SUCCEEDED(GS_wakeupvm(thread,wakeupret,GSTrue,GSTrue,GSFalse))) {
            GS_move(v,thread,-1);
            GS_pop(thread,1); //pop retval
            if(GS_getvmstate(thread) == GS_VMSTATE_IDLE) {
                GS_settop(thread,1); //pop roottable
            }
            return 1;
        }
        GS_settop(thread,1);
        v->_lasterror = thread->_lasterror;
        return GS_ERROR;
    }
    return GS_throwerror(v,_SC("wrong parameter"));
}

static GSInteger thread_wakeupthrow(HGameScriptVM v)
{
    GSObjectPtr o = stack_get(v,1);
    if(GS_type(o) == OT_THREAD) {
        GSVM *thread = _thread(o);
        GSInteger state = GS_getvmstate(thread);
        if(state != GS_VMSTATE_SUSPENDED) {
            switch(state) {
                case GS_VMSTATE_IDLE:
                    return GS_throwerror(v,_SC("cannot wakeup a idle thread"));
                break;
                case GS_VMSTATE_RUNNING:
                    return GS_throwerror(v,_SC("cannot wakeup a running thread"));
                break;
            }
        }

        GS_move(thread,v,2);
        GS_throwobject(thread);
        GSBool rethrow_error = GSTrue;
        if(GS_gettop(v) > 2) {
            GS_getbool(v,3,&rethrow_error);
        }
        if(GS_SUCCEEDED(GS_wakeupvm(thread,GSFalse,GSTrue,GSTrue,GSTrue))) {
            GS_move(v,thread,-1);
            GS_pop(thread,1); //pop retval
            if(GS_getvmstate(thread) == GS_VMSTATE_IDLE) {
                GS_settop(thread,1); //pop roottable
            }
            return 1;
        }
        GS_settop(thread,1);
        if(rethrow_error) {
            v->_lasterror = thread->_lasterror;
            return GS_ERROR;
        }
        return GS_OK;
    }
    return GS_throwerror(v,_SC("wrong parameter"));
}

static GSInteger thread_getstatus(HGameScriptVM v)
{
    GSObjectPtr &o = stack_get(v,1);
    switch(GS_getvmstate(_thread(o))) {
        case GS_VMSTATE_IDLE:
            GS_pushstring(v,_SC("idle"),-1);
        break;
        case GS_VMSTATE_RUNNING:
            GS_pushstring(v,_SC("running"),-1);
        break;
        case GS_VMSTATE_SUSPENDED:
            GS_pushstring(v,_SC("suspended"),-1);
        break;
        default:
            return GS_throwerror(v,_SC("internal VM error"));
    }
    return 1;
}

static GSInteger thread_getstackinfos(HGameScriptVM v)
{
    GSObjectPtr o = stack_get(v,1);
    if(GS_type(o) == OT_THREAD) {
        GSVM *thread = _thread(o);
        GSInteger threadtop = GS_gettop(thread);
        GSInteger level;
        GS_getinteger(v,-1,&level);
        GSRESULT res = __getcallstackinfos(thread,level);
        if(GS_FAILED(res))
        {
            GS_settop(thread,threadtop);
            if(GS_type(thread->_lasterror) == OT_STRING) {
                GS_throwerror(v,_stringval(thread->_lasterror));
            }
            else {
                GS_throwerror(v,_SC("unknown error"));
            }
        }
        if(res > 0) {
            //some result
            GS_move(v,thread,-1);
            GS_settop(thread,threadtop);
            return 1;
        }
        //no result
        GS_settop(thread,threadtop);
        return 0;

    }
    return GS_throwerror(v,_SC("wrong parameter"));
}

const GSRegFunction GSSharedState::_thread_default_delegate_funcz[] = {
    {_SC("call"), thread_call, -1, _SC("v")},
    {_SC("wakeup"), thread_wakeup, -1, _SC("v")},
    {_SC("wakeupthrow"), thread_wakeupthrow, -2, _SC("v.b")},
    {_SC("getstatus"), thread_getstatus, 1, _SC("v")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("getstackinfos"),thread_getstackinfos,2, _SC("vn")},
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {NULL,(GSFUNCTION)0,0,NULL}
};

static GSInteger class_getattributes(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_getattributes(v,-2))?1:GS_ERROR;
}

static GSInteger class_setattributes(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_setattributes(v,-3))?1:GS_ERROR;
}

static GSInteger class_instance(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_createinstance(v,-1))?1:GS_ERROR;
}

static GSInteger class_getbase(HGameScriptVM v)
{
    return GS_SUCCEEDED(GS_getbase(v,-1))?1:GS_ERROR;
}

static GSInteger class_newmember(HGameScriptVM v)
{
    GSInteger top = GS_gettop(v);
    GSBool bstatic = GSFalse;
    if(top == 5)
    {
        GS_tobool(v,-1,&bstatic);
        GS_pop(v,1);
    }

    if(top < 4) {
        GS_pushnull(v);
    }
    return GS_SUCCEEDED(GS_newmember(v,-4,bstatic))?1:GS_ERROR;
}

static GSInteger class_rawnewmember(HGameScriptVM v)
{
    GSInteger top = GS_gettop(v);
    GSBool bstatic = GSFalse;
    if(top == 5)
    {
        GS_tobool(v,-1,&bstatic);
        GS_pop(v,1);
    }

    if(top < 4) {
        GS_pushnull(v);
    }
    return GS_SUCCEEDED(GS_rawnewmember(v,-4,bstatic))?1:GS_ERROR;
}

const GSRegFunction GSSharedState::_class_default_delegate_funcz[] = {
    {_SC("getattributes"), class_getattributes, 2, _SC("y.")},
    {_SC("setattributes"), class_setattributes, 3, _SC("y..")},
    {_SC("rawget"),container_rawget,2, _SC("y")},
    {_SC("rawset"),container_rawset,3, _SC("y")},
    {_SC("rawin"),container_rawexists,2, _SC("y")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {_SC("instance"),class_instance,1, _SC("y")},
    {_SC("getbase"),class_getbase,1, _SC("y")},
    {_SC("newmember"),class_newmember,-3, _SC("y")},
    {_SC("rawnewmember"),class_rawnewmember,-3, _SC("y")},
    {NULL,(GSFUNCTION)0,0,NULL}
};


static GSInteger instance_getclass(HGameScriptVM v)
{
    if(GS_SUCCEEDED(GS_getclass(v,1)))
        return 1;
    return GS_ERROR;
}

const GSRegFunction GSSharedState::_instance_default_delegate_funcz[] = {
    {_SC("getclass"), instance_getclass, 1, _SC("x")},
    {_SC("rawget"),container_rawget,2, _SC("x")},
    {_SC("rawset"),container_rawset,3, _SC("x")},
    {_SC("rawin"),container_rawexists,2, _SC("x")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {NULL,(GSFUNCTION)0,0,NULL}
};

static GSInteger weakref_ref(HGameScriptVM v)
{
    if(GS_FAILED(GS_getweakrefval(v,1)))
        return GS_ERROR;
    return 1;
}

const GSRegFunction GSSharedState::_weakref_default_delegate_funcz[] = {
    {_SC("ref"),weakref_ref,1, _SC("r")},
    {_SC("weakref"),obj_delegate_weakref,1, NULL },
    {_SC("tostring"),default_delegate_tostring,1, _SC(".")},
    {NULL,(GSFUNCTION)0,0,NULL}
};

