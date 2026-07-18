/* see copyright notice in GameScript.h */
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GameScript.h>
#include <gstdio.h>
#include <gstdblob.h>
#include "gstdstream.h"
#include "gstdblobimpl.h"

#define SETUP_STREAM(v) \
    GSStream *self = NULL; \
    if(GS_FAILED(GS_getinstanceup(v,1,(GSUserPointer*)&self,(GSUserPointer)((GSUnsignedInteger)GSSTD_STREAM_TYPE_TAG),GSFalse))) \
        return GS_throwerror(v,_SC("invalid type tag")); \
    if(!self || !self->IsValid())  \
        return GS_throwerror(v,_SC("the stream is invalid"));

GSInteger _stream_readblob(HGameScriptVM v)
{
    SETUP_STREAM(v);
    GSUserPointer data,blobp;
    GSInteger size,res;
    GS_getinteger(v,2,&size);
    if(size > self->Len()) {
        size = self->Len();
    }
    data = GS_getscratchpad(v,size);
    res = self->Read(data,size);
    if(res <= 0)
        return GS_throwerror(v,_SC("no data left to read"));
    blobp = gstd_createblob(v,res);
    memcpy(blobp,data,res);
    return 1;
}

#define SAFE_READN(ptr,len) { \
    if(self->Read(ptr,len) != len) return GS_throwerror(v,_SC("io error")); \
    }
GSInteger _stream_readn(HGameScriptVM v)
{
    SETUP_STREAM(v);
    GSInteger format;
    GS_getinteger(v, 2, &format);
    switch(format) {
    case 'l': {
        GSInteger i;
        SAFE_READN(&i, sizeof(i));
        GS_pushinteger(v, i);
              }
        break;
    case 'i': {
        GSInt32 i;
        SAFE_READN(&i, sizeof(i));
        GS_pushinteger(v, i);
              }
        break;
    case 's': {
        short s;
        SAFE_READN(&s, sizeof(short));
        GS_pushinteger(v, s);
              }
        break;
    case 'w': {
        unsigned short w;
        SAFE_READN(&w, sizeof(unsigned short));
        GS_pushinteger(v, w);
              }
        break;
    case 'c': {
        char c;
        SAFE_READN(&c, sizeof(char));
        GS_pushinteger(v, c);
              }
        break;
    case 'b': {
        unsigned char c;
        SAFE_READN(&c, sizeof(unsigned char));
        GS_pushinteger(v, c);
              }
        break;
    case 'f': {
        float f;
        SAFE_READN(&f, sizeof(float));
        GS_pushfloat(v, f);
              }
        break;
    case 'd': {
        double d;
        SAFE_READN(&d, sizeof(double));
        GS_pushfloat(v, (GSFloat)d);
              }
        break;
    default:
        return GS_throwerror(v, _SC("invalid format"));
    }
    return 1;
}

GSInteger _stream_writeblob(HGameScriptVM v)
{
    GSUserPointer data;
    GSInteger size;
    SETUP_STREAM(v);
    if(GS_FAILED(gstd_getblob(v,2,&data)))
        return GS_throwerror(v,_SC("invalid parameter"));
    size = gstd_getblobsize(v,2);
    if(self->Write(data,size) != size)
        return GS_throwerror(v,_SC("io error"));
    GS_pushinteger(v,size);
    return 1;
}

GSInteger _stream_writen(HGameScriptVM v)
{
    SETUP_STREAM(v);
    GSInteger format, ti;
    GSFloat tf;
    GS_getinteger(v, 3, &format);
    switch(format) {
    case 'l': {
        GSInteger i;
        GS_getinteger(v, 2, &ti);
        i = ti;
        self->Write(&i, sizeof(GSInteger));
              }
        break;
    case 'i': {
        GSInt32 i;
        GS_getinteger(v, 2, &ti);
        i = (GSInt32)ti;
        self->Write(&i, sizeof(GSInt32));
              }
        break;
    case 's': {
        short s;
        GS_getinteger(v, 2, &ti);
        s = (short)ti;
        self->Write(&s, sizeof(short));
              }
        break;
    case 'w': {
        unsigned short w;
        GS_getinteger(v, 2, &ti);
        w = (unsigned short)ti;
        self->Write(&w, sizeof(unsigned short));
              }
        break;
    case 'c': {
        char c;
        GS_getinteger(v, 2, &ti);
        c = (char)ti;
        self->Write(&c, sizeof(char));
                  }
        break;
    case 'b': {
        unsigned char b;
        GS_getinteger(v, 2, &ti);
        b = (unsigned char)ti;
        self->Write(&b, sizeof(unsigned char));
              }
        break;
    case 'f': {
        float f;
        GS_getfloat(v, 2, &tf);
        f = (float)tf;
        self->Write(&f, sizeof(float));
              }
        break;
    case 'd': {
        double d;
        GS_getfloat(v, 2, &tf);
        d = tf;
        self->Write(&d, sizeof(double));
              }
        break;
    default:
        return GS_throwerror(v, _SC("invalid format"));
    }
    return 0;
}

GSInteger _stream_seek(HGameScriptVM v)
{
    SETUP_STREAM(v);
    GSInteger offset, origin = GS_SEEK_SET;
    GS_getinteger(v, 2, &offset);
    if(GS_gettop(v) > 2) {
        GSInteger t;
        GS_getinteger(v, 3, &t);
        switch(t) {
            case 'b': origin = GS_SEEK_SET; break;
            case 'c': origin = GS_SEEK_CUR; break;
            case 'e': origin = GS_SEEK_END; break;
            default: return GS_throwerror(v,_SC("invalid origin"));
        }
    }
    GS_pushinteger(v, self->Seek(offset, origin));
    return 1;
}

GSInteger _stream_tell(HGameScriptVM v)
{
    SETUP_STREAM(v);
    GS_pushinteger(v, self->Tell());
    return 1;
}

GSInteger _stream_len(HGameScriptVM v)
{
    SETUP_STREAM(v);
    GS_pushinteger(v, self->Len());
    return 1;
}

GSInteger _stream_flush(HGameScriptVM v)
{
    SETUP_STREAM(v);
    if(!self->Flush())
        GS_pushinteger(v, 1);
    else
        GS_pushnull(v);
    return 1;
}

GSInteger _stream_eos(HGameScriptVM v)
{
    SETUP_STREAM(v);
    if(self->EOS())
        GS_pushinteger(v, 1);
    else
        GS_pushnull(v);
    return 1;
}

 GSInteger _stream__cloned(HGameScriptVM v)
 {
     return GS_throwerror(v,_SC("this object cannot be cloned"));
 }

static const GSRegFunction _stream_methods[] = {
    _DECL_STREAM_FUNC(readblob,2,_SC("xn")),
    _DECL_STREAM_FUNC(readn,2,_SC("xn")),
    _DECL_STREAM_FUNC(writeblob,-2,_SC("xx")),
    _DECL_STREAM_FUNC(writen,3,_SC("xnn")),
    _DECL_STREAM_FUNC(seek,-2,_SC("xnn")),
    _DECL_STREAM_FUNC(tell,1,_SC("x")),
    _DECL_STREAM_FUNC(len,1,_SC("x")),
    _DECL_STREAM_FUNC(eos,1,_SC("x")),
    _DECL_STREAM_FUNC(flush,1,_SC("x")),
    _DECL_STREAM_FUNC(_cloned,0,NULL),
    {NULL,(GSFUNCTION)0,0,NULL}
};

void init_streamclass(HGameScriptVM v)
{
    GS_pushregistrytable(v);
    GS_pushstring(v,_SC("std_stream"),-1);
    if(GS_FAILED(GS_get(v,-2))) {
        GS_pushstring(v,_SC("std_stream"),-1);
        GS_newclass(v,GSFalse);
        GS_settypetag(v,-1,(GSUserPointer)((GSUnsignedInteger)GSSTD_STREAM_TYPE_TAG));
        GSInteger i = 0;
        while(_stream_methods[i].name != 0) {
            const GSRegFunction &f = _stream_methods[i];
            GS_pushstring(v,f.name,-1);
            GS_newclosure(v,f.f,0);
            GS_setparamscheck(v,f.nparamscheck,f.typemask);
            GS_newslot(v,-3,GSFalse);
            i++;
        }
        GS_newslot(v,-3,GSFalse);
        GS_pushroottable(v);
        GS_pushstring(v,_SC("stream"),-1);
        GS_pushstring(v,_SC("std_stream"),-1);
        GS_get(v,-4);
        GS_newslot(v,-3,GSFalse);
        GS_pop(v,1);
    }
    else {
        GS_pop(v,1); //result
    }
    GS_pop(v,1);
}

GSRESULT declare_stream(HGameScriptVM v,const GSChar* name,GSUserPointer typetag,const GSChar* reg_name,const GSRegFunction *methods,const GSRegFunction *globals)
{
    if(GS_gettype(v,-1) != OT_TABLE)
        return GS_throwerror(v,_SC("table expected"));
    GSInteger top = GS_gettop(v);
    //create delegate
    init_streamclass(v);
    GS_pushregistrytable(v);
    GS_pushstring(v,reg_name,-1);
    GS_pushstring(v,_SC("std_stream"),-1);
    if(GS_SUCCEEDED(GS_get(v,-3))) {
        GS_newclass(v,GSTrue);
        GS_settypetag(v,-1,typetag);
        GSInteger i = 0;
        while(methods[i].name != 0) {
            const GSRegFunction &f = methods[i];
            GS_pushstring(v,f.name,-1);
            GS_newclosure(v,f.f,0);
            GS_setparamscheck(v,f.nparamscheck,f.typemask);
            GS_setnativeclosurename(v,-1,f.name);
            GS_newslot(v,-3,GSFalse);
            i++;
        }
        GS_newslot(v,-3,GSFalse);
        GS_pop(v,1);

        i = 0;
        while(globals[i].name!=0)
        {
            const GSRegFunction &f = globals[i];
            GS_pushstring(v,f.name,-1);
            GS_newclosure(v,f.f,0);
            GS_setparamscheck(v,f.nparamscheck,f.typemask);
            GS_setnativeclosurename(v,-1,f.name);
            GS_newslot(v,-3,GSFalse);
            i++;
        }
        //register the class in the target table
        GS_pushstring(v,name,-1);
        GS_pushregistrytable(v);
        GS_pushstring(v,reg_name,-1);
        GS_get(v,-2);
        GS_remove(v,-2);
        GS_newslot(v,-3,GSFalse);

        GS_settop(v,top);
        return GS_OK;
    }
    GS_settop(v,top);
    return GS_ERROR;
}

