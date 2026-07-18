/* see copyright notice in GameScript.h */
#include <new>
#include <stdio.h>
#include <GameScript.h>
#include <gstdio.h>
#include "gstdstream.h"

#define GSSTD_FILE_TYPE_TAG ((GSUnsignedInteger)(GSSTD_STREAM_TYPE_TAG | 0x00000001))
//basic API
GSFILE gstd_fopen(const GSChar *filename ,const GSChar *mode)
{
#ifndef GSUNICODE
    return (GSFILE)fopen(filename,mode);
#else
    return (GSFILE)_wfopen(filename,mode);
#endif
}

GSInteger gstd_fread(void* buffer, GSInteger size, GSInteger count, GSFILE file)
{
    GSInteger ret = (GSInteger)fread(buffer,size,count,(FILE *)file);
    return ret;
}

GSInteger gstd_fwrite(const GSUserPointer buffer, GSInteger size, GSInteger count, GSFILE file)
{
    return (GSInteger)fwrite(buffer,size,count,(FILE *)file);
}

GSInteger gstd_fseek(GSFILE file, GSInteger offset, GSInteger origin)
{
    GSInteger realorigin;
    switch(origin) {
        case GS_SEEK_CUR: realorigin = SEEK_CUR; break;
        case GS_SEEK_END: realorigin = SEEK_END; break;
        case GS_SEEK_SET: realorigin = SEEK_SET; break;
        default: return -1; //failed
    }
    return fseek((FILE *)file,(long)offset,(int)realorigin);
}

GSInteger gstd_ftell(GSFILE file)
{
    return ftell((FILE *)file);
}

GSInteger gstd_fflush(GSFILE file)
{
    return fflush((FILE *)file);
}

GSInteger gstd_fclose(GSFILE file)
{
    return fclose((FILE *)file);
}

GSInteger gstd_feof(GSFILE file)
{
    return feof((FILE *)file);
}

//File
struct GSFile : public GSStream {
    GSFile() { _handle = NULL; _owns = false;}
    GSFile(GSFILE file, bool owns) { _handle = file; _owns = owns;}
    virtual ~GSFile() { Close(); }
    bool Open(const GSChar *filename ,const GSChar *mode) {
        Close();
        if( (_handle = gstd_fopen(filename,mode)) ) {
            _owns = true;
            return true;
        }
        return false;
    }
    void Close() {
        if(_handle && _owns) {
            gstd_fclose(_handle);
            _handle = NULL;
            _owns = false;
        }
    }
    GSInteger Read(void *buffer,GSInteger size) {
        return gstd_fread(buffer,1,size,_handle);
    }
    GSInteger Write(void *buffer,GSInteger size) {
        return gstd_fwrite(buffer,1,size,_handle);
    }
    GSInteger Flush() {
        return gstd_fflush(_handle);
    }
    GSInteger Tell() {
        return gstd_ftell(_handle);
    }
    GSInteger Len() {
        GSInteger prevpos=Tell();
        Seek(0,GS_SEEK_END);
        GSInteger size=Tell();
        Seek(prevpos,GS_SEEK_SET);
        return size;
    }
    GSInteger Seek(GSInteger offset, GSInteger origin)  {
        return gstd_fseek(_handle,offset,origin);
    }
    bool IsValid() { return _handle?true:false; }
    bool EOS() { return Tell()==Len()?true:false;}
    GSFILE GetHandle() {return _handle;}
private:
    GSFILE _handle;
    bool _owns;
};

static GSInteger _file__typeof(HGameScriptVM v)
{
    GS_pushstring(v,_SC("file"),-1);
    return 1;
}

static GSInteger _file_releasehook(GSUserPointer p, GSInteger GS_UNUSED_ARG(size))
{
    GSFile *self = (GSFile*)p;
    self->~GSFile();
    GS_free(self,sizeof(GSFile));
    return 1;
}

static GSInteger _file_constructor(HGameScriptVM v)
{
    const GSChar *filename,*mode;
    bool owns = true;
    GSFile *f;
    GSFILE newf;
    if(GS_gettype(v,2) == OT_STRING && GS_gettype(v,3) == OT_STRING) {
        GS_getstring(v, 2, &filename);
        GS_getstring(v, 3, &mode);
        newf = gstd_fopen(filename, mode);
        if(!newf) return GS_throwerror(v, _SC("cannot open file"));
    } else if(GS_gettype(v,2) == OT_USERPOINTER) {
        owns = !(GS_gettype(v,3) == OT_NULL);
        GS_getuserpointer(v,2,&newf);
    } else {
        return GS_throwerror(v,_SC("wrong parameter"));
    }

    f = new (GS_malloc(sizeof(GSFile)))GSFile(newf,owns);
    if(GS_FAILED(GS_setinstanceup(v,1,f))) {
        f->~GSFile();
        GS_free(f,sizeof(GSFile));
        return GS_throwerror(v, _SC("cannot create blob with negative size"));
    }
    GS_setreleasehook(v,1,_file_releasehook);
    return 0;
}

static GSInteger _file_close(HGameScriptVM v)
{
    GSFile *self = NULL;
    if(GS_SUCCEEDED(GS_getinstanceup(v,1,(GSUserPointer*)&self,(GSUserPointer)GSSTD_FILE_TYPE_TAG, GSTrue))
        && self != NULL)
    {
        self->Close();
    }
    return 0;
}

//bindings
#define _DECL_FILE_FUNC(name,nparams,typecheck) {_SC(#name),_file_##name,nparams,typecheck}
static const GSRegFunction _file_methods[] = {
    _DECL_FILE_FUNC(constructor,3,_SC("x")),
    _DECL_FILE_FUNC(_typeof,1,_SC("x")),
    _DECL_FILE_FUNC(close,1,_SC("x")),
    {NULL,(GSFUNCTION)0,0,NULL}
};



GSRESULT gstd_createfile(HGameScriptVM v, GSFILE file,GSBool own)
{
    GSInteger top = GS_gettop(v);
    GS_pushregistrytable(v);
    GS_pushstring(v,_SC("std_file"),-1);
    if(GS_SUCCEEDED(GS_get(v,-2))) {
        GS_remove(v,-2); //removes the registry
        GS_pushroottable(v); // push the this
        GS_pushuserpointer(v,file); //file
        if(own){
            GS_pushinteger(v,1); //true
        }
        else{
            GS_pushnull(v); //false
        }
        if(GS_SUCCEEDED( GS_call(v,3,GSTrue,GSFalse) )) {
            GS_remove(v,-2);
            return GS_OK;
        }
    }
    GS_settop(v,top);
    return GS_ERROR;
}

GSRESULT gstd_getfile(HGameScriptVM v, GSInteger idx, GSFILE *file)
{
    GSFile *fileobj = NULL;
    if(GS_SUCCEEDED(GS_getinstanceup(v,idx,(GSUserPointer*)&fileobj,(GSUserPointer)GSSTD_FILE_TYPE_TAG,GSFalse))) {
        *file = fileobj->GetHandle();
        return GS_OK;
    }
    return GS_throwerror(v,_SC("not a file"));
}



#define IO_BUFFER_SIZE 2048
struct IOBuffer {
    unsigned char buffer[IO_BUFFER_SIZE];
    GSInteger size;
    GSInteger ptr;
    GSFILE file;
};

GSInteger _read_byte(IOBuffer *iobuffer)
{
    if(iobuffer->ptr < iobuffer->size) {

        GSInteger ret = iobuffer->buffer[iobuffer->ptr];
        iobuffer->ptr++;
        return ret;
    }
    else {
        if( (iobuffer->size = gstd_fread(iobuffer->buffer,1,IO_BUFFER_SIZE,iobuffer->file )) > 0 )
        {
            GSInteger ret = iobuffer->buffer[0];
            iobuffer->ptr = 1;
            return ret;
        }
    }

    return 0;
}

GSInteger _read_two_bytes(IOBuffer *iobuffer)
{
    if(iobuffer->ptr < iobuffer->size) {
        if(iobuffer->size < 2) return 0;
        GSInteger ret = *((const wchar_t*)&iobuffer->buffer[iobuffer->ptr]);
        iobuffer->ptr += 2;
        return ret;
    }
    else {
        if( (iobuffer->size = gstd_fread(iobuffer->buffer,1,IO_BUFFER_SIZE,iobuffer->file )) > 0 )
        {
            if(iobuffer->size < 2) return 0;
            GSInteger ret = *((const wchar_t*)&iobuffer->buffer[0]);
            iobuffer->ptr = 2;
            return ret;
        }
    }

    return 0;
}

static GSInteger _io_file_lexfeed_PLAIN(GSUserPointer iobuf)
{
    IOBuffer *iobuffer = (IOBuffer *)iobuf;
    return _read_byte(iobuffer);

}

#ifdef GSUNICODE
static GSInteger _io_file_lexfeed_UTF8(GSUserPointer iobuf)
{
    IOBuffer *iobuffer = (IOBuffer *)iobuf;
#define READ(iobuf) \
    if((inchar = (unsigned char)_read_byte(iobuf)) == 0) \
        return 0;

    static const GSInteger utf8_lengths[16] =
    {
        1,1,1,1,1,1,1,1,        /* 0000 to 0111 : 1 byte (plain ASCII) */
        0,0,0,0,                /* 1000 to 1011 : not valid */
        2,2,                    /* 1100, 1101 : 2 bytes */
        3,                      /* 1110 : 3 bytes */
        4                       /* 1111 :4 bytes */
    };
    static const unsigned char byte_masks[5] = {0,0,0x1f,0x0f,0x07};
    unsigned char inchar;
    GSInteger c = 0;
    READ(iobuffer);
    c = inchar;
    //
    if(c >= 0x80) {
        GSInteger tmp;
        GSInteger codelen = utf8_lengths[c>>4];
        if(codelen == 0)
            return 0;
            //"invalid UTF-8 stream";
        tmp = c&byte_masks[codelen];
        for(GSInteger n = 0; n < codelen-1; n++) {
            tmp<<=6;
            READ(iobuffer);
            tmp |= inchar & 0x3F;
        }
        c = tmp;
    }
    return c;
}
#endif

static GSInteger _io_file_lexfeed_UCS2_LE(GSUserPointer iobuf)
{
    GSInteger ret;
    IOBuffer *iobuffer = (IOBuffer *)iobuf;
    if( (ret = _read_two_bytes(iobuffer)) > 0 )
        return ret;
    return 0;
}

static GSInteger _io_file_lexfeed_UCS2_BE(GSUserPointer iobuf)
{
    GSInteger c;
    IOBuffer *iobuffer = (IOBuffer *)iobuf;
    if( (c = _read_two_bytes(iobuffer)) > 0 ) {
        c = ((c>>8)&0x00FF)| ((c<<8)&0xFF00);
        return c;
    }
    return 0;
}

GSInteger file_read(GSUserPointer file,GSUserPointer buf,GSInteger size)
{
    GSInteger ret;
    if( ( ret = gstd_fread(buf,1,size,(GSFILE)file ))!=0 )return ret;
    return -1;
}

GSInteger file_write(GSUserPointer file,GSUserPointer p,GSInteger size)
{
    return gstd_fwrite(p,1,size,(GSFILE)file);
}

GSRESULT gstd_loadfile(HGameScriptVM v,const GSChar *filename,GSBool printerror)
{
    GSFILE file = gstd_fopen(filename,_SC("rb"));

    GSInteger ret;
    unsigned short us;
    unsigned char uc;
    GSLEXREADFUNC func = _io_file_lexfeed_PLAIN;
    if(file){
        ret = gstd_fread(&us,1,2,file);
        if(ret != 2) {
            //probably an empty file
            us = 0;
        }
        if(us == GS_BYTECODE_STREAM_TAG) { //BYTECODE
            gstd_fseek(file,0,GS_SEEK_SET);
            if(GS_SUCCEEDED(GS_readclosure(v,file_read,file))) {
                gstd_fclose(file);
                return GS_OK;
            }
        }
        else { //SCRIPT

            switch(us)
            {
                //gotta swap the next 2 lines on BIG endian machines
                case 0xFFFE: func = _io_file_lexfeed_UCS2_BE; break;//UTF-16 little endian;
                case 0xFEFF: func = _io_file_lexfeed_UCS2_LE; break;//UTF-16 big endian;
                case 0xBBEF:
                    if(gstd_fread(&uc,1,sizeof(uc),file) == 0) {
                        gstd_fclose(file);
                        return GS_throwerror(v,_SC("io error"));
                    }
                    if(uc != 0xBF) {
                        gstd_fclose(file);
                        return GS_throwerror(v,_SC("Unrecognized encoding"));
                    }
#ifdef GSUNICODE
                    func = _io_file_lexfeed_UTF8;
#else
                    func = _io_file_lexfeed_PLAIN;
#endif
                    break;//UTF-8 ;
                default: gstd_fseek(file,0,GS_SEEK_SET); break; // ascii
            }
            IOBuffer buffer;
            buffer.ptr = 0;
            buffer.size = 0;
            buffer.file = file;
            if(GS_SUCCEEDED(GS_compile(v,func,&buffer,filename,printerror))){
                gstd_fclose(file);
                return GS_OK;
            }
        }
        gstd_fclose(file);
        return GS_ERROR;
    }
    return GS_throwerror(v,_SC("cannot open the file"));
}

GSRESULT gstd_dofile(HGameScriptVM v,const GSChar *filename,GSBool retval,GSBool printerror)
{
    //at least one entry must exist in order for us to push it as the environment
    if(GS_gettop(v) == 0)
        return GS_throwerror(v,_SC("environment table expected"));

    if(GS_SUCCEEDED(gstd_loadfile(v,filename,printerror))) {
        GS_push(v,-2);
        if(GS_SUCCEEDED(GS_call(v,1,retval,GSTrue))) {
            GS_remove(v,retval?-2:-1); //removes the closure
            return 1;
        }
        GS_pop(v,1); //removes the closure
    }
    return GS_ERROR;
}

GSRESULT gstd_writeclosuretofile(HGameScriptVM v,const GSChar *filename)
{
    GSFILE file = gstd_fopen(filename,_SC("wb+"));
    if(!file) return GS_throwerror(v,_SC("cannot open the file"));
    if(GS_SUCCEEDED(GS_writeclosure(v,file_write,file))) {
        gstd_fclose(file);
        return GS_OK;
    }
    gstd_fclose(file);
    return GS_ERROR; //forward the error
}

GSInteger _g_io_loadfile(HGameScriptVM v)
{
    const GSChar *filename;
    GSBool printerror = GSFalse;
    GS_getstring(v,2,&filename);
    if(GS_gettop(v) >= 3) {
        GS_getbool(v,3,&printerror);
    }
    if(GS_SUCCEEDED(gstd_loadfile(v,filename,printerror)))
        return 1;
    return GS_ERROR; //propagates the error
}

GSInteger _g_io_writeclosuretofile(HGameScriptVM v)
{
    const GSChar *filename;
    GS_getstring(v,2,&filename);
    if(GS_SUCCEEDED(gstd_writeclosuretofile(v,filename)))
        return 1;
    return GS_ERROR; //propagates the error
}

GSInteger _g_io_dofile(HGameScriptVM v)
{
    const GSChar *filename;
    GSBool printerror = GSFalse;
    GS_getstring(v,2,&filename);
    if(GS_gettop(v) >= 3) {
        GS_getbool(v,3,&printerror);
    }
    GS_push(v,1); //repush the this
    if(GS_SUCCEEDED(gstd_dofile(v,filename,GSTrue,printerror)))
        return 1;
    return GS_ERROR; //propagates the error
}

#define _DECL_GLOBALIO_FUNC(name,nparams,typecheck) {_SC(#name),_g_io_##name,nparams,typecheck}
static const GSRegFunction iolib_funcs[]={
    _DECL_GLOBALIO_FUNC(loadfile,-2,_SC(".sb")),
    _DECL_GLOBALIO_FUNC(dofile,-2,_SC(".sb")),
    _DECL_GLOBALIO_FUNC(writeclosuretofile,3,_SC(".sc")),
    {NULL,(GSFUNCTION)0,0,NULL}
};

GSRESULT gstd_register_iolib(HGameScriptVM v)
{
    GSInteger top = GS_gettop(v);
    //create delegate
    declare_stream(v,_SC("file"),(GSUserPointer)GSSTD_FILE_TYPE_TAG,_SC("std_file"),_file_methods,iolib_funcs);
    GS_pushstring(v,_SC("stdout"),-1);
    gstd_createfile(v,stdout,GSFalse);
    GS_newslot(v,-3,GSFalse);
    GS_pushstring(v,_SC("stdin"),-1);
    gstd_createfile(v,stdin,GSFalse);
    GS_newslot(v,-3,GSFalse);
    GS_pushstring(v,_SC("stderr"),-1);
    gstd_createfile(v,stderr,GSFalse);
    GS_newslot(v,-3,GSFalse);
    GS_settop(v,top);
    return GS_OK;
}

