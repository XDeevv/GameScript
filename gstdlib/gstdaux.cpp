/* see copyright notice in GameScript.h */
#include <GameScript.h>
#include <gstdaux.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

void gstd_printcallstack(HGameScriptVM v)
{
    GSPRINTFUNCTION pf = GS_geterrorfunc(v);
    if(pf) {
        GSStackInfos si;
        GSInteger i;
        GSFloat f;
        const GSChar *s;
        GSInteger level=1; //1 is to skip this function that is level 0
        const GSChar *name=0;
        GSInteger seq=0;
        pf(v,_SC("\nCALLSTACK\n"));
        while(GS_SUCCEEDED(GS_stackinfos(v,level,&si)))
        {
            const GSChar *fn=_SC("unknown");
            const GSChar *src=_SC("unknown");
            if(si.funcname)fn=si.funcname;
            if(si.source)src=si.source;
            pf(v,_SC("*FUNCTION [%s()] %s line [%d]\n"),fn,src,si.line);
            level++;
        }
        level=0;
        pf(v,_SC("\nLOCALS\n"));

        for(level=0;level<10;level++){
            seq=0;
            while((name = GS_getlocal(v,level,seq)))
            {
                seq++;
                switch(GS_gettype(v,-1))
                {
                case OT_NULL:
                    pf(v,_SC("[%s] NULL\n"),name);
                    break;
                case OT_INTEGER:
                    GS_getinteger(v,-1,&i);
                    pf(v,_SC("[%s] %d\n"),name,i);
                    break;
                case OT_FLOAT:
                    GS_getfloat(v,-1,&f);
                    pf(v,_SC("[%s] %.14g\n"),name,f);
                    break;
                case OT_USERPOINTER:
                    pf(v,_SC("[%s] USERPOINTER\n"),name);
                    break;
                case OT_STRING:
                    GS_getstring(v,-1,&s);
                    pf(v,_SC("[%s] \"%s\"\n"),name,s);
                    break;
                case OT_TABLE:
                    pf(v,_SC("[%s] TABLE\n"),name);
                    break;
                case OT_ARRAY:
                    pf(v,_SC("[%s] ARRAY\n"),name);
                    break;
                case OT_CLOSURE:
                    pf(v,_SC("[%s] CLOSURE\n"),name);
                    break;
                case OT_NATIVECLOSURE:
                    pf(v,_SC("[%s] NATIVECLOSURE\n"),name);
                    break;
                case OT_GENERATOR:
                    pf(v,_SC("[%s] GENERATOR\n"),name);
                    break;
                case OT_USERDATA:
                    pf(v,_SC("[%s] USERDATA\n"),name);
                    break;
                case OT_THREAD:
                    pf(v,_SC("[%s] THREAD\n"),name);
                    break;
                case OT_CLASS:
                    pf(v,_SC("[%s] CLASS\n"),name);
                    break;
                case OT_INSTANCE:
                    pf(v,_SC("[%s] INSTANCE\n"),name);
                    break;
                case OT_WEAKREF:
                    pf(v,_SC("[%s] WEAKREF\n"),name);
                    break;
                case OT_BOOL:{
                    GSBool bval;
                    GS_getbool(v,-1,&bval);
                    pf(v,_SC("[%s] %s\n"),name,bval == GSTrue ? _SC("true"):_SC("false"));
                             }
                    break;
                default: assert(0); break;
                }
                GS_pop(v,1);
            }
        }
    }
}

static GSInteger _gstd_aux_printerror(HGameScriptVM v)
{
    GSPRINTFUNCTION pf = GS_geterrorfunc(v);
    if(pf) {
        const GSChar *sErr = 0;
        if(GS_gettop(v)>=1) {
            if(GS_SUCCEEDED(GS_getstring(v,2,&sErr)))   {
                pf(v,_SC("\nAN ERROR HAS OCCURRED [%s]\n"),sErr);
            }
            else{
                pf(v,_SC("\nAN ERROR HAS OCCURRED [unknown]\n"));
            }
            gstd_printcallstack(v);
        }
    }
    return 0;
}

void _gstd_compiler_error(HGameScriptVM v,const GSChar *sErr,const GSChar *sSource,GSInteger line,GSInteger column)
{
    GSPRINTFUNCTION pf = GS_geterrorfunc(v);
    if(pf) {
        pf(v,_SC("%s line = (%d) column = (%d) : error %s\n"),sSource,line,column,sErr);
    }
}

void gstd_seterrorhandlers(HGameScriptVM v)
{
    GS_setcompilererrorhandler(v,_gstd_compiler_error);
    GS_newclosure(v,_gstd_aux_printerror,0);
    GS_seterrorhandler(v);
}

GSRESULT gstd_throwerrorf(HGameScriptVM v,const GSChar *err,...)
{
    GSInteger n=256;
    va_list args;
begin:
    va_start(args,err);
    GSChar *b=GS_getscratchpad(v,n);
    GSInteger r=scvsprintf(b,n,err,args);
    va_end(args);
    if (r>=n) {
        n=r+1;//required+null
        goto begin;
    } else if (r<0) {
        return GS_throwerror(v,_SC("@failed to generate formatted error message"));
    } else {
        return GS_throwerror(v,b);
    }
}

