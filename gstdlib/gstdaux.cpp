/* see copyright notice in GameScript.h */
#include <GameScript.h>
#include <gstdaux.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

#define GS_COLOR_RED     _SC("\x1b[31m")
#define GS_COLOR_GREEN   _SC("\x1b[32m")
#define GS_COLOR_YELLOW  _SC("\x1b[33m")
#define GS_COLOR_BLUE    _SC("\x1b[34m")
#define GS_COLOR_MAGENTA _SC("\x1b[35m")
#define GS_COLOR_CYAN    _SC("\x1b[36m")
#define GS_COLOR_RESET   _SC("\x1b[0m")
#define GS_COLOR_BOLD    _SC("\x1b[1m")

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
        
        pf(v, _SC("\n") GS_COLOR_BOLD GS_COLOR_YELLOW _SC("--- Callstack ---") GS_COLOR_RESET _SC("\n"));
        
        while(GS_SUCCEEDED(GS_stackinfos(v,level,&si)))
        {
            const GSChar *fn=_SC("unknown");
            const GSChar *src=_SC("unknown");
            if(si.funcname)fn=si.funcname;
            if(si.source)src=si.source;
            
            // Format:  at function_name() in filename.gs:line
            pf(v, _SC("  ") GS_COLOR_CYAN _SC("at ") GS_COLOR_GREEN _SC("%s()") GS_COLOR_RESET 
                  _SC(" in ") GS_COLOR_BLUE _SC("%s:%d\n") GS_COLOR_RESET, fn, src, si.line);
            level++;
        }
        level=0;
        
        // Beautiful Locals Header
        pf(v, _SC("\n") GS_COLOR_BOLD GS_COLOR_YELLOW _SC("--- Locals ---") GS_COLOR_RESET _SC("\n"));

        for(level=0;level<10;level++){
            seq=0;
            while((name = GS_getlocal(v,level,seq)))
            {
                seq++;
                // Common prefix for all variables:   var_name = 
                pf(v, _SC("  ") GS_COLOR_MAGENTA _SC("%s") GS_COLOR_RESET _SC(" = "), name);
                
                switch(GS_gettype(v,-1))
                {
                case OT_NULL:
                    pf(v, GS_COLOR_CYAN _SC("[NULL]\n") GS_COLOR_RESET);
                    break;
                case OT_INTEGER:
                    GS_getinteger(v,-1,&i);
                    pf(v,_SC("%d\n"),i);
                    break;
                case OT_FLOAT:
                    GS_getfloat(v,-1,&f);
                    pf(v,_SC("%.14g\n"),f);
                    break;
                case OT_USERPOINTER:
                    pf(v, GS_COLOR_CYAN _SC("USERPOINTER\n") GS_COLOR_RESET);
                    break;
                case OT_STRING:
                    GS_getstring(v,-1,&s);
                    pf(v, GS_COLOR_GREEN _SC("\"%s\"\n") GS_COLOR_RESET, s);
                    break;
                case OT_TABLE:
                    pf(v, GS_COLOR_CYAN _SC("TABLE\n") GS_COLOR_RESET);
                    break;
                case OT_ARRAY:
                    pf(v, GS_COLOR_CYAN _SC("ARRAY\n") GS_COLOR_RESET);
                    break;
                case OT_CLOSURE:
                    pf(v, GS_COLOR_CYAN _SC("CLOSURE\n") GS_COLOR_RESET);
                    break;
                case OT_NATIVECLOSURE:
                    pf(v, GS_COLOR_CYAN _SC("NATIVECLOSURE\n") GS_COLOR_RESET);
                    break;
                case OT_GENERATOR:
                    pf(v, GS_COLOR_CYAN _SC("GENERATOR\n") GS_COLOR_RESET);
                    break;
                case OT_USERDATA:
                    pf(v, GS_COLOR_CYAN _SC("USERDATA\n") GS_COLOR_RESET);
                    break;
                case OT_THREAD:
                    pf(v, GS_COLOR_CYAN _SC("THREAD\n") GS_COLOR_RESET);
                    break;
                case OT_CLASS:
                    pf(v, GS_COLOR_CYAN _SC("CLASS\n") GS_COLOR_RESET);
                    break;
                case OT_INSTANCE:
                    pf(v, GS_COLOR_CYAN _SC("INSTANCE\n") GS_COLOR_RESET);
                    break;
                case OT_WEAKREF:
                    pf(v, GS_COLOR_CYAN _SC("WEAKREF\n") GS_COLOR_RESET);
                    break;
                case OT_BOOL:{
                    GSBool bval;
                    GS_getbool(v,-1,&bval);
                    pf(v, GS_COLOR_CYAN _SC("%s\n") GS_COLOR_RESET, bval == GSTrue ? _SC("true"):_SC("false"));
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
                // Modern Runtime Error output
                pf(v, _SC("\n") GS_COLOR_BOLD GS_COLOR_RED _SC("runtime error: ") GS_COLOR_RESET 
                      GS_COLOR_BOLD _SC("%s\n") GS_COLOR_RESET, sErr);
            }
            else{
                pf(v, _SC("\n") GS_COLOR_BOLD GS_COLOR_RED _SC("runtime error: ") GS_COLOR_RESET 
                      GS_COLOR_BOLD _SC("unknown\n") GS_COLOR_RESET);
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
        // Modern Compiler Error output
        pf(v, GS_COLOR_BOLD GS_COLOR_RED _SC("error: ") GS_COLOR_RESET GS_COLOR_BOLD _SC("%s\n") GS_COLOR_RESET, sErr);
        pf(v, GS_COLOR_BLUE _SC("  --> ") GS_COLOR_RESET _SC("%s:%d:%d\n\n"), sSource, line, column);
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
        return GS_throwerror(v,GS_COLOR_RED _SC("@failed to generate formatted error message")  GS_COLOR_RESET);
    } else {
        return GS_throwerror(v,b);
    }
}