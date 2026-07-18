/*  see copyright notice in GameScript.h */
#ifndef _GSSTDBLOB_H_
#define _GSSTDBLOB_H_

#ifdef __cplusplus
extern "C" {
#endif

GameScript_API GSUserPointer gstd_createblob(HGameScriptVM v, GSInteger size);
GameScript_API GSRESULT gstd_getblob(HGameScriptVM v,GSInteger idx,GSUserPointer *ptr);
GameScript_API GSInteger gstd_getblobsize(HGameScriptVM v,GSInteger idx);

GameScript_API GSRESULT gstd_register_bloblib(HGameScriptVM v);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*_GSSTDBLOB_H_*/


