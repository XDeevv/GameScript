#include <stdarg.h>
#include <stdio.h>

#include <GameScript.h>
#include <gstdio.h>
#include <gstdaux.h>

#ifdef _MSC_VER
#pragma comment (lib ,"GameScript.lib")
#pragma comment (lib ,"gstdlib.lib")
#endif

#ifdef GSUNICODE

#define scvprintf vfwprintf
#else

#define scvprintf vfprintf
#endif

void printfunc(HGameScriptVM v,const GSChar *s,...)
{
    va_list vl;
    va_start(vl, s);
    scvprintf(stdout, s, vl);
    va_end(vl);
}

void errorfunc(HGameScriptVM v,const GSChar *s,...)
{
    va_list vl;
    va_start(vl, s);
    scvprintf(stderr, s, vl);
    va_end(vl);
}

void call_foo(HGameScriptVM v, int n,float f,const GSChar *s)
{
    GSInteger top = GS_gettop(v); //saves the stack size before the call
    GS_pushroottable(v); //pushes the global table
    GS_pushstring(v,_SC("foo"),-1);
    if(GS_SUCCEEDED(GS_get(v,-2))) { //gets the field 'foo' from the global table
        GS_pushroottable(v); //push the 'this' (in this case is the global table)
        GS_pushinteger(v,n);
        GS_pushfloat(v,f);
        GS_pushstring(v,s,-1);
        GS_call(v,4,GSFalse,GSTrue); //calls the function
    }
    GS_settop(v,top); //restores the original stack size
}

int main(int argc, char* argv[])
{
    HGameScriptVM v;
    v = GS_open(1024); // creates a VM with initial stack size 1024

    //REGISTRATION OF STDLIB
    //GS_pushroottable(v); //push the root table where the std function will be registered
    //gstd_register_iolib(v);  //registers a library
    // ... call here other stdlibs string,math etc...
    //GS_pop(v,1); //pops the root table
    //END REGISTRATION OF STDLIB

    gstd_seterrorhandlers(v); //registers the default error handlers

    GS_setprintfunc(v, printfunc,errorfunc); //sets the print function

    GS_pushroottable(v); //push the root table(were the globals of the script will be stored)
    if(GS_SUCCEEDED(gstd_dofile(v, _SC("test.gs"), GSFalse, GSTrue))) // also prints syntax errors if any
    {
        call_foo(v,1,2.5,_SC("teststring"));
    }

    GS_pop(v,1); //pops the root table
    GS_close(v);

    return 0;
}

