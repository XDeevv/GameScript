
#ifdef _GS64

#ifdef _MSC_VER
typedef __int64 GSInteger;
typedef unsigned __int64 GSUnsignedInteger;
typedef unsigned __int64 GSHash; /*should be the same size of a pointer*/
#else
typedef long long GSInteger;
typedef unsigned long long GSUnsignedInteger;
typedef unsigned long long GSHash; /*should be the same size of a pointer*/
#endif
typedef int GSInt32;
typedef unsigned int GSUnsignedInteger32;
#else
typedef int GSInteger;
typedef int GSInt32; /*must be 32 bits(also on 64bits processors)*/
typedef unsigned int GSUnsignedInteger32; /*must be 32 bits(also on 64bits processors)*/
typedef unsigned int GSUnsignedInteger;
typedef unsigned int GSHash; /*should be the same size of a pointer*/
#endif


#ifdef GSUSEDOUBLE
typedef double GSFloat;
#else
typedef float GSFloat;
#endif

#if defined(GSUSEDOUBLE) && !defined(_GS64) || !defined(GSUSEDOUBLE) && defined(_GS64)
#ifdef _MSC_VER
typedef __int64 GSRawObjectVal; //must be 64bits
#else
typedef long long GSRawObjectVal; //must be 64bits
#endif
#define GS_OBJECT_RAWINIT() { _unVal.raw = 0; }
#else
typedef GSUnsignedInteger GSRawObjectVal; //is 32 bits on 32 bits builds and 64 bits otherwise
#define GS_OBJECT_RAWINIT()
#endif

#ifndef GS_ALIGNMENT // GS_ALIGNMENT shall be less than or equal to GS_MALLOC alignments, and its value shall be power of 2.
#if defined(GSUSEDOUBLE) || defined(_GS64)
#define GS_ALIGNMENT 8
#else
#define GS_ALIGNMENT 4
#endif
#endif

typedef void* GSUserPointer;
typedef GSUnsignedInteger GSBool;
typedef GSInteger GSRESULT;

#ifdef GSUNICODE
#include <wchar.h>
#include <wctype.h>


typedef wchar_t GSChar;


#define scstrcmp    wcscmp
#ifdef _WIN32
#define scsprintf   _snwprintf
#else
#define scsprintf   swprintf
#endif
#define scstrlen    wcslen
#define scstrtod    wcstod
#ifdef _GS64
#define scstrtol    wcstoll
#else
#define scstrtol    wcstol
#endif
#define scstrtoul   wcstoul
#define scvsprintf  vswprintf
#define scstrstr    wcsstr
#define scprintf    wprintf

#ifdef _WIN32
#define WCHAR_SIZE 2
#define WCHAR_SHIFT_MUL 1
#define MAX_CHAR 0xFFFF
#else
#define WCHAR_SIZE 4
#define WCHAR_SHIFT_MUL 2
#define MAX_CHAR 0xFFFFFFFF
#endif

#define _SC(a) L##a


#define scisspace   iswspace
#define scisdigit   iswdigit
#define scisprint   iswprint
#define scisxdigit  iswxdigit
#define scisalpha   iswalpha
#define sciscntrl   iswcntrl
#define scisalnum   iswalnum


#define GS_rsl(l) ((l)<<WCHAR_SHIFT_MUL)

#else
typedef char GSChar;
#define _SC(a) a
#define scstrcmp    strcmp
#ifdef _MSC_VER
#define scsprintf   _snprintf
#else
#define scsprintf   snprintf
#endif
#define scstrlen    strlen
#define scstrtod    strtod
#ifdef _GS64
#ifdef _MSC_VER
#define scstrtol    _strtoi64
#else
#define scstrtol    strtoll
#endif
#else
#define scstrtol    strtol
#endif
#define scstrtoul   strtoul
#define scvsprintf  vsnprintf
#define scstrstr    strstr
#define scisspace   isspace
#define scisdigit   isdigit
#define scisprint   isprint
#define scisxdigit  isxdigit
#define sciscntrl   iscntrl
#define scisalpha   isalpha
#define scisalnum   isalnum
#define scprintf    printf
#define MAX_CHAR 0xFF

#define GS_rsl(l) (l)

#endif

#ifdef _GS64
#define _PRINT_INT_PREC _SC("ll")
#define _PRINT_INT_FMT _SC("%lld")
#else
#define _PRINT_INT_FMT _SC("%d")
#endif

