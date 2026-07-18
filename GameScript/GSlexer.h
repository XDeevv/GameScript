/*  see copyright notice in GameScript.h */
#ifndef _GSLEXER_H_
#define _GSLEXER_H_

#ifdef GSUNICODE
typedef GSChar LexChar;
#else
typedef unsigned char LexChar;
#endif

struct GSLexer
{
    GSLexer();
    ~GSLexer();
    void Init(GSSharedState *ss,GSLEXREADFUNC rg,GSUserPointer up,CompilerErrorFunc efunc,void *ed);
    void Error(const GSChar *err);
    GSInteger Lex();
    const GSChar *Tok2Str(GSInteger tok);
private:
    GSInteger GetIDType(const GSChar *s,GSInteger len);
    GSInteger ReadString(GSInteger ndelim,bool verbatim);
    GSInteger ReadNumber();
    void LexBlockComment();
    void LexLineComment();
    GSInteger ReadID();
    void Next();
#ifdef GSUNICODE
#if WCHAR_SIZE == 2
    GSInteger AddUTF16(GSUnsignedInteger ch);
#endif
#else
    GSInteger AddUTF8(GSUnsignedInteger ch);
#endif
    GSInteger ProcessStringHexEscape(GSChar *dest, GSInteger maxdigits);
    GSInteger _curtoken;
    GSTable *_keywords;
    GSBool _reached_eof;
public:
    GSInteger _prevtoken;
    GSInteger _currentline;
    GSInteger _lasttokenline;
    GSInteger _currentcolumn;
    const GSChar *_svalue;
    GSInteger _nvalue;
    GSFloat _fvalue;
    GSLEXREADFUNC _readf;
    GSUserPointer _up;
    LexChar _currdata;
    GSSharedState *_sharedstate;
    GSvector<GSChar> _longstr;
    CompilerErrorFunc _errfunc;
    void *_errtarget;
};

#endif

