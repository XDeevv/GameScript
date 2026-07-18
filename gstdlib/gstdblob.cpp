/* see copyright notice in GameScript.h */
#include <new>
#include <GameScript.h>
#include <gstdio.h>
#include <string.h>
#include <gstdblob.h>
#include "gstdstream.h"
#include "gstdblobimpl.h"

#define GSSTD_BLOB_TYPE_TAG ((GSUnsignedInteger)(GSSTD_STREAM_TYPE_TAG | 0x00000002))

//Blob


#define SETUP_BLOB(v) \
    GSBlob *self = NULL; \
    { if(GS_FAILED(GS_getinstanceup(v,1,(GSUserPointer*)&self,(GSUserPointer)GSSTD_BLOB_TYPE_TAG,GSFalse))) \
        return GS_throwerror(v,_SC("invalid type tag"));  } \
    if(!self || !self->IsValid())  \
        return GS_throwerror(v,_SC("the blob is invalid"));


static GSInteger _blob_resize(HGameScriptVM v)
{
    SETUP_BLOB(v);
    GSInteger size;
    GS_getinteger(v,2,&size);
    if(!self->Resize(size))
        return GS_throwerror(v,_SC("resize failed"));
    return 0;
}

static void __swap_dword(unsigned int *n)
{
    *n=(unsigned int)(((*n&0xFF000000)>>24)  |
            ((*n&0x00FF0000)>>8)  |
            ((*n&0x0000FF00)<<8)  |
            ((*n&0x000000FF)<<24));
}

static void __swap_word(unsigned short *n)
{
    *n=(unsigned short)((*n>>8)&0x00FF)| ((*n<<8)&0xFF00);
}

static GSInteger _blob_swap4(HGameScriptVM v)
{
    SETUP_BLOB(v);
    GSInteger num=(self->Len()-(self->Len()%4))>>2;
    unsigned int *t=(unsigned int *)self->GetBuf();
    for(GSInteger i = 0; i < num; i++) {
        __swap_dword(&t[i]);
    }
    return 0;
}

static GSInteger _blob_swap2(HGameScriptVM v)
{
    SETUP_BLOB(v);
    GSInteger num=(self->Len()-(self->Len()%2))>>1;
    unsigned short *t = (unsigned short *)self->GetBuf();
    for(GSInteger i = 0; i < num; i++) {
        __swap_word(&t[i]);
    }
    return 0;
}

static GSInteger _blob__set(HGameScriptVM v)
{
    SETUP_BLOB(v);
    GSInteger idx,val;
    GS_getinteger(v,2,&idx);
    GS_getinteger(v,3,&val);
    if(idx < 0 || idx >= self->Len())
        return GS_throwerror(v,_SC("index out of range"));
    ((unsigned char *)self->GetBuf())[idx] = (unsigned char) val;
    GS_push(v,3);
    return 1;
}

static GSInteger _blob__get(HGameScriptVM v)
{
    SETUP_BLOB(v);
    GSInteger idx;
	
	if ((GS_gettype(v, 2) & GSOBJECT_NUMERIC) == 0)
	{
		GS_pushnull(v);
		return GS_throwobject(v);
	}
    GS_getinteger(v,2,&idx);
    if(idx < 0 || idx >= self->Len())
        return GS_throwerror(v,_SC("index out of range"));
    GS_pushinteger(v,((unsigned char *)self->GetBuf())[idx]);
    return 1;
}

static GSInteger _blob__nexti(HGameScriptVM v)
{
    SETUP_BLOB(v);
    if(GS_gettype(v,2) == OT_NULL) {
        GS_pushinteger(v, 0);
        return 1;
    }
    GSInteger idx;
    if(GS_SUCCEEDED(GS_getinteger(v, 2, &idx))) {
        if(idx+1 < self->Len()) {
            GS_pushinteger(v, idx+1);
            return 1;
        }
        GS_pushnull(v);
        return 1;
    }
    return GS_throwerror(v,_SC("internal error (_nexti) wrong argument type"));
}

static GSInteger _blob__typeof(HGameScriptVM v)
{
    GS_pushstring(v,_SC("blob"),-1);
    return 1;
}

static GSInteger _blob_releasehook(GSUserPointer p, GSInteger GS_UNUSED_ARG(size))
{
    GSBlob *self = (GSBlob*)p;
    self->~GSBlob();
    GS_free(self,sizeof(GSBlob));
    return 1;
}

static GSInteger _blob_constructor(HGameScriptVM v)
{
    GSInteger nparam = GS_gettop(v);
    GSInteger size = 0;
    if(nparam == 2) {
        GS_getinteger(v, 2, &size);
    }
    if(size < 0) return GS_throwerror(v, _SC("cannot create blob with negative size"));
    //GSBlob *b = new GSBlob(size);

    GSBlob *b = new (GS_malloc(sizeof(GSBlob)))GSBlob(size);
    if(GS_FAILED(GS_setinstanceup(v,1,b))) {
        b->~GSBlob();
        GS_free(b,sizeof(GSBlob));
        return GS_throwerror(v, _SC("cannot create blob"));
    }
    GS_setreleasehook(v,1,_blob_releasehook);
    return 0;
}

static GSInteger _blob__cloned(HGameScriptVM v)
{
    GSBlob *other = NULL;
    {
        if(GS_FAILED(GS_getinstanceup(v,2,(GSUserPointer*)&other,(GSUserPointer)GSSTD_BLOB_TYPE_TAG,GSFalse)))
            return GS_ERROR;
    }
    //GSBlob *thisone = new GSBlob(other->Len());
    GSBlob *thisone = new (GS_malloc(sizeof(GSBlob)))GSBlob(other->Len());
    memcpy(thisone->GetBuf(),other->GetBuf(),thisone->Len());
    if(GS_FAILED(GS_setinstanceup(v,1,thisone))) {
        thisone->~GSBlob();
        GS_free(thisone,sizeof(GSBlob));
        return GS_throwerror(v, _SC("cannot clone blob"));
    }
    GS_setreleasehook(v,1,_blob_releasehook);
    return 0;
}

#define _DECL_BLOB_FUNC(name,nparams,typecheck) {_SC(#name),_blob_##name,nparams,typecheck}
static const GSRegFunction _blob_methods[] = {
    _DECL_BLOB_FUNC(constructor,-1,_SC("xn")),
    _DECL_BLOB_FUNC(resize,2,_SC("xn")),
    _DECL_BLOB_FUNC(swap2,1,_SC("x")),
    _DECL_BLOB_FUNC(swap4,1,_SC("x")),
    _DECL_BLOB_FUNC(_set,3,_SC("xnn")),
    _DECL_BLOB_FUNC(_get,2,_SC("x.")),
    _DECL_BLOB_FUNC(_typeof,1,_SC("x")),
    _DECL_BLOB_FUNC(_nexti,2,_SC("x")),
    _DECL_BLOB_FUNC(_cloned,2,_SC("xx")),
    {NULL,(GSFUNCTION)0,0,NULL}
};



//GLOBAL FUNCTIONS

static GSInteger _g_blob_casti2f(HGameScriptVM v)
{
    GSInteger i;
    GS_getinteger(v,2,&i);
    GS_pushfloat(v,*((const GSFloat *)&i));
    return 1;
}

static GSInteger _g_blob_castf2i(HGameScriptVM v)
{
    GSFloat f;
    GS_getfloat(v,2,&f);
    GS_pushinteger(v,*((const GSInteger *)&f));
    return 1;
}

static GSInteger _g_blob_swap2(HGameScriptVM v)
{
    GSInteger i;
    GS_getinteger(v,2,&i);
    unsigned short s = (unsigned short)i;
    GS_pushinteger(v, ((s << 8) | ((s >> 8) & 0x00FFu)) & 0xFFFFu);
    return 1;
}

static GSInteger _g_blob_swap4(HGameScriptVM v)
{
    GSInteger i;
    GS_getinteger(v,2,&i);
    unsigned int t4 = (unsigned int)i;
    __swap_dword(&t4);
    GS_pushinteger(v,(GSInteger)t4);
    return 1;
}

static GSInteger _g_blob_swapfloat(HGameScriptVM v)
{
    GSFloat f;
    GS_getfloat(v,2,&f);
    __swap_dword((unsigned int *)&f);
    GS_pushfloat(v,f);
    return 1;
}

#define _DECL_GLOBALBLOB_FUNC(name,nparams,typecheck) {_SC(#name),_g_blob_##name,nparams,typecheck}
static const GSRegFunction bloblib_funcs[]={
    _DECL_GLOBALBLOB_FUNC(casti2f,2,_SC(".n")),
    _DECL_GLOBALBLOB_FUNC(castf2i,2,_SC(".n")),
    _DECL_GLOBALBLOB_FUNC(swap2,2,_SC(".n")),
    _DECL_GLOBALBLOB_FUNC(swap4,2,_SC(".n")),
    _DECL_GLOBALBLOB_FUNC(swapfloat,2,_SC(".n")),
    {NULL,(GSFUNCTION)0,0,NULL}
};

GSRESULT gstd_getblob(HGameScriptVM v,GSInteger idx,GSUserPointer *ptr)
{
    GSBlob *blob;
    if(GS_FAILED(GS_getinstanceup(v,idx,(GSUserPointer *)&blob,(GSUserPointer)GSSTD_BLOB_TYPE_TAG,GSTrue)))
        return -1;
    *ptr = blob->GetBuf();
    return GS_OK;
}

GSInteger gstd_getblobsize(HGameScriptVM v,GSInteger idx)
{
    GSBlob *blob;
    if(GS_FAILED(GS_getinstanceup(v,idx,(GSUserPointer *)&blob,(GSUserPointer)GSSTD_BLOB_TYPE_TAG,GSTrue)))
        return -1;
    return blob->Len();
}

GSUserPointer gstd_createblob(HGameScriptVM v, GSInteger size)
{
    GSInteger top = GS_gettop(v);
    GS_pushregistrytable(v);
    GS_pushstring(v,_SC("std_blob"),-1);
    if(GS_SUCCEEDED(GS_get(v,-2))) {
        GS_remove(v,-2); //removes the registry
        GS_push(v,1); // push the this
        GS_pushinteger(v,size); //size
        GSBlob *blob = NULL;
        if(GS_SUCCEEDED(GS_call(v,2,GSTrue,GSFalse))
            && GS_SUCCEEDED(GS_getinstanceup(v,-1,(GSUserPointer *)&blob,(GSUserPointer)GSSTD_BLOB_TYPE_TAG,GSTrue))) {
            GS_remove(v,-2);
            return blob->GetBuf();
        }
    }
    GS_settop(v,top);
    return NULL;
}

GSRESULT gstd_register_bloblib(HGameScriptVM v)
{
    return declare_stream(v,_SC("blob"),(GSUserPointer)GSSTD_BLOB_TYPE_TAG,_SC("std_blob"),_blob_methods,bloblib_funcs);
}


