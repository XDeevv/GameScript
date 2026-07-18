/*  see copyright notice in GameScript.h */
#ifndef _GSSTD_STREAM_H_
#define _GSSTD_STREAM_H_

GSInteger _stream_readblob(HGameScriptVM v);
GSInteger _stream_readline(HGameScriptVM v);
GSInteger _stream_readn(HGameScriptVM v);
GSInteger _stream_writeblob(HGameScriptVM v);
GSInteger _stream_writen(HGameScriptVM v);
GSInteger _stream_seek(HGameScriptVM v);
GSInteger _stream_tell(HGameScriptVM v);
GSInteger _stream_len(HGameScriptVM v);
GSInteger _stream_eos(HGameScriptVM v);
GSInteger _stream_flush(HGameScriptVM v);

#define _DECL_STREAM_FUNC(name,nparams,typecheck) {_SC(#name),_stream_##name,nparams,typecheck}
GSRESULT declare_stream(HGameScriptVM v,const GSChar* name,GSUserPointer typetag,const GSChar* reg_name,const GSRegFunction *methods,const GSRegFunction *globals);
#endif /*_GSSTD_STREAM_H_*/

