/* see copyright notice in GameScript.h */
#include <GameScript.h>
#include <math.h>
#include <stdlib.h>
#include <gstdmath.h>

#define SINGLE_ARG_FUNC(_funcname) static GSInteger math_##_funcname(HGameScriptVM v){ \
    GSFloat f; \
    GS_getfloat(v,2,&f); \
    GS_pushfloat(v,(GSFloat)_funcname(f)); \
    return 1; \
}

#define TWO_ARGS_FUNC(_funcname) static GSInteger math_##_funcname(HGameScriptVM v){ \
    GSFloat p1,p2; \
    GS_getfloat(v,2,&p1); \
    GS_getfloat(v,3,&p2); \
    GS_pushfloat(v,(GSFloat)_funcname(p1,p2)); \
    return 1; \
}

static GSInteger math_srand(HGameScriptVM v)
{
    GSInteger i;
    if(GS_FAILED(GS_getinteger(v,2,&i)))
        return GS_throwerror(v,_SC("invalid param"));
    srand((unsigned int)i);
    return 0;
}

static GSInteger math_rand(HGameScriptVM v)
{
    GS_pushinteger(v,rand());
    return 1;
}

static GSInteger math_abs(HGameScriptVM v)
{
    GSInteger n;
    GS_getinteger(v,2,&n);
    GS_pushinteger(v,(GSInteger)abs((int)n));
    return 1;
}

SINGLE_ARG_FUNC(sqrt)
SINGLE_ARG_FUNC(fabs)
SINGLE_ARG_FUNC(sin)
SINGLE_ARG_FUNC(cos)
SINGLE_ARG_FUNC(asin)
SINGLE_ARG_FUNC(acos)
SINGLE_ARG_FUNC(log)
SINGLE_ARG_FUNC(log10)
SINGLE_ARG_FUNC(tan)
SINGLE_ARG_FUNC(atan)
TWO_ARGS_FUNC(atan2)
TWO_ARGS_FUNC(pow)
SINGLE_ARG_FUNC(floor)
SINGLE_ARG_FUNC(ceil)
SINGLE_ARG_FUNC(exp)

#define _DECL_FUNC(name,nparams,tycheck) {_SC(#name),math_##name,nparams,tycheck}
static const GSRegFunction mathlib_funcs[] = {
    _DECL_FUNC(sqrt,2,_SC(".n")),
    _DECL_FUNC(sin,2,_SC(".n")),
    _DECL_FUNC(cos,2,_SC(".n")),
    _DECL_FUNC(asin,2,_SC(".n")),
    _DECL_FUNC(acos,2,_SC(".n")),
    _DECL_FUNC(log,2,_SC(".n")),
    _DECL_FUNC(log10,2,_SC(".n")),
    _DECL_FUNC(tan,2,_SC(".n")),
    _DECL_FUNC(atan,2,_SC(".n")),
    _DECL_FUNC(atan2,3,_SC(".nn")),
    _DECL_FUNC(pow,3,_SC(".nn")),
    _DECL_FUNC(floor,2,_SC(".n")),
    _DECL_FUNC(ceil,2,_SC(".n")),
    _DECL_FUNC(exp,2,_SC(".n")),
    _DECL_FUNC(srand,2,_SC(".n")),
    _DECL_FUNC(rand,1,NULL),
    _DECL_FUNC(fabs,2,_SC(".n")),
    _DECL_FUNC(abs,2,_SC(".n")),
    {NULL,(GSFUNCTION)0,0,NULL}
};
#undef _DECL_FUNC

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

GSRESULT gstd_register_mathlib(HGameScriptVM v)
{
    GSInteger i=0;
    while(mathlib_funcs[i].name!=0) {
        GS_pushstring(v,mathlib_funcs[i].name,-1);
        GS_newclosure(v,mathlib_funcs[i].f,0);
        GS_setparamscheck(v,mathlib_funcs[i].nparamscheck,mathlib_funcs[i].typemask);
        GS_setnativeclosurename(v,-1,mathlib_funcs[i].name);
        GS_newslot(v,-3,GSFalse);
        i++;
    }
    GS_pushstring(v,_SC("RAND_MAX"),-1);
    GS_pushinteger(v,RAND_MAX);
    GS_newslot(v,-3,GSFalse);
    GS_pushstring(v,_SC("PI"),-1);
    GS_pushfloat(v,(GSFloat)M_PI);
    GS_newslot(v,-3,GSFalse);
    return GS_OK;
}

