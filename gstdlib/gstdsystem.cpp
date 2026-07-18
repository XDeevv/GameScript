/* see copyright notice in GameScript.h */
#include <GameScript.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <gstdsystem.h>

#if defined(__APPLE__) && !defined(IOS)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define IOS
#endif
#endif

#ifdef GSUNICODE
#include <wchar.h>
#define scgetenv _wgetenv
#define scsystem _wsystem
#define scasctime _wasctime
#define scremove _wremove
#define screname _wrename
#else
#define scgetenv getenv
#define scsystem system
#define scasctime asctime
#define scremove remove
#define screname rename
#endif
#ifdef IOS
	#include <spawn.h>
	extern char **environ;
#endif

static GSInteger _system_getenv(HGameScriptVM v)
{
    const GSChar *s;
    if(GS_SUCCEEDED(GS_getstring(v,2,&s))){
        GS_pushstring(v,scgetenv(s),-1);
        return 1;
    }
    return 0;
}

static GSInteger _system_system(HGameScriptVM v)
{
    const GSChar *s;
    if(GS_SUCCEEDED(GS_getstring(v,2,&s))){
	#ifdef IOS
		pid_t pid;
		posix_spawn(&pid, s, NULL, NULL, NULL, environ);
		GS_pushinteger(v, 0);
	#else
	        GS_pushinteger(v,scsystem(s));
	#endif
        return 1;
    }
    return GS_throwerror(v,_SC("wrong param"));
}

static GSInteger _system_clock(HGameScriptVM v)
{
    GS_pushfloat(v,((GSFloat)clock())/(GSFloat)CLOCKS_PER_SEC);
    return 1;
}

static GSInteger _system_time(HGameScriptVM v)
{
    GSInteger t = (GSInteger)time(NULL);
    GS_pushinteger(v,t);
    return 1;
}

static GSInteger _system_remove(HGameScriptVM v)
{
    const GSChar *s;
    GS_getstring(v,2,&s);
    if(scremove(s)==-1)
        return GS_throwerror(v,_SC("remove() failed"));
    return 0;
}

static GSInteger _system_rename(HGameScriptVM v)
{
    const GSChar *oldn,*newn;
    GS_getstring(v,2,&oldn);
    GS_getstring(v,3,&newn);
    if(screname(oldn,newn)==-1)
        return GS_throwerror(v,_SC("rename() failed"));
    return 0;
}

static void _set_integer_slot(HGameScriptVM v,const GSChar *name,GSInteger val)
{
    GS_pushstring(v,name,-1);
    GS_pushinteger(v,val);
    GS_rawset(v,-3);
}

static GSInteger _system_date(HGameScriptVM v)
{
    time_t t;
    GSInteger it;
    GSInteger format = 'l';
    if(GS_gettop(v) > 1) {
        GS_getinteger(v,2,&it);
        t = it;
        if(GS_gettop(v) > 2) {
            GS_getinteger(v,3,(GSInteger*)&format);
        }
    }
    else {
        time(&t);
    }
    tm *date;
    if(format == 'u')
        date = gmtime(&t);
    else
        date = localtime(&t);
    if(!date)
        return GS_throwerror(v,_SC("crt api failure"));
    GS_newtable(v);
    _set_integer_slot(v, _SC("sec"), date->tm_sec);
    _set_integer_slot(v, _SC("min"), date->tm_min);
    _set_integer_slot(v, _SC("hour"), date->tm_hour);
    _set_integer_slot(v, _SC("day"), date->tm_mday);
    _set_integer_slot(v, _SC("month"), date->tm_mon);
    _set_integer_slot(v, _SC("year"), date->tm_year+1900);
    _set_integer_slot(v, _SC("wday"), date->tm_wday);
    _set_integer_slot(v, _SC("yday"), date->tm_yday);
    return 1;
}



#define _DECL_FUNC(name,nparams,pmask) {_SC(#name),_system_##name,nparams,pmask}
static const GSRegFunction systemlib_funcs[]={
    _DECL_FUNC(getenv,2,_SC(".s")),
    _DECL_FUNC(system,2,_SC(".s")),
    _DECL_FUNC(clock,0,NULL),
    _DECL_FUNC(time,1,NULL),
    _DECL_FUNC(date,-1,_SC(".nn")),
    _DECL_FUNC(remove,2,_SC(".s")),
    _DECL_FUNC(rename,3,_SC(".ss")),
    {NULL,(GSFUNCTION)0,0,NULL}
};
#undef _DECL_FUNC

GSInteger gstd_register_systemlib(HGameScriptVM v)
{
    GSInteger i=0;
    while(systemlib_funcs[i].name!=0)
    {
        GS_pushstring(v,systemlib_funcs[i].name,-1);
        GS_newclosure(v,systemlib_funcs[i].f,0);
        GS_setparamscheck(v,systemlib_funcs[i].nparamscheck,systemlib_funcs[i].typemask);
        GS_setnativeclosurename(v,-1,systemlib_funcs[i].name);
        GS_newslot(v,-3,GSFalse);
        i++;
    }
    return 1;
}

