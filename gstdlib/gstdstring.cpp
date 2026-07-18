/* see copyright notice in GameScript.h */
#include <GameScript.h>
#include <gstdstring.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>

#define MAX_FORMAT_LEN  20
#define MAX_WFORMAT_LEN 3
#define ADDITIONAL_FORMAT_SPACE (100*sizeof(GSChar))

static GSUserPointer rex_typetag = NULL;

static GSBool isfmtchr(GSChar ch)
{
    switch(ch) {
    case '-': case '+': case ' ': case '#': case '0': return GSTrue;
    }
    return GSFalse;
}

static GSInteger validate_format(HGameScriptVM v, GSChar *fmt, const GSChar *src, GSInteger n,GSInteger &width)
{
    GSChar *dummy;
    GSChar swidth[MAX_WFORMAT_LEN];
    GSInteger wc = 0;
    GSInteger start = n;
    fmt[0] = '%';
    while (isfmtchr(src[n])) n++;
    while (scisdigit(src[n])) {
        swidth[wc] = src[n];
        n++;
        wc++;
        if(wc>=MAX_WFORMAT_LEN)
            return GS_throwerror(v,_SC("width format too long"));
    }
    swidth[wc] = '\0';
    if(wc > 0) {
        width = scstrtol(swidth,&dummy,10);
    }
    else
        width = 0;
    if (src[n] == '.') {
        n++;

        wc = 0;
        while (scisdigit(src[n])) {
            swidth[wc] = src[n];
            n++;
            wc++;
            if(wc>=MAX_WFORMAT_LEN)
                return GS_throwerror(v,_SC("precision format too long"));
        }
        swidth[wc] = '\0';
        if(wc > 0) {
            width += scstrtol(swidth,&dummy,10);

        }
    }
    if (n-start > MAX_FORMAT_LEN )
        return GS_throwerror(v,_SC("format too long"));
    memcpy(&fmt[1],&src[start],((n-start)+1)*sizeof(GSChar));
    fmt[(n-start)+2] = '\0';
    return n;
}

GSRESULT gstd_format(HGameScriptVM v,GSInteger nformatstringidx,GSInteger *outlen,GSChar **output)
{
    const GSChar *format;
    GSChar *dest;
    GSChar fmt[MAX_FORMAT_LEN];
    const GSRESULT res = GS_getstring(v,nformatstringidx,&format);
    if (GS_FAILED(res)) {
        return res; // propagate the error
    }
    GSInteger format_size = GS_getsize(v,nformatstringidx);
    GSInteger allocated = (format_size+2)*sizeof(GSChar);
    dest = GS_getscratchpad(v,allocated);
    GSInteger n = 0,i = 0, nparam = nformatstringidx+1, w = 0;
    //while(format[n] != '\0')
    while(n < format_size)
    {
        if(format[n] != '%') {
            assert(i < allocated);
            dest[i++] = format[n];
            n++;
        }
        else if(format[n+1] == '%') { //handles %%
                dest[i++] = '%';
                n += 2;
        }
        else {
            n++;
            if( nparam > GS_gettop(v) )
                return GS_throwerror(v,_SC("not enough parameters for the given format string"));
            n = validate_format(v,fmt,format,n,w);
            if(n < 0) return -1;
            GSInteger addlen = 0;
            GSInteger valtype = 0;
            const GSChar *ts = NULL;
            GSInteger ti = 0;
            GSFloat tf = 0;
            switch(format[n]) {
            case 's':
                if(GS_FAILED(GS_getstring(v,nparam,&ts)))
                    return GS_throwerror(v,_SC("string expected for the specified format"));
                addlen = (GS_getsize(v,nparam)*sizeof(GSChar))+((w+1)*sizeof(GSChar));
                valtype = 's';
                break;
            case 'i': case 'd': case 'o': case 'u':  case 'x':  case 'X':
#ifdef _GS64
                {
                size_t flen = scstrlen(fmt);
                GSInteger fpos = flen - 1;
                GSChar f = fmt[fpos];
                const GSChar *prec = (const GSChar *)_PRINT_INT_PREC;
                while(*prec != _SC('\0')) {
                    fmt[fpos++] = *prec++;
                }
                fmt[fpos++] = f;
                fmt[fpos++] = _SC('\0');
                }
#endif
            case 'c':
                if(GS_FAILED(GS_getinteger(v,nparam,&ti)))
                    return GS_throwerror(v,_SC("integer expected for the specified format"));
                addlen = (ADDITIONAL_FORMAT_SPACE)+((w+1)*sizeof(GSChar));
                valtype = 'i';
                break;
            case 'f': case 'g': case 'G': case 'e':  case 'E':
                if(GS_FAILED(GS_getfloat(v,nparam,&tf)))
                    return GS_throwerror(v,_SC("float expected for the specified format"));
                addlen = (ADDITIONAL_FORMAT_SPACE)+((w+1)*sizeof(GSChar));
                valtype = 'f';
                break;
            default:
                return GS_throwerror(v,_SC("invalid format"));
            }
            n++;
            allocated += addlen + sizeof(GSChar);
            dest = GS_getscratchpad(v,allocated);
            switch(valtype) {
            case 's': i += scsprintf(&dest[i],allocated,fmt,ts); break;
            case 'i': i += scsprintf(&dest[i],allocated,fmt,ti); break;
            case 'f': i += scsprintf(&dest[i],allocated,fmt,tf); break;
            };
            nparam ++;
        }
    }
    *outlen = i;
    dest[i] = '\0';
    *output = dest;
    return GS_OK;
}

void gstd_pushstringf(HGameScriptVM v,const GSChar *s,...)
{
    GSInteger n=256;
    va_list args;
begin:
    va_start(args,s);
    GSChar *b=GS_getscratchpad(v,n);
    GSInteger r=scvsprintf(b,n,s,args);
    va_end(args);
    if (r>=n) {
        n=r+1;//required+null
        goto begin;
    } else if (r<0) {
        GS_pushnull(v);
    } else {
        GS_pushstring(v,b,r);
    }
}

static GSInteger _string_printf(HGameScriptVM v)
{
    GSChar *dest = NULL;
    GSInteger length = 0;
    if(GS_FAILED(gstd_format(v,2,&length,&dest)))
        return -1;

    GSPRINTFUNCTION printfunc = GS_getprintfunc(v);
    if(printfunc) printfunc(v,_SC("%s"),dest);

    return 0;
}

static GSInteger _string_format(HGameScriptVM v)
{
    GSChar *dest = NULL;
    GSInteger length = 0;
    if(GS_FAILED(gstd_format(v,2,&length,&dest)))
        return -1;
    GS_pushstring(v,dest,length);
    return 1;
}

static void __strip_l(const GSChar *str,const GSChar **start)
{
    const GSChar *t = str;
    while(((*t) != '\0') && scisspace(*t)){ t++; }
    *start = t;
}

static void __strip_r(const GSChar *str,GSInteger len,const GSChar **end)
{
    if(len == 0) {
        *end = str;
        return;
    }
    const GSChar *t = &str[len-1];
    while(t >= str && scisspace(*t)) { t--; }
    *end = t + 1;
}

static GSInteger _string_strip(HGameScriptVM v)
{
    const GSChar *str,*start,*end;
    GS_getstring(v,2,&str);
    GSInteger len = GS_getsize(v,2);
    __strip_l(str,&start);
    __strip_r(str,len,&end);
    GS_pushstring(v,start,end - start);
    return 1;
}

static GSInteger _string_lstrip(HGameScriptVM v)
{
    const GSChar *str,*start;
    GS_getstring(v,2,&str);
    __strip_l(str,&start);
    GS_pushstring(v,start,-1);
    return 1;
}

static GSInteger _string_rstrip(HGameScriptVM v)
{
    const GSChar *str,*end;
    GS_getstring(v,2,&str);
    GSInteger len = GS_getsize(v,2);
    __strip_r(str,len,&end);
    GS_pushstring(v,str,end - str);
    return 1;
}

static GSInteger _string_split(HGameScriptVM v)
{
    const GSChar *str,*seps;
    GSInteger sepsize;
    GSBool skipempty = GSFalse;
    GS_getstring(v,2,&str);
    GS_getstringandsize(v,3,&seps,&sepsize);
    if(sepsize == 0) return GS_throwerror(v,_SC("empty separators string"));
    if(GS_gettop(v)>3) {
        GS_getbool(v,4,&skipempty);
    }
    const GSChar *start = str;
    const GSChar *end = str;
    GS_newarray(v,0);
    while(*end != '\0')
    {
        GSChar cur = *end;
        for(GSInteger i = 0; i < sepsize; i++)
        {
            if(cur == seps[i])
            {
                if(!skipempty || (end != start)) {
                    GS_pushstring(v,start,end-start);
                    GS_arrayappend(v,-2);
                }
                start = end + 1;
                break;
            }
        }
        end++;
    }
    if(end != start)
    {
        GS_pushstring(v,start,end-start);
        GS_arrayappend(v,-2);
    }
    return 1;
}

static GSInteger _string_escape(HGameScriptVM v)
{
    const GSChar *str;
    GSChar *dest,*resstr;
    GSInteger size;
    GS_getstring(v,2,&str);
    size = GS_getsize(v,2);
    if(size == 0) {
        GS_push(v,2);
        return 1;
    }
#ifdef GSUNICODE
#if WCHAR_SIZE == 2
    const GSChar *escpat = _SC("\\x%04x");
    const GSInteger maxescsize = 6;
#else //WCHAR_SIZE == 4
    const GSChar *escpat = _SC("\\x%08x");
    const GSInteger maxescsize = 10;
#endif
#else
    const GSChar *escpat = _SC("\\x%02x");
    const GSInteger maxescsize = 4;
#endif
    GSInteger destcharsize = (size * maxescsize); //assumes every char could be escaped
    resstr = dest = (GSChar *)GS_getscratchpad(v,destcharsize * sizeof(GSChar));
    GSChar c;
    GSChar escch;
    GSInteger escaped = 0;
    for(int n = 0; n < size; n++){
        c = *str++;
        escch = 0;
        if(scisprint(c) || c == 0) {
            switch(c) {
            case '\a': escch = 'a'; break;
            case '\b': escch = 'b'; break;
            case '\t': escch = 't'; break;
            case '\n': escch = 'n'; break;
            case '\v': escch = 'v'; break;
            case '\f': escch = 'f'; break;
            case '\r': escch = 'r'; break;
            case '\\': escch = '\\'; break;
            case '\"': escch = '\"'; break;
            case '\'': escch = '\''; break;
            case 0: escch = '0'; break;
            }
            if(escch) {
                *dest++ = '\\';
                *dest++ = escch;
                escaped++;
            }
            else {
                *dest++ = c;
            }
        }
        else {

            dest += scsprintf(dest, destcharsize, escpat, c);
            escaped++;
        }
    }

    if(escaped) {
        GS_pushstring(v,resstr,dest - resstr);
    }
    else {
        GS_push(v,2); //nothing escaped
    }
    return 1;
}

static GSInteger _string_startswith(HGameScriptVM v)
{
    const GSChar *str,*cmp;
    GS_getstring(v,2,&str);
    GS_getstring(v,3,&cmp);
    GSInteger len = GS_getsize(v,2);
    GSInteger cmplen = GS_getsize(v,3);
    GSBool ret = GSFalse;
    if(cmplen <= len) {
        ret = memcmp(str,cmp,GS_rsl(cmplen)) == 0 ? GSTrue : GSFalse;
    }
    GS_pushbool(v,ret);
    return 1;
}

static GSInteger _string_endswith(HGameScriptVM v)
{
    const GSChar *str,*cmp;
    GS_getstring(v,2,&str);
    GS_getstring(v,3,&cmp);
    GSInteger len = GS_getsize(v,2);
    GSInteger cmplen = GS_getsize(v,3);
    GSBool ret = GSFalse;
    if(cmplen <= len) {
        ret = memcmp(&str[len - cmplen],cmp,GS_rsl(cmplen)) == 0 ? GSTrue : GSFalse;
    }
    GS_pushbool(v,ret);
    return 1;
}

#define SETUP_REX(v) \
    GSRex *self = NULL; \
    if(GS_FAILED(GS_getinstanceup(v,1,(GSUserPointer *)&self,rex_typetag,GSFalse))) { \
		return GS_throwerror(v,_SC("invalid type tag")); \
	}

static GSInteger _rexobj_releasehook(GSUserPointer p, GSInteger GS_UNUSED_ARG(size))
{
    GSRex *self = ((GSRex *)p);
    gstd_rex_free(self);
    return 1;
}

static GSInteger _regexp_match(HGameScriptVM v)
{
    SETUP_REX(v);
    const GSChar *str;
    GS_getstring(v,2,&str);
    if(gstd_rex_match(self,str) == GSTrue)
    {
        GS_pushbool(v,GSTrue);
        return 1;
    }
    GS_pushbool(v,GSFalse);
    return 1;
}

static void _addrexmatch(HGameScriptVM v,const GSChar *str,const GSChar *begin,const GSChar *end)
{
    GS_newtable(v);
    GS_pushstring(v,_SC("begin"),-1);
    GS_pushinteger(v,begin - str);
    GS_rawset(v,-3);
    GS_pushstring(v,_SC("end"),-1);
    GS_pushinteger(v,end - str);
    GS_rawset(v,-3);
}

static GSInteger _regexp_search(HGameScriptVM v)
{
    SETUP_REX(v);
    const GSChar *str,*begin,*end;
    GSInteger start = 0;
    GS_getstring(v,2,&str);
    if(GS_gettop(v) > 2) GS_getinteger(v,3,&start);
    if(gstd_rex_search(self,str+start,&begin,&end) == GSTrue) {
        _addrexmatch(v,str,begin,end);
        return 1;
    }
    return 0;
}

static GSInteger _regexp_capture(HGameScriptVM v)
{
    SETUP_REX(v);
    const GSChar *str,*begin,*end;
    GSInteger start = 0;
    GS_getstring(v,2,&str);
    if(GS_gettop(v) > 2) GS_getinteger(v,3,&start);
    if(gstd_rex_search(self,str+start,&begin,&end) == GSTrue) {
        GSInteger n = gstd_rex_getsubexpcount(self);
        GSRexMatch match;
        GS_newarray(v,0);
        for(GSInteger i = 0;i < n; i++) {
            gstd_rex_getsubexp(self,i,&match);
            if(match.len > 0)
                _addrexmatch(v,str,match.begin,match.begin+match.len);
            else
                _addrexmatch(v,str,str,str); //empty match
            GS_arrayappend(v,-2);
        }
        return 1;
    }
    return 0;
}

static GSInteger _regexp_subexpcount(HGameScriptVM v)
{
    SETUP_REX(v);
    GS_pushinteger(v,gstd_rex_getsubexpcount(self));
    return 1;
}

static GSInteger _regexp_constructor(HGameScriptVM v)
{
	GSRex *self = NULL;
	if (GS_FAILED(GS_getinstanceup(v, 1, (GSUserPointer *)&self, rex_typetag, GSFalse))) {
		return GS_throwerror(v, _SC("invalid type tag"));
	}
	if (self != NULL) {
		return GS_throwerror(v, _SC("invalid regexp object"));
	}
    const GSChar *error,*pattern;
    GS_getstring(v,2,&pattern);
    GSRex *rex = gstd_rex_compile(pattern,&error);
    if(!rex) return GS_throwerror(v,error);
    GS_setinstanceup(v,1,rex);
    GS_setreleasehook(v,1,_rexobj_releasehook);
    return 0;
}

static GSInteger _regexp__typeof(HGameScriptVM v)
{
    GS_pushstring(v,_SC("regexp"),-1);
    return 1;
}

#define _DECL_REX_FUNC(name,nparams,pmask) {_SC(#name),_regexp_##name,nparams,pmask}
static const GSRegFunction rexobj_funcs[]={
    _DECL_REX_FUNC(constructor,2,_SC(".s")),
    _DECL_REX_FUNC(search,-2,_SC("xsn")),
    _DECL_REX_FUNC(match,2,_SC("xs")),
    _DECL_REX_FUNC(capture,-2,_SC("xsn")),
    _DECL_REX_FUNC(subexpcount,1,_SC("x")),
    _DECL_REX_FUNC(_typeof,1,_SC("x")),
    {NULL,(GSFUNCTION)0,0,NULL}
};
#undef _DECL_REX_FUNC

#define _DECL_FUNC(name,nparams,pmask) {_SC(#name),_string_##name,nparams,pmask}
static const GSRegFunction stringlib_funcs[]={
    _DECL_FUNC(format,-2,_SC(".s")),
    _DECL_FUNC(printf,-2,_SC(".s")),
    _DECL_FUNC(strip,2,_SC(".s")),
    _DECL_FUNC(lstrip,2,_SC(".s")),
    _DECL_FUNC(rstrip,2,_SC(".s")),
    _DECL_FUNC(split,-3,_SC(".ssb")),
    _DECL_FUNC(escape,2,_SC(".s")),
    _DECL_FUNC(startswith,3,_SC(".ss")),
    _DECL_FUNC(endswith,3,_SC(".ss")),
    {NULL,(GSFUNCTION)0,0,NULL}
};
#undef _DECL_FUNC


GSInteger gstd_register_stringlib(HGameScriptVM v)
{
    GS_pushstring(v,_SC("regexp"),-1);
    GS_newclass(v,GSFalse);
	rex_typetag = (GSUserPointer)rexobj_funcs;
	GS_settypetag(v, -1, rex_typetag);
    GSInteger i = 0;
    while(rexobj_funcs[i].name != 0) {
        const GSRegFunction &f = rexobj_funcs[i];
        GS_pushstring(v,f.name,-1);
        GS_newclosure(v,f.f,0);
        GS_setparamscheck(v,f.nparamscheck,f.typemask);
        GS_setnativeclosurename(v,-1,f.name);
        GS_newslot(v,-3,GSFalse);
        i++;
    }
    GS_newslot(v,-3,GSFalse);

    i = 0;
    while(stringlib_funcs[i].name!=0)
    {
        GS_pushstring(v,stringlib_funcs[i].name,-1);
        GS_newclosure(v,stringlib_funcs[i].f,0);
        GS_setparamscheck(v,stringlib_funcs[i].nparamscheck,stringlib_funcs[i].typemask);
        GS_setnativeclosurename(v,-1,stringlib_funcs[i].name);
        GS_newslot(v,-3,GSFalse);
        i++;
    }
    return 1;
}

