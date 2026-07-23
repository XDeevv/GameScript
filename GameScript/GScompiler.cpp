/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#ifndef NO_COMPILER
#include <stdarg.h>
#include <setjmp.h>
#include "GSopcodes.h"
#include "GSstring.h"
#include "GSfuncproto.h"
#include "GScompiler.h"
#include "GSfuncstate.h"
#include "GSlexer.h"
#include "GSvm.h"
#include "GStable.h"

#include <vector>

#define EXPR   1
#define OBJECT 2
#define BASE   3
#define LOCAL  4
#define OUTER  5

struct GSExpState {
  GSInteger  etype;       /* expr. type; one of EXPR, OBJECT, BASE, OUTER or LOCAL */
  GSInteger  epos;        /* expr. location on stack; -1 for OBJECT and BASE */
  bool       donot_get;   /* signal not to deref the next value */
};

#define MAX_COMPILER_ERROR_LEN 256

struct GSScope {
    GSInteger outers;
    GSInteger stacksize;
};

#define BEGIN_SCOPE() GSScope __oldscope__ = _scope; \
                     _scope.outers = _fs->_outers; \
                     _scope.stacksize = _fs->GetStackSize();

#define RESOLVE_OUTERS() if(_fs->GetStackSize() != _scope.stacksize) { \
                            if(_fs->CountOuters(_scope.stacksize)) { \
                                _fs->AddInstruction(_OP_CLOSE,0,_scope.stacksize); \
                            } \
                        }

#define END_SCOPE_NO_CLOSE() {  if(_fs->GetStackSize() != _scope.stacksize) { \
                            _fs->SetStackSize(_scope.stacksize); \
                        } \
                        _scope = __oldscope__; \
                    }

#define END_SCOPE() {   GSInteger oldouters = _fs->_outers;\
                        if(_fs->GetStackSize() != _scope.stacksize) { \
                            _fs->SetStackSize(_scope.stacksize); \
                            if(oldouters != _fs->_outers) { \
                                _fs->AddInstruction(_OP_CLOSE,0,_scope.stacksize); \
                            } \
                        } \
                        _scope = __oldscope__; \
                    }

#define BEGIN_BREAKBLE_BLOCK()  GSInteger __nbreaks__=_fs->_unresolvedbreaks.size(); \
                            GSInteger __ncontinues__=_fs->_unresolvedcontinues.size(); \
                            _fs->_breaktargets.push_back(0);_fs->_continuetargets.push_back(0);

#define END_BREAKBLE_BLOCK(continue_target) {__nbreaks__=_fs->_unresolvedbreaks.size()-__nbreaks__; \
                    __ncontinues__=_fs->_unresolvedcontinues.size()-__ncontinues__; \
                    if(__ncontinues__>0)ResolveContinues(_fs,__ncontinues__,continue_target); \
                    if(__nbreaks__>0)ResolveBreaks(_fs,__nbreaks__); \
                    _fs->_breaktargets.pop_back();_fs->_continuetargets.pop_back();}

class GSCompiler
{
public:
    GSCompiler(GSVM *v, GSLEXREADFUNC rg, GSUserPointer up, const GSChar* sourcename, bool raiseerror, bool lineinfo)
    {
        _vm=v;
        _lex.Init(_ss(v), rg, up,ThrowError,this);
        _sourcename = GSString::Create(_ss(v), sourcename);
        _lineinfo = lineinfo;_raiseerror = raiseerror;
        _scope.outers = 0;
        _scope.stacksize = 0;
        _compilererror[0] = _SC('\0');
    }
    static void ThrowError(void *ud, const GSChar *s) {
        GSCompiler *c = (GSCompiler *)ud;
        c->Error(s);
    }
    void Error(const GSChar *s, ...)
    {
        va_list vl;
        va_start(vl, s);
        scvsprintf(_compilererror, MAX_COMPILER_ERROR_LEN, s, vl);
        va_end(vl);
        longjmp(_errorjmp,1);
    }
    void Lex(){ _token = _lex.Lex();}
    GSObject Expect(GSInteger tok)
    {

        if(_token != tok) {
            if(_token == TK_CONSTRUCTOR && tok == TK_IDENTIFIER) {
                //do nothing
            }
            else {
                const GSChar *etypename;
                if(tok > 255) {
                    switch(tok)
                    {
                    case TK_IDENTIFIER:
                        etypename = _SC("IDENTIFIER");
                        break;
                    case TK_STRING_LITERAL:
                        etypename = _SC("STRING_LITERAL");
                        break;
                    case TK_INTEGER:
                        etypename = _SC("INTEGER");
                        break;
                    case TK_FLOAT:
                        etypename = _SC("FLOAT");
                        break;
                    default:
                        etypename = _lex.Tok2Str(tok);
                    }
                    Error(_SC("expected '%s'"), etypename);
                }
                Error(_SC("expected '%c'"), tok);
            }
        }
        GSObjectPtr ret;
        switch(tok)
        {
        case TK_IDENTIFIER:
            ret = _fs->CreateString(_lex._svalue);
            break;
        case TK_STRING_LITERAL:
            ret = _fs->CreateString(_lex._svalue,_lex._longstr.size()-1);
            break;
        case TK_INTEGER:
            ret = GSObjectPtr(_lex._nvalue);
            break;
        case TK_FLOAT:
            ret = GSObjectPtr(_lex._fvalue);
            break;
        }
        Lex();
        return ret;
    }
    bool IsEndOfStatement() { return ((_lex._prevtoken == _SC('\n')) || (_token == GameScript_EOB) || (_token == _SC('}')) || (_token == _SC(';'))); }
    void OptionalSemicolon()
    {
        if(_token == _SC(';')) { Lex(); return; }
        if(!IsEndOfStatement()) {
            Error(_SC("end of statement expected (; or lf)"));
        }
    }
    void MoveIfCurrentTargetIsLocal() {
        GSInteger trg = _fs->TopTarget();
        if(_fs->IsLocal(trg)) {
            trg = _fs->PopTarget(); //pops the target and moves it
            _fs->AddInstruction(_OP_MOVE, _fs->PushTarget(), trg);
        }
    }
    bool Compile(GSObjectPtr &o)
    {
        _debugline = 1;
        _debugop = 0;

        GSFuncState funcstate(_ss(_vm), NULL,ThrowError,this);
        funcstate._name = GSString::Create(_ss(_vm), _SC("main"));
        _fs = &funcstate;
        _fs->AddParameter(_fs->CreateString(_SC("this")));
        _fs->AddParameter(_fs->CreateString(_SC("vargv")));
        _fs->_varparams = true;
        _fs->_sourcename = _sourcename;
        GSInteger stacksize = _fs->GetStackSize();
        if(setjmp(_errorjmp) == 0) {
            Lex();
            while(_token > 0){
                Statement();
                if(_lex._prevtoken != _SC('}') && _lex._prevtoken != _SC(';')) OptionalSemicolon();
            }
            _fs->SetStackSize(stacksize);
            _fs->AddLineInfos(_lex._currentline, _lineinfo, true);
            _fs->AddInstruction(_OP_RETURN, 0xFF);
            _fs->SetStackSize(0);
            o =_fs->BuildProto();
#ifdef _DEBUG_DUMP
            _fs->Dump(_funcproto(o));
#endif
        }
        else {
            if(_raiseerror && _ss(_vm)->_compilererrorhandler) {
                _ss(_vm)->_compilererrorhandler(_vm, _compilererror, GS_type(_sourcename) == OT_STRING?_stringval(_sourcename):_SC("unknown"),
                    _lex._currentline, _lex._currentcolumn);
            }
            _vm->_lasterror = GSString::Create(_ss(_vm), _compilererror, -1);
            return false;
        }
        return true;
    }
    void Statements()
    {
        while(_token != _SC('}') && _token != TK_DEFAULT && _token != TK_CASE) {
            Statement();
            if(_lex._prevtoken != _SC('}') && _lex._prevtoken != _SC(';')) OptionalSemicolon();
        }
    }
    void Statement(bool closeframe = true)
    {
        _fs->AddLineInfos(_lex._currentline, _lineinfo);
        switch(_token){
        case _SC(';'):  Lex();                  break;
        case TK_IF:     IfStatement();          break;
        case TK_WHILE:      WhileStatement();       break;
        case TK_DO:     DoWhileStatement();     break;
        case TK_FOR:        ForStatement();         break;
        case TK_FOREACH:    ForEachStatement();     break;
        case TK_SWITCH: SwitchStatement();      break;
        case TK_LOCAL:      LocalDeclStatement();   break;
        case TK_RETURN:
        case TK_YIELD: {
            GSOpcode op;
            if(_token == TK_RETURN) {
                op = _OP_RETURN;
            }
            else {
                op = _OP_YIELD;
                _fs->_bgenerator = true;
            }
            Lex();
            if(!IsEndOfStatement()) {
                GSInteger retexp = _fs->GetCurrentPos()+1;
                CommaExpr();
                if(op == _OP_RETURN && _fs->_traps > 0)
                    _fs->AddInstruction(_OP_POPTRAP, _fs->_traps, 0);
                _fs->_returnexp = retexp;
                _fs->AddInstruction(op, 1, _fs->PopTarget(),_fs->GetStackSize());
            }
            else{
                if(op == _OP_RETURN && _fs->_traps > 0)
                    _fs->AddInstruction(_OP_POPTRAP, _fs->_traps ,0);
                _fs->_returnexp = -1;
                _fs->AddInstruction(op, 0xFF,0,_fs->GetStackSize());
            }
            break;}
        case TK_BREAK:
            if(_fs->_breaktargets.size() <= 0)Error(_SC("'break' has to be in a loop block"));
            if(_fs->_breaktargets.top() > 0){
                _fs->AddInstruction(_OP_POPTRAP, _fs->_breaktargets.top(), 0);
            }
            RESOLVE_OUTERS();
            _fs->AddInstruction(_OP_JMP, 0, -1234);
            _fs->_unresolvedbreaks.push_back(_fs->GetCurrentPos());
            Lex();
            break;
        case TK_CONTINUE:
            if(_fs->_continuetargets.size() <= 0)Error(_SC("'continue' has to be in a loop block"));
            if(_fs->_continuetargets.top() > 0) {
                _fs->AddInstruction(_OP_POPTRAP, _fs->_continuetargets.top(), 0);
            }
            RESOLVE_OUTERS();
            _fs->AddInstruction(_OP_JMP, 0, -1234);
            _fs->_unresolvedcontinues.push_back(_fs->GetCurrentPos());
            Lex();
            break;
        case TK_FUNCTION:
            FunctionStatement();
            break;
        case TK_CLASS:
            ClassStatement();
            break;
        case TK_ENUM:
            EnumStatement();
            break;
        case TK_IMPORT:
            ImportStatement();
            break;
        case TK_NAMESPACE:
            NamespaceStatement();
            break;
        case TK_PUB:
            Lex();
            if (_token == TK_FUNCTION) {
                FunctionStatement(true);
            } else if (_token == TK_CLASS) {
                ClassStatement(true);
            } else if (_token == TK_IDENTIFIER) {
                GSObject id = Expect(TK_IDENTIFIER);
                _fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
                _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_fs->CreateString(_SC("__namespace"))));
                Emit2ArgsOP(_OP_GET); 
                
                _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
                Expect(_SC('='));
                Expression();
                EmitDerefOp(_OP_NEWSLOT);
                _fs->PopTarget();
            } else {
                Error(_SC("expected 'func', 'class', or identifier after 'pub'"));
            }
            break;
        case _SC('{'):{
                BEGIN_SCOPE();
                Lex();
                Statements();
                Expect(_SC('}'));
                if(closeframe) {
                    END_SCOPE();
                }
                else {
                    END_SCOPE_NO_CLOSE();
                }
            }
            break;
        case TK_TRY:
            TryCatchStatement();
            break;
        case TK_THROW:
            Lex();
            CommaExpr();
            _fs->AddInstruction(_OP_THROW, _fs->PopTarget());
            break;
        case TK_CONST:
            {
            Lex();
            GSObject id = Expect(TK_IDENTIFIER);
            Expect('=');
            GSObject val = ExpectScalar();
            OptionalSemicolon();
            GSTable *enums = _table(_ss(_vm)->_consts);
            GSObjectPtr strongid = id;
            enums->NewSlot(strongid,GSObjectPtr(val));
            strongid.Null();
            }
            break;
        default:
            CommaExpr();
            _fs->DiscardTarget();
            //_fs->PopTarget();
            break;
        }
        _fs->SnoozeOpt();
    }
    void EmitDerefOp(GSOpcode op)
    {
        GSInteger val = _fs->PopTarget();
        GSInteger key = _fs->PopTarget();
        GSInteger src = _fs->PopTarget();
        _fs->AddInstruction(op,_fs->PushTarget(),src,key,val);
    }
    void Emit2ArgsOP(GSOpcode op, GSInteger p3 = 0)
    {
        GSInteger p2 = _fs->PopTarget(); //src in OP_GET
        GSInteger p1 = _fs->PopTarget(); //key in OP_GET
        _fs->AddInstruction(op,_fs->PushTarget(), p1, p2, p3);
    }
    void EmitCompoundArith(GSInteger tok, GSInteger etype, GSInteger pos)
    {
        /* Generate code depending on the expression type */
        switch(etype) {
        case LOCAL:{
            GSInteger p2 = _fs->PopTarget(); //src in OP_GET
            GSInteger p1 = _fs->PopTarget(); //key in OP_GET
            _fs->PushTarget(p1);
            //EmitCompArithLocal(tok, p1, p1, p2);
            _fs->AddInstruction(ChooseArithOpByToken(tok),p1, p2, p1, 0);
            _fs->SnoozeOpt();
                   }
            break;
        case OBJECT:
        case BASE:
            {
                GSInteger val = _fs->PopTarget();
                GSInteger key = _fs->PopTarget();
                GSInteger src = _fs->PopTarget();
                /* _OP_COMPARITH mixes dest obj and source val in the arg1 */
                _fs->AddInstruction(_OP_COMPARITH, _fs->PushTarget(), (src<<16)|val, key, ChooseCompArithCharByToken(tok));
            }
            break;
        case OUTER:
            {
                GSInteger val = _fs->TopTarget();
                GSInteger tmp = _fs->PushTarget();
                _fs->AddInstruction(_OP_GETOUTER,   tmp, pos);
                _fs->AddInstruction(ChooseArithOpByToken(tok), tmp, val, tmp, 0);
                _fs->PopTarget();
                _fs->PopTarget();
                _fs->AddInstruction(_OP_SETOUTER, _fs->PushTarget(), pos, tmp);
            }
            break;
        }
    }
    void CommaExpr()
    {
        for(Expression();_token == ',';_fs->PopTarget(), Lex(), CommaExpr());
    }
    void Expression()
    {
         GSExpState es = _es;
        _es.etype     = EXPR;
        _es.epos      = -1;
        _es.donot_get = false;
        LogicalOrExp();
        switch(_token)  {
        case _SC('='):
        case TK_NEWSLOT:
        case TK_MINUSEQ:
        case TK_PLUSEQ:
        case TK_MULEQ:
        case TK_DIVEQ:
        case TK_MODEQ:{
            GSInteger op = _token;
            GSInteger ds = _es.etype;
            GSInteger pos = _es.epos;
            if(ds == EXPR) Error(_SC("can't assign expression"));
            else if(ds == BASE) Error(_SC("'base' cannot be modified"));
            Lex(); Expression();

            switch(op){
            case TK_NEWSLOT:
                if(ds == OBJECT || ds == BASE)
                    EmitDerefOp(_OP_NEWSLOT);
                else //if _derefstate != DEREF_NO_DEREF && DEREF_FIELD so is the index of a local
                    Error(_SC("can't 'create' a local slot"));
                break;
            case _SC('='): //ASSIGN
                switch(ds) {
                    case LOCAL:
                    {
                        GSInteger src = _fs->PopTarget();
                        GSInteger dst = _fs->TopTarget();
                        _fs->AddInstruction(_OP_MOVE, dst, src);

                        for (size_t i = 0; i < _fs->_vlocals.size(); i++) {
                            if (_fs->_vlocals[i]._pos == (GSUnsignedInteger)dst) {
                                if (_fs->_vlocals[i]._type != -1) {
                                    _fs->AddInstruction(_OP_TYPECHECK, dst, _fs->_vlocals[i]._type, _fs->_vlocals[i]._array_size);
                                }
                                break;
                            }
                        }
                    }
                    break;
                case OBJECT:
                case BASE:
                    EmitDerefOp(_OP_SET);
                    break;
                case OUTER:
                    {
                        GSInteger src = _fs->PopTarget();
                        GSInteger dst = _fs->PushTarget();
                        _fs->AddInstruction(_OP_SETOUTER, dst, pos, src);
                    }
                }
                break;
            case TK_MINUSEQ:
            case TK_PLUSEQ:
            case TK_MULEQ:
            case TK_DIVEQ:
            case TK_MODEQ:
                EmitCompoundArith(op, ds, pos);
                break;
            }
            }
            break;
        case _SC('?'): {
            Lex();
            _fs->AddInstruction(_OP_JZ, _fs->PopTarget());
            GSInteger jzpos = _fs->GetCurrentPos();
            GSInteger trg = _fs->PushTarget();
            Expression();
            GSInteger first_exp = _fs->PopTarget();
            if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
            GSInteger endfirstexp = _fs->GetCurrentPos();
            _fs->AddInstruction(_OP_JMP, 0, 0);
            Expect(_SC(':'));
            GSInteger jmppos = _fs->GetCurrentPos();
            Expression();
            GSInteger second_exp = _fs->PopTarget();
            if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
            _fs->SetInstructionParam(jmppos, 1, _fs->GetCurrentPos() - jmppos);
            _fs->SetInstructionParam(jzpos, 1, endfirstexp - jzpos + 1);
            _fs->SnoozeOpt();
            }
            break;
        default:
            break;
        }
        _es = es;
    }
    template<typename T> void INVOKE_EXP(T f)
    {
        GSExpState es = _es;
        _es.etype     = EXPR;
        _es.epos      = -1;
        _es.donot_get = false;
        (this->*f)();
        _es = es;
    }
    template<typename T> void BIN_EXP(GSOpcode op, T f,GSInteger op3 = 0)
    {
        Lex();
        INVOKE_EXP(f);
        GSInteger op1 = _fs->PopTarget();GSInteger op2 = _fs->PopTarget();
        _fs->AddInstruction(op, _fs->PushTarget(), op1, op2, op3);
        _es.etype = EXPR;
    }
    void LogicalOrExp()
    {
        LogicalAndExp();
        for(;;) if(_token == TK_OR) {
            GSInteger first_exp = _fs->PopTarget();
            GSInteger trg = _fs->PushTarget();
            _fs->AddInstruction(_OP_OR, trg, 0, first_exp, 0);
            GSInteger jpos = _fs->GetCurrentPos();
            if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
            Lex(); INVOKE_EXP(&GSCompiler::LogicalOrExp);
            _fs->SnoozeOpt();
            GSInteger second_exp = _fs->PopTarget();
            if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
            _fs->SnoozeOpt();
            _fs->SetInstructionParam(jpos, 1, (_fs->GetCurrentPos() - jpos));
            _es.etype = EXPR;
            break;
        }else return;
    }
    void LogicalAndExp()
    {
        BitwiseOrExp();
        for(;;) switch(_token) {
        case TK_AND: {
            GSInteger first_exp = _fs->PopTarget();
            GSInteger trg = _fs->PushTarget();
            _fs->AddInstruction(_OP_AND, trg, 0, first_exp, 0);
            GSInteger jpos = _fs->GetCurrentPos();
            if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
            Lex(); INVOKE_EXP(&GSCompiler::LogicalAndExp);
            _fs->SnoozeOpt();
            GSInteger second_exp = _fs->PopTarget();
            if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
            _fs->SnoozeOpt();
            _fs->SetInstructionParam(jpos, 1, (_fs->GetCurrentPos() - jpos));
            _es.etype = EXPR;
            break;
            }

        default:
            return;
        }
    }
    void BitwiseOrExp()
    {
        BitwiseXorExp();
        for(;;) if(_token == _SC('|'))
        {BIN_EXP(_OP_BITW, &GSCompiler::BitwiseXorExp,BW_OR);
        }else return;
    }
    void BitwiseXorExp()
    {
        BitwiseAndExp();
        for(;;) if(_token == _SC('^'))
        {BIN_EXP(_OP_BITW, &GSCompiler::BitwiseAndExp,BW_XOR);
        }else return;
    }
    void BitwiseAndExp()
    {
        EqExp();
        for(;;) if(_token == _SC('&'))
        {BIN_EXP(_OP_BITW, &GSCompiler::EqExp,BW_AND);
        }else return;
    }
    void EqExp()
    {
        CompExp();
        for(;;) switch(_token) {
        case TK_EQ: BIN_EXP(_OP_EQ, &GSCompiler::CompExp); break;
        case TK_NE: BIN_EXP(_OP_NE, &GSCompiler::CompExp); break;
        case TK_3WAYSCMP: BIN_EXP(_OP_CMP, &GSCompiler::CompExp,CMP_3W); break;
        default: return;
        }
    }
    void CompExp()
    {
        ShiftExp();
        for(;;) switch(_token) {
        case _SC('>'): BIN_EXP(_OP_CMP, &GSCompiler::ShiftExp,CMP_G); break;
        case _SC('<'): BIN_EXP(_OP_CMP, &GSCompiler::ShiftExp,CMP_L); break;
        case TK_GE: BIN_EXP(_OP_CMP, &GSCompiler::ShiftExp,CMP_GE); break;
        case TK_LE: BIN_EXP(_OP_CMP, &GSCompiler::ShiftExp,CMP_LE); break;
        case TK_IN: BIN_EXP(_OP_EXISTS, &GSCompiler::ShiftExp); break;
        case TK_INSTANCEOF: BIN_EXP(_OP_INSTANCEOF, &GSCompiler::ShiftExp); break;
        default: return;
        }
    }
    void ShiftExp()
    {
        PlusExp();
        for(;;) switch(_token) {
        case TK_USHIFTR: BIN_EXP(_OP_BITW, &GSCompiler::PlusExp,BW_USHIFTR); break;
        case TK_SHIFTL: BIN_EXP(_OP_BITW, &GSCompiler::PlusExp,BW_SHIFTL); break;
        case TK_SHIFTR: BIN_EXP(_OP_BITW, &GSCompiler::PlusExp,BW_SHIFTR); break;
        default: return;
        }
    }
    GSOpcode ChooseArithOpByToken(GSInteger tok)
    {
        switch(tok) {
            case TK_PLUSEQ: case '+': return _OP_ADD;
            case TK_MINUSEQ: case '-': return _OP_SUB;
            case TK_MULEQ: case '*': return _OP_MUL;
            case TK_DIVEQ: case '/': return _OP_DIV;
            case TK_MODEQ: case '%': return _OP_MOD;
            default: assert(0);
        }
        return _OP_ADD;
    }
    GSInteger ChooseCompArithCharByToken(GSInteger tok)
    {
        GSInteger oper;
        switch(tok){
        case TK_MINUSEQ: oper = '-'; break;
        case TK_PLUSEQ: oper = '+'; break;
        case TK_MULEQ: oper = '*'; break;
        case TK_DIVEQ: oper = '/'; break;
        case TK_MODEQ: oper = '%'; break;
        default: oper = 0; //shut up compiler
            assert(0); break;
        };
        return oper;
    }
    void PlusExp()
    {
        MultExp();
        for(;;) switch(_token) {
        case _SC('+'): case _SC('-'):
            BIN_EXP(ChooseArithOpByToken(_token), &GSCompiler::MultExp); break;
        default: return;
        }
    }

    void MultExp()
    {
        PrefixedExpr();
        for(;;) switch(_token) {
        case _SC('*'): case _SC('/'): case _SC('%'):
            BIN_EXP(ChooseArithOpByToken(_token), &GSCompiler::PrefixedExpr); break;
        default: return;
        }
    }
    //if 'pos' != -1 the previous variable is a local variable
    void PrefixedExpr()
    {
        GSInteger pos = Factor();
        for(;;) {
            switch(_token) {
            case _SC('.'):
                pos = -1;
                Lex();

                _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
                if(_es.etype==BASE) {
                    Emit2ArgsOP(_OP_GET);
                    pos = _fs->TopTarget();
                    _es.etype = EXPR;
                    _es.epos   = pos;
                }
                else {
                    if(NeedGet()) {
                        Emit2ArgsOP(_OP_GET);
                    }
                    _es.etype = OBJECT;
                }
                break;
            case _SC('['):
                if(_lex._prevtoken == _SC('\n')) Error(_SC("cannot break deref/or comma needed after [exp]=exp slot declaration"));
                Lex(); Expression(); Expect(_SC(']'));
                pos = -1;
                if(_es.etype==BASE) {
                    Emit2ArgsOP(_OP_GET);
                    pos = _fs->TopTarget();
                    _es.etype = EXPR;
                    _es.epos   = pos;
                }
                else {
                    if(NeedGet()) {
                        Emit2ArgsOP(_OP_GET);
                    }
                    _es.etype = OBJECT;
                }
                break;
            case TK_MINUSMINUS:
            case TK_PLUSPLUS:
                {
                    if(IsEndOfStatement()) return;
                    GSInteger diff = (_token==TK_MINUSMINUS) ? -1 : 1;
                    Lex();
                    switch(_es.etype)
                    {
                        case EXPR: Error(_SC("can't '++' or '--' an expression")); break;
                        case BASE: Error(_SC("'base' cannot be modified")); break;
                        case OBJECT:
                            if(_es.donot_get == true)  { Error(_SC("can't '++' or '--' an expression")); break; } //mmh dor this make sense?
                            Emit2ArgsOP(_OP_PINC, diff);
                            break;
                        case LOCAL: {
                            GSInteger src = _fs->PopTarget();
                            _fs->AddInstruction(_OP_PINCL, _fs->PushTarget(), src, 0, diff);
                                    }
                            break;
                        case OUTER: {
                            GSInteger tmp1 = _fs->PushTarget();
                            GSInteger tmp2 = _fs->PushTarget();
                            _fs->AddInstruction(_OP_GETOUTER, tmp2, _es.epos);
                            _fs->AddInstruction(_OP_PINCL,    tmp1, tmp2, 0, diff);
                            _fs->AddInstruction(_OP_SETOUTER, tmp2, _es.epos, tmp2);
                            _fs->PopTarget();
                        }
                    }
                    _es.etype = EXPR;
                }
                return;
                break;
            case _SC('<'): {
                Lex(); // consume '<'
                do {
                    if (_token != TK_INT && _token != TK_FLOAT && _token != TK_STRING && 
                        _token != TK_BOOL && _token != TK_VOID && _token != TK_IDENTIFIER) {
                        Error(_SC("expected type name in generic argument list"));
                    }
                    Lex(); // consume type token
                    if (_token == _SC(',')) {
                        Lex();
                    } else {
                        break;
                    }
                } while (1);
                Expect(_SC('>')); // consume '>'
                break;
            }
            case _SC('('):
                switch(_es.etype) {
                    case OBJECT: {
                        GSInteger key     = _fs->PopTarget();  /* location of the key */
                        GSInteger table   = _fs->PopTarget();  /* location of the object */
                        GSInteger closure = _fs->PushTarget(); /* location for the closure */
                        GSInteger ttarget = _fs->PushTarget(); /* location for 'this' pointer */
                        _fs->AddInstruction(_OP_PREPCALL, closure, key, table, ttarget);
                        }
                        break;
                    case BASE:
                        _fs->AddInstruction(_OP_MOVE, _fs->PushTarget(), 0);
                        break;
                    case OUTER:
                        _fs->AddInstruction(_OP_GETOUTER, _fs->PushTarget(), _es.epos);
                        _fs->AddInstruction(_OP_MOVE,     _fs->PushTarget(), 0);
                        break;
                    default:
                        _fs->AddInstruction(_OP_MOVE, _fs->PushTarget(), 0);
                }
                _es.etype = EXPR;

                Lex(); // Now safely consume '(' after handling optional <...>
                FunctionCallArgs();
                break;
            default: return;
            }
        }
    }
    GSInteger Factor()
    {
        //_es.etype = EXPR;
        switch(_token)
        {
        case TK_STRING_LITERAL:
            _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_fs->CreateString(_lex._svalue,_lex._longstr.size()-1)));
            Lex();
            break;
        case TK_BASE:
            Lex();
            _fs->AddInstruction(_OP_GETBASE, _fs->PushTarget());
            _es.etype  = BASE;
            _es.epos   = _fs->TopTarget();
            return (_es.epos);
            break;
        case TK_IDENTIFIER:
        case TK_CONSTRUCTOR:
        case TK_THIS:{
                GSObject id;
                GSObject constant;

                switch(_token) {
                    case TK_IDENTIFIER:  id = _fs->CreateString(_lex._svalue);       break;
                    case TK_THIS:        id = _fs->CreateString(_SC("this"),4);        break;
                    case TK_CONSTRUCTOR: id = _fs->CreateString(_SC("constructor"),11); break;
                }

                GSInteger pos = -1;
                Lex();

                if (_token == _SC('<') || _token == '<') {
                    Lex(); // consume '<'
                    do {
                        if (_token != TK_INT && _token != TK_FLOAT && _token != TK_STRING && 
                            _token != TK_BOOL && _token != TK_VOID && _token != TK_IDENTIFIER) {
                            Error(_SC("expected type name in generic argument list"));
                        }
                        Lex(); // consume type token
                        if (_token == _SC(',')) {
                            Lex();
                        } else {
                            break;
                        }
                    } while (1);
                    
                    if (_token == _SC('>') || _token == '>') {
                        Lex(); // consume '>'
                    } else {
                        Error(_SC("expected '>' after generic argument list"));
                    }
                }

                if((pos = _fs->GetLocalVariable(id)) != -1) {
                    /* Handle a local variable (includes 'this') */
                    _fs->PushTarget(pos);
                    _es.etype  = LOCAL;
                    _es.epos   = pos;
                }

                else if((pos = _fs->GetOuterVariable(id)) != -1) {
                    /* Handle a free var */
                    if(NeedGet()) {
                        _es.epos  = _fs->PushTarget();
                        _fs->AddInstruction(_OP_GETOUTER, _es.epos, pos);
                        /* _es.etype = EXPR; already default value */
                    }
                    else {
                        _es.etype = OUTER;
                        _es.epos  = pos;
                    }
                }

                else if(_fs->IsConstant(id, constant)) {
                    /* Handle named constant */
                    GSObjectPtr constval;
                    GSObject    constid;
                    if(GS_type(constant) == OT_TABLE) {
                        Expect('.');
                        constid = Expect(TK_IDENTIFIER);
                        if(!_table(constant)->Get(constid, constval)) {
                            constval.Null();
                            Error(_SC("invalid constant [%s.%s]"), _stringval(id), _stringval(constid));
                        }
                    }
                    else {
                        constval = constant;
                    }
                    _es.epos = _fs->PushTarget();

                    /* generate direct or literal function depending on size */
                    GSObjectType ctype = GS_type(constval);
                    switch(ctype) {
                        case OT_INTEGER: EmitLoadConstInt(_integer(constval),_es.epos); break;
                        case OT_FLOAT: EmitLoadConstFloat(_float(constval),_es.epos); break;
                        case OT_BOOL: _fs->AddInstruction(_OP_LOADBOOL, _es.epos, _integer(constval)); break;
                        default: _fs->AddInstruction(_OP_LOAD,_es.epos,_fs->GetConstant(constval)); break;
                    }
                    _es.etype = EXPR;
                }
                else {
                    /* Handle a non-local variable, aka a field. Push the 'this' pointer on
                    * the virtual stack (always found in offset 0, so no instruction needs to
                    * be generated), and push the key next. Generate an _OP_LOAD instruction
                    * for the latter. If we are not using the variable as a dref expr, generate
                    * the _OP_GET instruction.
                    */
                    _fs->PushTarget(0);
                    _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
                    if(NeedGet()) {
                        Emit2ArgsOP(_OP_GET);
                    }
                    _es.etype = OBJECT;
                }
                return _es.epos;
            }
            break;
        case TK_DOUBLE_COLON:  // "::"
            _fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
            _es.etype = OBJECT;
            _token = _SC('.'); /* hack: drop into PrefixExpr, case '.'*/
            _es.epos = -1;
            return _es.epos;
            break;
        case TK_NULL:
            _fs->AddInstruction(_OP_LOADNULLS, _fs->PushTarget(),1);
            Lex();
            break;
        case TK_INTEGER: EmitLoadConstInt(_lex._nvalue,-1); Lex();  break;
        case TK_FLOAT: EmitLoadConstFloat(_lex._fvalue,-1); Lex(); break;
        case TK_TRUE: case TK_FALSE:
            _fs->AddInstruction(_OP_LOADBOOL, _fs->PushTarget(),_token == TK_TRUE?1:0);
            Lex();
            break;
        case _SC('['): {
                _fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,0,NOT_ARRAY);
                GSInteger apos = _fs->GetCurrentPos(),key = 0;
                Lex();
                while(_token != _SC(']')) {
                    Expression();
                    if(_token == _SC(',')) Lex();
                    GSInteger val = _fs->PopTarget();
                    GSInteger array = _fs->TopTarget();
                    _fs->AddInstruction(_OP_APPENDARRAY, array, val, AAT_STACK);
                    key++;
                }
                _fs->SetInstructionParam(apos, 1, key);
                Lex();
            }
            break;
        case _SC('{'):
            _fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,0,NOT_TABLE);
            Lex();ParseTableOrClass(_SC(','),_SC('}'));
            break;
        case TK_FUNCTION: FunctionExp();break;
        case _SC('@'): FunctionExp(true);break;
        case TK_CLASS: Lex(); ClassExp();break;
        case _SC('-'):
            Lex();
            switch(_token) {
            case TK_INTEGER: EmitLoadConstInt(-_lex._nvalue,-1); Lex(); break;
            case TK_FLOAT: EmitLoadConstFloat(-_lex._fvalue,-1); Lex(); break;
            default: UnaryOP(_OP_NEG);
            }
            break;
        case _SC('!'): Lex(); UnaryOP(_OP_NOT); break;
        case _SC('~'):
            Lex();
            if(_token == TK_INTEGER)  { EmitLoadConstInt(~_lex._nvalue,-1); Lex(); break; }
            UnaryOP(_OP_BWNOT);
            break;
        case TK_TYPEOF : Lex() ;UnaryOP(_OP_TYPEOF); break;
        case TK_RESUME : Lex(); UnaryOP(_OP_RESUME); break;
        case TK_CLONE : Lex(); UnaryOP(_OP_CLONE); break;
        case TK_RAWCALL: Lex(); Expect('('); FunctionCallArgs(true); break;
        case TK_MINUSMINUS :
        case TK_PLUSPLUS :PrefixIncDec(_token); break;
        case TK_DELETE : DeleteExpr(); break;
        case _SC('('): Lex(); CommaExpr(); Expect(_SC(')'));
            break;
        case TK___LINE__: EmitLoadConstInt(_lex._currentline,-1); Lex(); break;
        case TK___FILE__: _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_sourcename)); Lex(); break;
        default: Error(_SC("expression expected"));
        }
        _es.etype = EXPR;
        return -1;
    }
    void EmitLoadConstInt(GSInteger value,GSInteger target)
    {
        if(target < 0) {
            target = _fs->PushTarget();
        }
        if(value <= INT_MAX && value > INT_MIN) { //does it fit in 32 bits?
            _fs->AddInstruction(_OP_LOADINT, target,value);
        }
        else {
            _fs->AddInstruction(_OP_LOAD, target, _fs->GetNumericConstant(value));
        }
    }
    void EmitLoadConstFloat(GSFloat value,GSInteger target)
    {
        if(target < 0) {
            target = _fs->PushTarget();
        }
        if(sizeof(GSFloat) == sizeof(GSInt32)) {
            _fs->AddInstruction(_OP_LOADFLOAT, target,*((GSInt32 *)&value));
        }
        else {
            _fs->AddInstruction(_OP_LOAD, target, _fs->GetNumericConstant(value));
        }
    }
    void UnaryOP(GSOpcode op)
    {
        PrefixedExpr();
        if (_fs->_targetstack.size() == 0)
            Error(_SC("cannot evaluate unary operator"));
        GSInteger src = _fs->PopTarget();
        _fs->AddInstruction(op, _fs->PushTarget(), src);
    }
    bool NeedGet()
    {
        switch(_token) {
        case _SC('='): case _SC('('): case TK_NEWSLOT: case TK_MODEQ: case TK_MULEQ:
        case TK_DIVEQ: case TK_MINUSEQ: case TK_PLUSEQ:
            return false;
        case TK_PLUSPLUS: case TK_MINUSMINUS:
            if (!IsEndOfStatement()) {
                return false;
            }
        break;
        }
        return (!_es.donot_get || ( _es.donot_get && (_token == _SC('.') || _token == _SC('['))));
    }
    void FunctionCallArgs(bool rawcall = false)
    {
        GSInteger nargs = 1;//this
         while(_token != _SC(')')) {
             Expression();
             MoveIfCurrentTargetIsLocal();
             nargs++;
             if(_token == _SC(',')){
                 Lex();
                 if(_token == ')') Error(_SC("expression expected, found ')'"));
             }
         }
         Lex();
         if (rawcall) {
             if (nargs < 3) Error(_SC("rawcall requires at least 2 parameters (callee and this)"));
             nargs -= 2; //removes callee and this from count
         }
         for(GSInteger i = 0; i < (nargs - 1); i++) _fs->PopTarget();
         GSInteger stackbase = _fs->PopTarget();
         GSInteger closure = _fs->PopTarget();
         _fs->AddInstruction(_OP_CALL, _fs->PushTarget(), closure, stackbase, nargs);
		 if (_token == '{')
		 {
			 GSInteger retval = _fs->TopTarget();
			 GSInteger nkeys = 0;
			 Lex();
			 while (_token != '}') {
				 switch (_token) {
				 case _SC('['):
					 Lex(); CommaExpr(); Expect(_SC(']'));
					 Expect(_SC('=')); Expression();
					 break;
				 default:
					 _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
					 Expect(_SC('=')); Expression();
					 break;
				 }
				 if (_token == ',') Lex();
				 nkeys++;
				 GSInteger val = _fs->PopTarget();
				 GSInteger key = _fs->PopTarget();
				 _fs->AddInstruction(_OP_SET, 0xFF, retval, key, val);
			 }
			 Lex();
		 }
    }
    void ParseTableOrClass(GSInteger separator,GSInteger terminator)
    {
        GSInteger tpos = _fs->GetCurrentPos(),nkeys = 0;
        while(_token != terminator) {
            bool hasattrs = false;
            bool isstatic = false;
            //check if is an attribute
            if(separator == ';') {
                if(_token == TK_ATTR_OPEN) {
                    _fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,0,NOT_TABLE); Lex();
                    ParseTableOrClass(',',TK_ATTR_CLOSE);
                    hasattrs = true;
                }
                if(_token == TK_STATIC) {
                    isstatic = true;
                    Lex();
                }
            }
            switch(_token) {
            case TK_FUNCTION:
            case TK_CONSTRUCTOR:{
                GSInteger tk = _token;
                Lex();
                GSObject id = tk == TK_FUNCTION ? Expect(TK_IDENTIFIER) : _fs->CreateString(_SC("constructor"));
				_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
				GSInteger boundtarget = 0xFF;
				if (_token == _SC('[')) {
					boundtarget = ParseBindEnv();
				}
                //Expect(_SC('('));
                
                CreateFunction(id, boundtarget);
                _fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, boundtarget);
                                }
                                break;
            case _SC('['):
                Lex(); CommaExpr(); Expect(_SC(']'));
                Expect(_SC('=')); Expression();
                break;
            case TK_STRING_LITERAL: //JSON
                if(separator == ',') { //only works for tables
                    _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(Expect(TK_STRING_LITERAL)));
                    Expect(_SC(':')); Expression();
                    break;
                }
            default :
                _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
                Expect(_SC('=')); Expression();
            }
            if(_token == separator) Lex();//optional comma/semicolon
            nkeys++;
            GSInteger val = _fs->PopTarget();
            GSInteger key = _fs->PopTarget();
            GSInteger attrs = hasattrs ? _fs->PopTarget():-1;
            ((void)attrs);
            assert((hasattrs && (attrs == key-1)) || !hasattrs);
            unsigned char flags = (hasattrs?NEW_SLOT_ATTRIBUTES_FLAG:0)|(isstatic?NEW_SLOT_STATIC_FLAG:0);
            GSInteger table = _fs->TopTarget(); //<<BECAUSE OF THIS NO COMMON EMIT FUNC IS POSSIBLE
            if(separator == _SC(',')) { //hack recognizes a table from the separator
                _fs->AddInstruction(_OP_NEWSLOT, 0xFF, table, key, val);
            }
            else {
                _fs->AddInstruction(_OP_NEWSLOTA, flags, table, key, val); //this for classes only as it invokes _newmember
            }
        }
        if(separator == _SC(',')) //hack recognizes a table from the separator
            _fs->SetInstructionParam(tpos, 1, nkeys);
        Lex();
    }
    void LocalDeclStatement()
    {
        GSObject varname;
        Lex();
        if( _token == TK_FUNCTION) {
            GSInteger boundtarget = 0xFF;
            Lex();
            varname = Expect(TK_IDENTIFIER);
            if (_token == _SC('[')) {
                boundtarget = ParseBindEnv();
            }
            //Expect(_SC('('));
            CreateFunction(varname,0xFF,false);
            _fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, boundtarget);
            _fs->PopTarget();
            _fs->PushLocalVariable(varname);
            return;
        }

        do {
            varname = Expect(TK_IDENTIFIER);
            
            GSInteger expected_type = -1;
            GSInteger array_size = -1; 
            bool is_array = false;

            if (_token == _SC(':')) {
                Lex(); // consume ':'
                expected_type = _token;
                bool is_valid_type = (_token == TK_INT || _token == TK_FLOAT || _token == TK_STRING || 
                                      _token == TK_BOOL || _token == TK_VOID || _token == TK_IDENTIFIER);
                if (!is_valid_type) {
                    Error(_SC("expected a valid type name after ':'"));
                }
                Lex(); // consume type token

                if (_token == _SC('[')) {
                    is_array = true; 
                    Lex();
                    if (_token == TK_INTEGER) {
                        array_size = _lex._nvalue;
                        if (array_size <= 0) {
                            Error(_SC("type mismatch: fixed array size must be greater than 0"));
                        }
                        Lex();
                    }
                    Expect(_SC(']'));
                }
            }

            if(_token == _SC('=')) {
                Lex(); Expression();
                GSInteger src = _fs->PopTarget();
                GSInteger dest = _fs->PushTarget();
                if (dest != src) {
                    if (_fs->IsLocal(src)) {
                        _fs->SnoozeOpt();
                    }
                    _fs->AddInstruction(_OP_MOVE, dest, src);
                }
                
                if (expected_type != -1) {
                    GSInteger check_type = is_array ? -expected_type : expected_type;
                
                    _fs->AddInstruction(_OP_TYPECHECK, dest, check_type, array_size);
                }
            }
            else{
                if (expected_type != -1) {
                    Error(_SC("strict type declarations must be initialized (cannot default to null)"));
                }
                _fs->AddInstruction(_OP_LOADNULLS, _fs->PushTarget(),1);
            }

            _fs->PopTarget();

            GSInteger check_type = (expected_type != -1) ? (is_array ? -expected_type : expected_type) : -1;
            _fs->PushLocalVariable(varname, check_type, array_size);
            
            // FORCES THE TYPE METADATA TO SAVE (kinda hacky but please dont remove)
            if (_fs->_vlocals.size() > 0) {
                size_t last_idx = _fs->_vlocals.size() - 1;
                _fs->_vlocals[last_idx]._type = check_type;
                _fs->_vlocals[last_idx]._array_size = array_size;
            }

            if(_token == _SC(',')) Lex(); else break;
        } while(1);
    }
    void IfBlock()
    {
        if (_token == _SC('{'))
        {
            BEGIN_SCOPE();
            Lex();
            Statements();
            Expect(_SC('}'));
            if (true) {
                END_SCOPE();
            }
            else {
                END_SCOPE_NO_CLOSE();
            }
        }
        else {
            Statement();
            if (_lex._prevtoken != _SC('}') && _lex._prevtoken != _SC(';')) OptionalSemicolon();
        }
    }
    void IfStatement()
    {
        GSInteger jmppos;
        bool haselse = false;
        Lex(); Expect(_SC('(')); CommaExpr(); Expect(_SC(')'));
        _fs->AddInstruction(_OP_JZ, _fs->PopTarget());
        GSInteger jnepos = _fs->GetCurrentPos();

        IfBlock();
      
        GSInteger endifblock = _fs->GetCurrentPos();
        if(_token == TK_ELSE){
            haselse = true;
            _fs->AddInstruction(_OP_JMP);
            jmppos = _fs->GetCurrentPos();
            Lex();
            IfBlock();
            _fs->SetInstructionParam(jmppos, 1, _fs->GetCurrentPos() - jmppos);
        }
        _fs->SetInstructionParam(jnepos, 1, endifblock - jnepos + (haselse?1:0));
    }
    void WhileStatement()
    {
        GSInteger jzpos, jmppos;
        jmppos = _fs->GetCurrentPos();
        Lex(); Expect(_SC('(')); CommaExpr(); Expect(_SC(')'));

        BEGIN_BREAKBLE_BLOCK();
        _fs->AddInstruction(_OP_JZ, _fs->PopTarget());
        jzpos = _fs->GetCurrentPos();
        BEGIN_SCOPE();

        Statement();

        END_SCOPE();
        _fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1);
        _fs->SetInstructionParam(jzpos, 1, _fs->GetCurrentPos() - jzpos);

        END_BREAKBLE_BLOCK(jmppos);
    }
    void DoWhileStatement()
    {
        Lex();
        GSInteger jmptrg = _fs->GetCurrentPos();
        BEGIN_BREAKBLE_BLOCK()
        BEGIN_SCOPE();
        Statement();
        END_SCOPE();
        Expect(TK_WHILE);
        GSInteger continuetrg = _fs->GetCurrentPos();
        Expect(_SC('(')); CommaExpr(); Expect(_SC(')'));
        _fs->AddInstruction(_OP_JZ, _fs->PopTarget(), 1);
        _fs->AddInstruction(_OP_JMP, 0, jmptrg - _fs->GetCurrentPos() - 1);
        END_BREAKBLE_BLOCK(continuetrg);
    }
    void ForStatement()
    {
        Lex();
        BEGIN_SCOPE();
        Expect(_SC('('));
        if(_token == TK_LOCAL) LocalDeclStatement();
        else if(_token != _SC(';')){
            CommaExpr();
            _fs->PopTarget();
        }
        Expect(_SC(';'));
        _fs->SnoozeOpt();
        GSInteger jmppos = _fs->GetCurrentPos();
        GSInteger jzpos = -1;
        if(_token != _SC(';')) { CommaExpr(); _fs->AddInstruction(_OP_JZ, _fs->PopTarget()); jzpos = _fs->GetCurrentPos(); }
        Expect(_SC(';'));
        _fs->SnoozeOpt();
        GSInteger expstart = _fs->GetCurrentPos() + 1;
        if(_token != _SC(')')) {
            CommaExpr();
            _fs->PopTarget();
        }
        Expect(_SC(')'));
        _fs->SnoozeOpt();
        GSInteger expend = _fs->GetCurrentPos();
        GSInteger expsize = (expend - expstart) + 1;
        GSInstructionVec exp;
        if(expsize > 0) {
            for(GSInteger i = 0; i < expsize; i++)
                exp.push_back(_fs->GetInstruction(expstart + i));
            _fs->PopInstructions(expsize);
        }
        BEGIN_BREAKBLE_BLOCK()
        Statement();
        GSInteger continuetrg = _fs->GetCurrentPos();
        if(expsize > 0) {
            for(GSInteger i = 0; i < expsize; i++)
                _fs->AddInstruction(exp[i]);
        }
        _fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1, 0);
        if(jzpos>  0) _fs->SetInstructionParam(jzpos, 1, _fs->GetCurrentPos() - jzpos);
        
        END_BREAKBLE_BLOCK(continuetrg);

		END_SCOPE();
    }
    void ForEachStatement()
    {
        GSObject idxname, valname;
        Lex(); Expect(_SC('(')); valname = Expect(TK_IDENTIFIER);
        if(_token == _SC(',')) {
            idxname = valname;
            Lex(); valname = Expect(TK_IDENTIFIER);
        }
        else{
            idxname = _fs->CreateString(_SC("@INDEX@"));
        }
        Expect(TK_IN);

        //save the stack size
        BEGIN_SCOPE();
        //put the table in the stack(evaluate the table expression)
        Expression(); Expect(_SC(')'));
        GSInteger container = _fs->TopTarget();
        //push the index local var
        GSInteger indexpos = _fs->PushLocalVariable(idxname);
        _fs->AddInstruction(_OP_LOADNULLS, indexpos,1);
        //push the value local var
        GSInteger valuepos = _fs->PushLocalVariable(valname);
        _fs->AddInstruction(_OP_LOADNULLS, valuepos,1);
        //push reference index
        GSInteger itrpos = _fs->PushLocalVariable(_fs->CreateString(_SC("@ITERATOR@"))); //use invalid id to make it inaccessible
        _fs->AddInstruction(_OP_LOADNULLS, itrpos,1);
        GSInteger jmppos = _fs->GetCurrentPos();
        _fs->AddInstruction(_OP_FOREACH, container, 0, indexpos);
        GSInteger foreachpos = _fs->GetCurrentPos();
        _fs->AddInstruction(_OP_POSTFOREACH, container, 0, indexpos);
        //generate the statement code
        BEGIN_BREAKBLE_BLOCK()
        Statement();
        _fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1);
        _fs->SetInstructionParam(foreachpos, 1, _fs->GetCurrentPos() - foreachpos);
        _fs->SetInstructionParam(foreachpos + 1, 1, _fs->GetCurrentPos() - foreachpos);
        END_BREAKBLE_BLOCK(foreachpos - 1);
        //restore the local variable stack(remove index,val and ref idx)
        _fs->PopTarget();
        END_SCOPE();
    }
    void SwitchStatement()
    {
        Lex(); Expect(_SC('(')); CommaExpr(); Expect(_SC(')'));
        Expect(_SC('{'));
        GSInteger expr = _fs->TopTarget();
        bool bfirst = true;
        GSInteger tonextcondjmp = -1;
        GSInteger skipcondjmp = -1;
        GSInteger __nbreaks__ = _fs->_unresolvedbreaks.size();
        _fs->_breaktargets.push_back(0);
        while(_token == TK_CASE) {
            if(!bfirst) {
                _fs->AddInstruction(_OP_JMP, 0, 0);
                skipcondjmp = _fs->GetCurrentPos();
                _fs->SetInstructionParam(tonextcondjmp, 1, _fs->GetCurrentPos() - tonextcondjmp);
            }
            //condition
            Lex(); Expression(); Expect(_SC(':'));
            GSInteger trg = _fs->PopTarget();
            GSInteger eqtarget = trg;
            bool local = _fs->IsLocal(trg);
            if(local) {
                eqtarget = _fs->PushTarget(); //we need to allocate a extra reg
            }
            _fs->AddInstruction(_OP_EQ, eqtarget, trg, expr);
            _fs->AddInstruction(_OP_JZ, eqtarget, 0);
            if(local) {
                _fs->PopTarget();
            }

            //end condition
            if(skipcondjmp != -1) {
                _fs->SetInstructionParam(skipcondjmp, 1, (_fs->GetCurrentPos() - skipcondjmp));
            }
            tonextcondjmp = _fs->GetCurrentPos();
            BEGIN_SCOPE();
            Statements();
            END_SCOPE();
            bfirst = false;
        }
        if(tonextcondjmp != -1)
            _fs->SetInstructionParam(tonextcondjmp, 1, _fs->GetCurrentPos() - tonextcondjmp);
        if(_token == TK_DEFAULT) {
            Lex(); Expect(_SC(':'));
            BEGIN_SCOPE();
            Statements();
            END_SCOPE();
        }
        Expect(_SC('}'));
        _fs->PopTarget();
        __nbreaks__ = _fs->_unresolvedbreaks.size() - __nbreaks__;
        if(__nbreaks__ > 0)ResolveBreaks(_fs, __nbreaks__);
        _fs->_breaktargets.pop_back();
    }
    void FunctionStatement(bool is_pub = false)
    {
        GSObject id;
        Lex(); id = Expect(TK_IDENTIFIER);
        
        if (is_pub) {
            _fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
            _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_fs->CreateString(_SC("__namespace"))));
            Emit2ArgsOP(_OP_GET);
        } else {
            _fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
        }
        
        _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
        if(_token == TK_DOUBLE_COLON) Emit2ArgsOP(_OP_GET);

        while(_token == TK_DOUBLE_COLON) {
            Lex();
            id = Expect(TK_IDENTIFIER);
            _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
            if(_token == TK_DOUBLE_COLON) Emit2ArgsOP(_OP_GET);
        }
		GSInteger boundtarget = 0xFF;
		if (_token == _SC('[')) {
			boundtarget = ParseBindEnv();
		}
        //Expect(_SC('('));
        CreateFunction(id, boundtarget);
        _fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, boundtarget);
        EmitDerefOp(_OP_NEWSLOT);
        _fs->PopTarget();
    }
    void ClassStatement(bool is_pub = false)
    {
        GSExpState es;
        Lex();
        es = _es;
        _es.donot_get = true;
        
        if (is_pub) {
            GSObject id = Expect(TK_IDENTIFIER);
            _fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
            _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_fs->CreateString(_SC("__namespace"))));
            Emit2ArgsOP(_OP_GET);

            _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
            _es.etype = OBJECT;
        } else {
            GSObject id = Expect(TK_IDENTIFIER);
            _fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
            _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
            _es.etype = OBJECT;
        }
        
        if(_es.etype == EXPR) {
            Error(_SC("invalid class name"));
        }
        else if(_es.etype == OBJECT || _es.etype == BASE) {
            ClassExp();
            EmitDerefOp(_OP_NEWSLOT);
            _fs->PopTarget();
        }
        else {
            Error(_SC("cannot create a class in a local with the syntax(class <local>)"));
        }
        _es = es;
    }
    GSObject ExpectScalar()
    {
        GSObject val;
        val._type = OT_NULL; val._unVal.nInteger = 0; //shut up GCC 4.x
        switch(_token) {
            case TK_INTEGER:
                val._type = OT_INTEGER;
                val._unVal.nInteger = _lex._nvalue;
                break;
            case TK_FLOAT:
                val._type = OT_FLOAT;
                val._unVal.fFloat = _lex._fvalue;
                break;
            case TK_STRING_LITERAL:
                val = _fs->CreateString(_lex._svalue,_lex._longstr.size()-1);
                break;
            case TK_TRUE:
            case TK_FALSE:
                val._type = OT_BOOL;
                val._unVal.nInteger = _token == TK_TRUE ? 1 : 0;
                break;
            case '-':
                Lex();
                switch(_token)
                {
                case TK_INTEGER:
                    val._type = OT_INTEGER;
                    val._unVal.nInteger = -_lex._nvalue;
                break;
                case TK_FLOAT:
                    val._type = OT_FLOAT;
                    val._unVal.fFloat = -_lex._fvalue;
                break;
                default:
                    Error(_SC("scalar expected : integer, float"));
                }
                break;
            default:
                Error(_SC("scalar expected : integer, float, or string"));
        }
        Lex();
        return val;
    }
    void EnumStatement()
    {
        Lex();
        GSObject id = Expect(TK_IDENTIFIER);
        Expect(_SC('{'));

        GSObject table = _fs->CreateTable();
        GSInteger nval = 0;
        while(_token != _SC('}')) {
            GSObject key = Expect(TK_IDENTIFIER);
            GSObject val;
            if(_token == _SC('=')) {
                Lex();
                val = ExpectScalar();
            }
            else {
                val._type = OT_INTEGER;
                val._unVal.nInteger = nval++;
            }
            _table(table)->NewSlot(GSObjectPtr(key),GSObjectPtr(val));
            if(_token == ',') Lex();
        }
        GSTable *enums = _table(_ss(_vm)->_consts);
        GSObjectPtr strongid = id;
        enums->NewSlot(GSObjectPtr(strongid),GSObjectPtr(table));
        strongid.Null();
        Lex();
    }
    void ImportStatement()
    {
        Lex();
        
        GSObject first_id = Expect(TK_IDENTIFIER);
        
        GSChar buf[1024];
        GSInteger len = 0;
        const GSChar* s1 = _stringval(first_id);
        
        while(*s1 && len < 1023) {
            buf[len++] = *s1++;
        }
        
        while (_token == '/') {
            Lex(); // consume /
            GSObject next_id = Expect(TK_IDENTIFIER);
            
            if (len < 1023) buf[len++] = _SC('/');
            
            const GSChar* s2 = _stringval(next_id);
            while(*s2 && len < 1023) {
                buf[len++] = *s2++;
            }
        }
        buf[len] = _SC('\0');
        
        GSObject final_import = _fs->CreateString(buf, len);
    
        _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(final_import));
        _fs->AddInstruction(_OP_IMPORT, _fs->PopTarget());
    }
    void NamespaceStatement()
    {
        Lex();
        
        GSObject first_id = Expect(TK_IDENTIFIER);
        
        GSChar buf[1024];
        GSInteger len = 0;
        const GSChar* s1 = _stringval(first_id);
        
        while(*s1 && len < 1023) {
            buf[len++] = *s1++;
        }
        
        while (_token == '/') {
            Lex(); // consume '/'
            GSObject next_id = Expect(TK_IDENTIFIER);
            
            if (len < 1023) buf[len++] = _SC('/');
            
            const GSChar* s2 = _stringval(next_id);
            while(*s2 && len < 1023) {
                buf[len++] = *s2++;
            }
        }
        buf[len] = _SC('\0');
        
        GSObject final_namespace = _fs->CreateString(buf, len);
        
        _fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(final_namespace));
        _fs->AddInstruction(_OP_NAMESPACE, _fs->PopTarget());
    }
    void TryCatchStatement()
    {
        GSObject exid;
        Lex();
        _fs->AddInstruction(_OP_PUSHTRAP,0,0);
        _fs->_traps++;
        if(_fs->_breaktargets.size()) _fs->_breaktargets.top()++;
        if(_fs->_continuetargets.size()) _fs->_continuetargets.top()++;
        GSInteger trappos = _fs->GetCurrentPos();
        {
            BEGIN_SCOPE();
            Statement();
            END_SCOPE();
        }
        _fs->_traps--;
        _fs->AddInstruction(_OP_POPTRAP, 1, 0);
        if(_fs->_breaktargets.size()) _fs->_breaktargets.top()--;
        if(_fs->_continuetargets.size()) _fs->_continuetargets.top()--;
        _fs->AddInstruction(_OP_JMP, 0, 0);
        GSInteger jmppos = _fs->GetCurrentPos();
        _fs->SetInstructionParam(trappos, 1, (_fs->GetCurrentPos() - trappos));
        Expect(TK_CATCH); Expect(_SC('(')); exid = Expect(TK_IDENTIFIER); Expect(_SC(')'));
        {
            BEGIN_SCOPE();
            GSInteger ex_target = _fs->PushLocalVariable(exid);
            _fs->SetInstructionParam(trappos, 0, ex_target);
            Statement();
            _fs->SetInstructionParams(jmppos, 0, (_fs->GetCurrentPos() - jmppos), 0);
            END_SCOPE();
        }
    }
	GSInteger ParseBindEnv()
	{
		GSInteger boundtarget;
		Lex();
		Expression();
		boundtarget = _fs->TopTarget();
		Expect(_SC(']'));
		return boundtarget;
	}
    void FunctionExp(bool lambda = false)
    {
        Lex(); 
		GSInteger boundtarget = 0xFF;
		if (_token == _SC('[')) {
			boundtarget = ParseBindEnv();
		}
		//Expect(_SC('('));
        GSObjectPtr dummy;
        CreateFunction(dummy, boundtarget, lambda);
        _fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, boundtarget);
    }
    void ClassExp()
    {
        GSInteger base = -1;
        GSInteger attrs = -1;
        if(_token == TK_EXTENDS) {
            Lex(); Expression();
            base = _fs->TopTarget();
        }
        if(_token == TK_ATTR_OPEN) {
            Lex();
            _fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,0,NOT_TABLE);
            ParseTableOrClass(_SC(','),TK_ATTR_CLOSE);
            attrs = _fs->TopTarget();
        }
        Expect(_SC('{'));
        if(attrs != -1) _fs->PopTarget();
        if(base != -1) _fs->PopTarget();
        _fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(), base, attrs,NOT_CLASS);
        ParseTableOrClass(_SC(';'),_SC('}'));
    }
    void DeleteExpr()
    {
        GSExpState es;
        Lex();
        es = _es;
        _es.donot_get = true;
        PrefixedExpr();
        if(_es.etype==EXPR) Error(_SC("can't delete an expression"));
        if(_es.etype==BASE) Error(_SC("can't delete 'base'"));
        if(_es.etype==OBJECT) {
            Emit2ArgsOP(_OP_DELETE);
        }
        else {
            Error(_SC("cannot delete an (outer) local"));
        }
        _es = es;
    }
    void PrefixIncDec(GSInteger token)
    {
        GSExpState  es;
        GSInteger diff = (token==TK_MINUSMINUS) ? -1 : 1;
        Lex();
        es = _es;
        _es.donot_get = true;
        PrefixedExpr();
        if(_es.etype==EXPR) {
            Error(_SC("can't '++' or '--' an expression"));
        }
        else if (_es.etype == BASE) {
            Error(_SC("can't '++' or '--' a base"));
        }
        else if(_es.etype==OBJECT) {
            Emit2ArgsOP(_OP_INC, diff);
        }
        else if(_es.etype==LOCAL) {
            GSInteger src = _fs->TopTarget();
            _fs->AddInstruction(_OP_INCL, src, src, 0, diff);

        }
        else if(_es.etype==OUTER) {
            GSInteger tmp = _fs->PushTarget();
            _fs->AddInstruction(_OP_GETOUTER, tmp, _es.epos);
            _fs->AddInstruction(_OP_INCL,     tmp, tmp, 0, diff);
            _fs->AddInstruction(_OP_SETOUTER, tmp, _es.epos, tmp);
        }
        _es = es;
    }
    void CreateFunction(GSObject &name,GSInteger boundtarget,bool lambda = false)
    {
        GSFuncState *funcstate = _fs->PushChildState(_ss(_vm));
        funcstate->_name = name;
        GSObject paramname;
        funcstate->AddParameter(_fs->CreateString(_SC("this")));
        funcstate->_sourcename = _sourcename;

        struct GenericDef {
            GSObject name;
            std::vector<GSInteger> param_regs;
        };
        std::vector<GenericDef> generics;

        if (_token == _SC('<')) {
            Lex(); // consume '<'
            do {
                GSObject gen_name = Expect(TK_IDENTIFIER);
                
                GenericDef def;
                def.name = gen_name;
                generics.push_back(def);
                
                if (_token == _SC(',')) {
                    Lex();
                } else {
                    break;
                }
            } while (1);
            Expect(_SC('>')); // consume '>'
        }

        Expect(_SC('('));

        GSInteger defparams = 0;
        GSInteger current_param_idx = 1;

        while(_token!=_SC(')')) {
            if(_token == TK_VARPARAMS) {
                if(defparams > 0) Error(_SC("function with default parameters cannot have variable number of parameters"));
                funcstate->AddParameter(_fs->CreateString(_SC("vargv")));
                funcstate->_varparams = true;
                current_param_idx++; // Increment our tracker
                Lex();
                if(_token != _SC(')')) Error(_SC("expected ')'"));
                break;
            }
            else {
                paramname = Expect(TK_IDENTIFIER);
                
                GSObject type_id;
                type_id._type = OT_NULL;
                
                if (_token == _SC(':')) {
                    Lex();
                    
                    if (_token == TK_IDENTIFIER) {
                        type_id = _fs->CreateString(_lex._svalue);
                    }

                    bool is_valid_type = (_token == TK_INT || _token == TK_FLOAT || _token == TK_STRING || 
                                          _token == TK_BOOL || _token == TK_VOID || _token == TK_IDENTIFIER);
                    if (!is_valid_type) {
                        Error(_SC("expected a valid type name after ':'"));
                    }
                    Lex();

                    if (_token == _SC('[')) {
                        Lex(); // consume '['
                        if (_token == TK_INTEGER) {
                            if (_lex._nvalue <= 0) {
                                Error(_SC("type mismatch: parameter array size must be greater than 0"));
                            }
                            Lex(); // consume the size integer
                        }
                        Expect(_SC(']')); // consume ']'
                    }
                }

                funcstate->AddParameter(paramname);
                
                if (GS_type(type_id) == OT_STRING) {
                    for(size_t i = 0; i < generics.size(); i++) {
                        if (scstrcmp(_stringval(generics[i].name), _stringval(type_id)) == 0) {
                            generics[i].param_regs.push_back(current_param_idx);
                        }
                    }
                }

                
                current_param_idx++;

                if(_token == _SC('=')) {
                    Lex();
                    Expression();
                    funcstate->AddDefaultParam(_fs->TopTarget());
                    defparams++;
                }
                else {
                    if(defparams > 0) Error(_SC("expected '='"));
                }
                if(_token == _SC(',')) Lex();
                else if(_token != _SC(')')) Error(_SC("expected ')' or ','"));
            }
        }
        Expect(_SC(')'));
        
        GSInteger expected_rettype = 0;
        if (_token == TK_ARROW) {
            Lex();
            bool is_valid_rettype = (_token == TK_INT || _token == TK_FLOAT || _token == TK_STRING || 
                                     _token == TK_BOOL || _token == TK_VOID || _token == TK_IDENTIFIER);
            if (!is_valid_rettype) {
                Error(_SC("expected return type after '->'"));
            }
            expected_rettype = _token;
            Lex();

            if (_token == _SC('[')) {
                Lex(); // consume '['
                if (_token == TK_INTEGER) {
                    GSInteger array_size = _lex._nvalue;
                    if (array_size <= 0) {
                        Error(_SC("type mismatch: fixed array size must be greater than 0"));
                    }
                    Lex(); // consume the size integer
                }
                Expect(_SC(']')); // consume ']'
            }
        }

        if (boundtarget != 0xFF) {
            _fs->PopTarget();
        }
        for(GSInteger n = 0; n < defparams; n++) {
            _fs->PopTarget();
        }

        GSFuncState *currchunk = _fs;
        _fs = funcstate;
        
        for (size_t i = 0; i < generics.size(); i++) {
            if (generics[i].param_regs.size() > 1) {
                GSInteger base_reg = generics[i].param_regs[0]; 
                for (size_t j = 1; j < generics[i].param_regs.size(); j++) {
                    _fs->AddInstruction(_OP_MATCHTYPES, base_reg, generics[i].param_regs[j]);
                }
            }
        }
        
        if(lambda) {
            Expression();
            _fs->AddInstruction(_OP_RETURN, 1, _fs->PopTarget());}
        else {
            Statement(false);
        }
        funcstate->AddLineInfos(_lex._prevtoken == _SC('\n')?_lex._lasttokenline:_lex._currentline, _lineinfo, true);
        funcstate->AddInstruction(_OP_RETURN, -1);
        funcstate->SetStackSize(0);

        GSFunctionProto *func = funcstate->BuildProto();
        func->_rettype = expected_rettype; 

#ifdef _DEBUG_DUMP
        funcstate->Dump(func);
#endif
        _fs = currchunk;
        _fs->_functions.push_back(func);
        _fs->PopChildState();
    }
    void ResolveBreaks(GSFuncState *funcstate, GSInteger ntoresolve)
    {
        while(ntoresolve > 0) {
            GSInteger pos = funcstate->_unresolvedbreaks.back();
            funcstate->_unresolvedbreaks.pop_back();
            //set the jmp instruction
            funcstate->SetInstructionParams(pos, 0, funcstate->GetCurrentPos() - pos, 0);
            ntoresolve--;
        }
    }
    void ResolveContinues(GSFuncState *funcstate, GSInteger ntoresolve, GSInteger targetpos)
    {
        while(ntoresolve > 0) {
            GSInteger pos = funcstate->_unresolvedcontinues.back();
            funcstate->_unresolvedcontinues.pop_back();
            //set the jmp instruction
            funcstate->SetInstructionParams(pos, 0, targetpos - pos, 0);
            ntoresolve--;
        }
    }
private:
    GSInteger _token;
    GSFuncState *_fs;
    GSObjectPtr _sourcename;
    GSLexer _lex;
    bool _lineinfo;
    bool _raiseerror;
    GSInteger _debugline;
    GSInteger _debugop;
    GSExpState   _es;
    GSScope _scope;
    GSChar _compilererror[MAX_COMPILER_ERROR_LEN];
    jmp_buf _errorjmp;
    GSVM *_vm;
};

bool Compile(GSVM *vm,GSLEXREADFUNC rg, GSUserPointer up, const GSChar *sourcename, GSObjectPtr &out, bool raiseerror, bool lineinfo)
{
    GSCompiler p(vm, rg, up, sourcename, raiseerror, lineinfo);
    return p.Compile(out);
}

#endif

