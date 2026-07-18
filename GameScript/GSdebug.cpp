/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#include <stdarg.h>
#include "GSvm.h"
#include "GSfuncproto.h"
#include "GSclosure.h"
#include "GSstring.h"

GSRESULT GS_getfunctioninfo(HGameScriptVM v,GSInteger level,GSFunctionInfo *fi)
{
    GSInteger cssize = v->_callsstacksize;
    if (cssize > level) {
        GSVM::CallInfo &ci = v->_callsstack[cssize-level-1];
        if(GS_isclosure(ci._closure)) {
            GSClosure *c = _closure(ci._closure);
            GSFunctionProto *proto = c->_function;
            fi->funcid = proto;
            fi->name = GS_type(proto->_name) == OT_STRING?_stringval(proto->_name):_SC("unknown");
            fi->source = GS_type(proto->_sourcename) == OT_STRING?_stringval(proto->_sourcename):_SC("unknown");
            fi->line = proto->_lineinfos[0]._line;
            return GS_OK;
        }
    }
    return GS_throwerror(v,_SC("the object is not a closure"));
}

GSRESULT GS_stackinfos(HGameScriptVM v, GSInteger level, GSStackInfos *si)
{
    GSInteger cssize = v->_callsstacksize;
    if (cssize > level) {
        memset(si, 0, sizeof(GSStackInfos));
        GSVM::CallInfo &ci = v->_callsstack[cssize-level-1];
        switch (GS_type(ci._closure)) {
        case OT_CLOSURE:{
            GSFunctionProto *func = _closure(ci._closure)->_function;
            if (GS_type(func->_name) == OT_STRING)
                si->funcname = _stringval(func->_name);
            if (GS_type(func->_sourcename) == OT_STRING)
                si->source = _stringval(func->_sourcename);
            si->line = func->GetLine(ci._ip);
                        }
            break;
        case OT_NATIVECLOSURE:
            si->source = _SC("NATIVE");
            si->funcname = _SC("unknown");
            if(GS_type(_nativeclosure(ci._closure)->_name) == OT_STRING)
                si->funcname = _stringval(_nativeclosure(ci._closure)->_name);
            si->line = -1;
            break;
        default: break; //shutup compiler
        }
        return GS_OK;
    }
    return GS_ERROR;
}

void GSVM::Raise_Error(const GSChar *s, ...)
{
    va_list vl;
    va_start(vl, s);
    GSInteger buffersize = (GSInteger)scstrlen(s)+(NUMBER_MAX_CHAR*2);
    scvsprintf(_sp(GS_rsl(buffersize)),buffersize, s, vl);
    va_end(vl);
    _lasterror = GSString::Create(_ss(this),_spval,-1);
}

void GSVM::Raise_Error(const GSObjectPtr &desc)
{
    _lasterror = desc;
}

GSString *GSVM::PrintObjVal(const GSObjectPtr &o)
{
    switch(GS_type(o)) {
    case OT_STRING: return _string(o);
    case OT_INTEGER:
        scsprintf(_sp(GS_rsl(NUMBER_MAX_CHAR+1)),GS_rsl(NUMBER_MAX_CHAR), _PRINT_INT_FMT, _integer(o));
        return GSString::Create(_ss(this), _spval);
        break;
    case OT_FLOAT:
        scsprintf(_sp(GS_rsl(NUMBER_MAX_CHAR+1)), GS_rsl(NUMBER_MAX_CHAR), _SC("%.14g"), _float(o));
        return GSString::Create(_ss(this), _spval);
        break;
    default:
        return GSString::Create(_ss(this), GetTypeName(o));
    }
}

void GSVM::Raise_IdxError(const GSObjectPtr &o)
{
    GSObjectPtr oval = PrintObjVal(o);
    Raise_Error(_SC("the index '%.50s' does not exist"), _stringval(oval));
}

void GSVM::Raise_CompareError(const GSObject &o1, const GSObject &o2)
{
    GSObjectPtr oval1 = PrintObjVal(o1), oval2 = PrintObjVal(o2);
    Raise_Error(_SC("comparison between '%.50s' and '%.50s'"), _stringval(oval1), _stringval(oval2));
}


void GSVM::Raise_ParamTypeError(GSInteger nparam,GSInteger typemask,GSInteger type)
{
    GSObjectPtr exptypes = GSString::Create(_ss(this), _SC(""), -1);
    GSInteger found = 0;
    for(GSInteger i=0; i<16; i++)
    {
        GSInteger mask = ((GSInteger)1) << i;
        if(typemask & (mask)) {
            if(found>0) StringCat(exptypes,GSString::Create(_ss(this), _SC("|"), -1), exptypes);
            found ++;
            StringCat(exptypes,GSString::Create(_ss(this), IdType2Name((GSObjectType)mask), -1), exptypes);
        }
    }
    Raise_Error(_SC("parameter %d has an invalid type '%s' ; expected: '%s'"), nparam, IdType2Name((GSObjectType)type), _stringval(exptypes));
}

