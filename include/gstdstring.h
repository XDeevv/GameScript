/*  see copyright notice in GameScript.h */
#ifndef _GSSTD_STRING_H_
#define _GSSTD_STRING_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int GSRexBool;
typedef struct GSRex GSRex;

typedef struct {
    const GSChar *begin;
    GSInteger len;
} GSRexMatch;

GameScript_API GSRex *gstd_rex_compile(const GSChar *pattern,const GSChar **error);
GameScript_API void gstd_rex_free(GSRex *exp);
GameScript_API GSBool gstd_rex_match(GSRex* exp,const GSChar* text);
GameScript_API GSBool gstd_rex_search(GSRex* exp,const GSChar* text, const GSChar** out_begin, const GSChar** out_end);
GameScript_API GSBool gstd_rex_searchrange(GSRex* exp,const GSChar* text_begin,const GSChar* text_end,const GSChar** out_begin, const GSChar** out_end);
GameScript_API GSInteger gstd_rex_getsubexpcount(GSRex* exp);
GameScript_API GSBool gstd_rex_getsubexp(GSRex* exp, GSInteger n, GSRexMatch *subexp);

GameScript_API GSRESULT gstd_format(HGameScriptVM v,GSInteger nformatstringidx,GSInteger *outlen,GSChar **output);

GameScript_API void gstd_pushstringf(HGameScriptVM v,const GSChar *s,...);

GameScript_API GSRESULT gstd_register_stringlib(HGameScriptVM v);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*_GSSTD_STRING_H_*/

