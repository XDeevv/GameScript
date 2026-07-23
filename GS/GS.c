/*  see copyright notice in GameScript.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#include <conio.h>
#endif
#include <GameScript.h>
#include <gstdblob.h>
#include <gstdsystem.h>
#include <gstdio.h>
#include <gstdmath.h>
#include <gstdstring.h>
#include <gstdaux.h>

#include "version.h" 

#ifdef GSUNICODE
#define scfprintf fwprintf
#define scvprintf vfwprintf
#else
#define scfprintf fprintf
#define scvprintf vfprintf
#endif


void PrintVersionInfos();

#if defined(_MSC_VER) && defined(_DEBUG)
int MemAllocHook( int allocType, void *userData, size_t size, int blockType,
   long requestNumber, const unsigned char *filename, int lineNumber)
{
    //if(requestNumber==769)_asm int 3;
    return 1;
}
#endif


GSInteger quit(HGameScriptVM v)
{
    int *done;
    GS_getuserpointer(v,-1,(GSUserPointer*)&done);
    *done=1;
    return 0;
}

void printfunc(HGameScriptVM GS_UNUSED_ARG(v),const GSChar *s,...)
{
    va_list vl;
    va_start(vl, s);
    scvprintf(stdout, s, vl);
    va_end(vl);
}

void errorfunc(HGameScriptVM GS_UNUSED_ARG(v),const GSChar *s,...)
{
    va_list vl;
    va_start(vl, s);
    scvprintf(stderr, s, vl);
    va_end(vl);
}

void PrintVersionInfos()
{
    // Updated to use the macros from version.h
    scfprintf(stdout,_SC("GameScript v%d.%d.%d %s (%d bits)\n"),
              GAMESCRIPT_VERSION_MAJOR, 
              GAMESCRIPT_VERSION_MINOR, 
              GAMESCRIPT_VERSION_PATCH,
              GameScript_COPYRIGHT,
              ((int)(sizeof(GSInteger)*8)));
}

void PrintUsage()
{
    scfprintf(stderr,_SC("usage: GS <options> <scriptpath [args]>.\n")
        _SC("Available options are:\n")
        _SC("   -c              compiles the file to bytecode(default output 'out.compgs')\n")
        _SC("   -o              specifies output file for the -c option\n")
        _SC("   -c              compiles only\n")
        _SC("   -d              generates debug infos\n")
        _SC("   -v              displays version infos\n")
        _SC("   -h              prints help\n"));
}

#define _INTERACTIVE 0
#define _DONE 2
#define _ERROR 3
//<<FIXME>> this func is a mess
int getargs(HGameScriptVM v,int argc, char* argv[],GSInteger *retval)
{
    int i;
    int compiles_only = 0;
#ifdef GSUNICODE
    static GSChar temp[500];
#endif
    char * output = NULL;
    *retval = 0;
    if(argc>1)
    {
        int arg=1,exitloop=0;

        while(arg < argc && !exitloop)
        {

            if(argv[arg][0]=='-')
            {
                switch(argv[arg][1])
                {
                case 'd': //DEBUG(debug infos)
                    GS_enabledebuginfo(v,1);
                    break;
                case 'c':
                    compiles_only = 1;
                    break;
                case 'o':
                    if(arg < argc) {
                        arg++;
                        output = argv[arg];
                    }
                    break;
                case 'v':
                    PrintVersionInfos();
                    return _DONE;

                case 'h':
                    PrintVersionInfos();
                    PrintUsage();
                    return _DONE;
                default:
                    PrintVersionInfos();
                    scprintf(_SC("unknown prameter '-%c'\n"),argv[arg][1]);
                    PrintUsage();
                    *retval = -1;
                    return _ERROR;
                }
            }else break;
            arg++;
        }

        // src file

        if(arg<argc) {
            const GSChar *filename=NULL;
#ifdef GSUNICODE
            mbstowcs(temp,argv[arg],strlen(argv[arg]));
            filename=temp;
#else
            filename=argv[arg];
#endif

            arg++;

            //GS_pushstring(v,_SC("ARGS"),-1);
            //GS_newarray(v,0);

            //GS_createslot(v,-3);
            //GS_pop(v,1);
            if(compiles_only) {
                if(GS_SUCCEEDED(gstd_loadfile(v,filename,GSTrue))){
                    const GSChar *outfile = _SC("out.compgs");
                    if(output) {
#ifdef GSUNICODE
                        int len = (int)(strlen(output)+1);
                        mbstowcs(GS_getscratchpad(v,len*sizeof(GSChar)),output,len);
                        outfile = GS_getscratchpad(v,-1);
#else
                        outfile = output;
#endif
                    }
                    if(GS_SUCCEEDED(gstd_writeclosuretofile(v,outfile)))
                        return _DONE;
                }
            }
            else {
                //if(GS_SUCCEEDED(gstd_dofile(v,filename,GSFalse,GSTrue))) {
                    //return _DONE;
                //}
                if(GS_SUCCEEDED(gstd_loadfile(v,filename,GSTrue))) {
                    int callargs = 1;
                    GS_pushroottable(v);
                    for(i=arg;i<argc;i++)
                    {
                        const GSChar *a;
#ifdef GSUNICODE
                        int alen=(int)strlen(argv[i]);
                        a=GS_getscratchpad(v,(int)(alen*sizeof(GSChar)));
                        mbstowcs(GS_getscratchpad(v,-1),argv[i],alen);
                        GS_getscratchpad(v,-1)[alen] = _SC('\0');
#else
                        a=argv[i];
#endif
                        GS_pushstring(v,a,-1);
                        callargs++;
                        //GS_arrayappend(v,-2);
                    }
                    if(GS_SUCCEEDED(GS_call(v,callargs,GSTrue,GSTrue))) {
                        GSObjectType type = GS_gettype(v,-1);
                        if(type == OT_INTEGER) {
                            *retval = type;
                            GS_getinteger(v,-1,retval);
                        }
                        return _DONE;
                    }
                    else{
                        return _ERROR;
                    }

                }
            }
            //if this point is reached an error occurred
            {
                const GSChar *err;
                GS_getlasterror(v);
                if(GS_SUCCEEDED(GS_getstring(v,-1,&err))) {
                    scprintf(_SC("Error [%s]\n"),err);
                    *retval = -2;
                    return _ERROR;
                }
            }

        }
    }

    return _INTERACTIVE;
}

void Interactive(HGameScriptVM v)
{

#define MAXINPUT 1024
    GSChar buffer[MAXINPUT];
    GSInteger blocks =0;
    GSInteger string=0;
    GSInteger retval=0;
    GSInteger done=0;
    PrintVersionInfos();

    GS_pushroottable(v);
    GS_pushstring(v,_SC("quit"),-1);
    GS_pushuserpointer(v,&done);
    GS_newclosure(v,quit,1);
    GS_setparamscheck(v,1,NULL);
    GS_newslot(v,-3,GSFalse);
    GS_pop(v,1);

    while (!done)
    {
        GSInteger i = 0;
        scprintf(_SC("\nGS>"));
        for(;;) {
            int c;
            if(done)return;
            c = getchar();
            if (c == _SC('\n')) {
                if (i>0 && buffer[i-1] == _SC('\\'))
                {
                    buffer[i-1] = _SC('\n');
                }
                else if(blocks==0)break;
                buffer[i++] = _SC('\n');
            }
            else if (c==_SC('}')) {blocks--; buffer[i++] = (GSChar)c;}
            else if(c==_SC('{') && !string){
                    blocks++;
                    buffer[i++] = (GSChar)c;
            }
            else if(c==_SC('"') || c==_SC('\'')){
                    string=!string;
                    buffer[i++] = (GSChar)c;
            }
            else if (i >= MAXINPUT-1) {
                scfprintf(stderr, _SC("GS : input line too long\n"));
                break;
            }
            else{
                buffer[i++] = (GSChar)c;
            }
        }
        buffer[i] = _SC('\0');

        if(buffer[0]==_SC('=')){
            scsprintf(GS_getscratchpad(v,MAXINPUT),(size_t)MAXINPUT,_SC("return (%s)"),&buffer[1]);
            memcpy(buffer,GS_getscratchpad(v,-1),(scstrlen(GS_getscratchpad(v,-1))+1)*sizeof(GSChar));
            retval=1;
        }
        i=scstrlen(buffer);
        if(i>0){
            GSInteger oldtop=GS_gettop(v);
            if(GS_SUCCEEDED(GS_compilebuffer(v,buffer,i,_SC("interactive console"),GSTrue))){
                GS_pushroottable(v);
                if(GS_SUCCEEDED(GS_call(v,1,retval,GSTrue)) &&  retval){
                    scprintf(_SC("\n"));
                    GS_pushroottable(v);
                    GS_pushstring(v,_SC("print"),-1);
                    GS_get(v,-2);
                    GS_pushroottable(v);
                    GS_push(v,-4);
                    GS_call(v,2,GSFalse,GSTrue);
                    retval=0;
                    scprintf(_SC("\n"));
                }
            }

            GS_settop(v,oldtop);
        }
    }
}

int main(int argc, char* argv[])
{
    HGameScriptVM v;
    GSInteger retval = 0;
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(MemAllocHook);
#endif

    v=GS_open(1024);
    GS_setprintfunc(v,printfunc,errorfunc);

    GS_pushroottable(v);

    gstd_register_bloblib(v);
    gstd_register_iolib(v);
    gstd_register_systemlib(v);
    gstd_register_mathlib(v);
    gstd_register_stringlib(v);

    //aux library
    //sets error handlers
    gstd_seterrorhandlers(v);

    //gets arguments
    switch(getargs(v,argc,argv,&retval))
    {
    case _INTERACTIVE:
        Interactive(v);
        break;
    case _DONE:
    case _ERROR:
    default:
        break;
    }

    GS_close(v);

#if defined(_MSC_VER) && defined(_DEBUG)
    _getch();
    _CrtMemDumpAllObjectsSince( NULL );
#endif
    return retval;
}