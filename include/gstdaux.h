/*  see copyright notice in GameScript.h */
#ifndef _GSSTD_AUXLIB_H_
#define _GSSTD_AUXLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

GameScript_API void gstd_seterrorhandlers(HGameScriptVM v);
GameScript_API void gstd_printcallstack(HGameScriptVM v);

GameScript_API GSRESULT gstd_throwerrorf(HGameScriptVM v,const GSChar *err,...);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* _GSSTD_AUXLIB_H_ */

