/*  see copyright notice in GameScript.h */
#ifndef _GSSTDIO_H_
#define _GSSTDIO_H_

#ifdef __cplusplus

#define GSSTD_STREAM_TYPE_TAG 0x80000000

struct GSStream {
    virtual ~GSStream() {}
    virtual GSInteger Read(void *buffer, GSInteger size) = 0;
    virtual GSInteger Write(void *buffer, GSInteger size) = 0;
    virtual GSInteger Flush() = 0;
    virtual GSInteger Tell() = 0;
    virtual GSInteger Len() = 0;
    virtual GSInteger Seek(GSInteger offset, GSInteger origin) = 0;
    virtual bool IsValid() = 0;
    virtual bool EOS() = 0;
};

extern "C" {
#endif

#define GS_SEEK_CUR 0
#define GS_SEEK_END 1
#define GS_SEEK_SET 2

typedef void* GSFILE;

GameScript_API GSFILE gstd_fopen(const GSChar *,const GSChar *);
GameScript_API GSInteger gstd_fread(GSUserPointer, GSInteger, GSInteger, GSFILE);
GameScript_API GSInteger gstd_fwrite(const GSUserPointer, GSInteger, GSInteger, GSFILE);
GameScript_API GSInteger gstd_fseek(GSFILE , GSInteger , GSInteger);
GameScript_API GSInteger gstd_ftell(GSFILE);
GameScript_API GSInteger gstd_fflush(GSFILE);
GameScript_API GSInteger gstd_fclose(GSFILE);
GameScript_API GSInteger gstd_feof(GSFILE);

GameScript_API GSRESULT gstd_createfile(HGameScriptVM v, GSFILE file,GSBool own);
GameScript_API GSRESULT gstd_getfile(HGameScriptVM v, GSInteger idx, GSFILE *file);

//compiler helpers
GameScript_API GSRESULT gstd_loadfile(HGameScriptVM v,const GSChar *filename,GSBool printerror);
GameScript_API GSRESULT gstd_dofile(HGameScriptVM v,const GSChar *filename,GSBool retval,GSBool printerror);
GameScript_API GSRESULT gstd_writeclosuretofile(HGameScriptVM v,const GSChar *filename);

GameScript_API GSRESULT gstd_register_iolib(HGameScriptVM v);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*_GSSTDIO_H_*/


