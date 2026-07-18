/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#ifndef NO_COMPILER
#include "GScompiler.h"
#include "GSstring.h"
#include "GSfuncproto.h"
#include "GStable.h"
#include "GSopcodes.h"
#include "GSfuncstate.h"

#ifdef _DEBUG_DUMP
GSInstructionDesc g_InstrDesc[]={
    {_SC("_OP_LINE")},
    {_SC("_OP_LOAD")},
    {_SC("_OP_LOADINT")},
    {_SC("_OP_LOADFLOAT")},
    {_SC("_OP_DLOAD")},
    {_SC("_OP_TAILCALL")},
    {_SC("_OP_CALL")},
    {_SC("_OP_PREPCALL")},
    {_SC("_OP_PREPCALLK")},
    {_SC("_OP_GETK")},
    {_SC("_OP_MOVE")},
    {_SC("_OP_NEWSLOT")},
    {_SC("_OP_DELETE")},
    {_SC("_OP_SET")},
    {_SC("_OP_GET")},
    {_SC("_OP_EQ")},
    {_SC("_OP_NE")},
    {_SC("_OP_ADD")},
    {_SC("_OP_SUB")},
    {_SC("_OP_MUL")},
    {_SC("_OP_DIV")},
    {_SC("_OP_MOD")},
    {_SC("_OP_BITW")},
    {_SC("_OP_RETURN")},
    {_SC("_OP_LOADNULLS")},
    {_SC("_OP_LOADROOT")},
    {_SC("_OP_LOADBOOL")},
    {_SC("_OP_DMOVE")},
    {_SC("_OP_JMP")},
    {_SC("_OP_JCMP")},
    {_SC("_OP_JZ")},
    {_SC("_OP_SETOUTER")},
    {_SC("_OP_GETOUTER")},
    {_SC("_OP_NEWOBJ")},
    {_SC("_OP_APPENDARRAY")},
    {_SC("_OP_COMPARITH")},
    {_SC("_OP_INC")},
    {_SC("_OP_INCL")},
    {_SC("_OP_PINC")},
    {_SC("_OP_PINCL")},
    {_SC("_OP_CMP")},
    {_SC("_OP_EXISTS")},
    {_SC("_OP_INSTANCEOF")},
    {_SC("_OP_AND")},
    {_SC("_OP_OR")},
    {_SC("_OP_NEG")},
    {_SC("_OP_NOT")},
    {_SC("_OP_BWNOT")},
    {_SC("_OP_CLOSURE")},
    {_SC("_OP_YIELD")},
    {_SC("_OP_RESUME")},
    {_SC("_OP_FOREACH")},
    {_SC("_OP_POSTFOREACH")},
    {_SC("_OP_CLONE")},
    {_SC("_OP_TYPEOF")},
    {_SC("_OP_PUSHTRAP")},
    {_SC("_OP_POPTRAP")},
    {_SC("_OP_THROW")},
    {_SC("_OP_NEWSLOTA")},
    {_SC("_OP_GETBASE")},
    {_SC("_OP_CLOSE")},
};
#endif
void DumpLiteral(GSObjectPtr &o)
{
    switch(GS_type(o)){
        case OT_STRING: scprintf(_SC("\"%s\""),_stringval(o));break;
        case OT_FLOAT: scprintf(_SC("{%f}"),_float(o));break;
        case OT_INTEGER: scprintf(_SC("{") _PRINT_INT_FMT _SC("}"),_integer(o));break;
        case OT_BOOL: scprintf(_SC("%s"),_integer(o)?_SC("true"):_SC("false"));break;
        default: scprintf(_SC("(%s %p)"),GetTypeName(o),(void*)_rawval(o));break; break; //shut up compiler
    }
}

GSFuncState::GSFuncState(GSSharedState *ss,GSFuncState *parent,CompilerErrorFunc efunc,void *ed)
{
        _nliterals = 0;
        _literals = GSTable::Create(ss,0);
        _strings =  GSTable::Create(ss,0);
        _sharedstate = ss;
        _lastline = 0;
        _optimization = true;
        _parent = parent;
        _stacksize = 0;
        _traps = 0;
        _returnexp = 0;
        _varparams = false;
        _errfunc = efunc;
        _errtarget = ed;
        _bgenerator = false;
        _outers = 0;
        _ss = ss;

}

void GSFuncState::Error(const GSChar *err)
{
    _errfunc(_errtarget,err);
}

#ifdef _DEBUG_DUMP
void GSFuncState::Dump(GSFunctionProto *func)
{
    GSUnsignedInteger n=0,i;
    GSInteger si;
    scprintf(_SC("GSInstruction sizeof %d\n"),(GSInt32)sizeof(GSInstruction));
    scprintf(_SC("GSObject sizeof %d\n"), (GSInt32)sizeof(GSObject));
    scprintf(_SC("--------------------------------------------------------------------\n"));
    scprintf(_SC("*****FUNCTION [%s]\n"),GS_type(func->_name)==OT_STRING?_stringval(func->_name):_SC("unknown"));
    scprintf(_SC("-----LITERALS\n"));
    GSObjectPtr refidx,key,val;
    GSInteger idx;
    GSObjectPtrVec templiterals;
    templiterals.resize(_nliterals);
    while((idx=_table(_literals)->Next(false,refidx,key,val))!=-1) {
        refidx=idx;
        templiterals[_integer(val)]=key;
    }
    for(i=0;i<templiterals.size();i++){
        scprintf(_SC("[%d] "), (GSInt32)n);
        DumpLiteral(templiterals[i]);
        scprintf(_SC("\n"));
        n++;
    }
    scprintf(_SC("-----PARAMS\n"));
    if(_varparams)
        scprintf(_SC("<<VARPARAMS>>\n"));
    n=0;
    for(i=0;i<_parameters.size();i++){
        scprintf(_SC("[%d] "), (GSInt32)n);
        DumpLiteral(_parameters[i]);
        scprintf(_SC("\n"));
        n++;
    }
    scprintf(_SC("-----LOCALS\n"));
    for(si=0;si<func->_nlocalvarinfos;si++){
        GSLocalVarInfo lvi=func->_localvarinfos[si];
        scprintf(_SC("[%d] %s \t%d %d\n"), (GSInt32)lvi._pos,_stringval(lvi._name), (GSInt32)lvi._start_op, (GSInt32)lvi._end_op);
        n++;
    }
    scprintf(_SC("-----LINE INFO\n"));
    for(i=0;i<_lineinfos.size();i++){
        GSLineInfo li=_lineinfos[i];
        scprintf(_SC("op [%d] line [%d] \n"), (GSInt32)li._op, (GSInt32)li._line);
        n++;
    }
    scprintf(_SC("-----dump\n"));
    n=0;
    for(i=0;i<_instructions.size();i++){
        GSInstruction &inst=_instructions[i];
        if(inst.op==_OP_LOAD || inst.op==_OP_DLOAD || inst.op==_OP_PREPCALLK || inst.op==_OP_GETK ){

            GSInteger lidx = inst._arg1;
            scprintf(_SC("[%03d] %15s %d "), (GSInt32)n,g_InstrDesc[inst.op].name,inst._arg0);
            if(lidx >= 0xFFFFFFFF)
                scprintf(_SC("null"));
            else {
                GSInteger refidx;
                GSObjectPtr val,key,refo;
                while(((refidx=_table(_literals)->Next(false,refo,key,val))!= -1) && (_integer(val) != lidx)) {
                    refo = refidx;
                }
                DumpLiteral(key);
            }
            if(inst.op != _OP_DLOAD) {
                scprintf(_SC(" %d %d \n"),inst._arg2,inst._arg3);
            }
            else {
                scprintf(_SC(" %d "),inst._arg2);
                lidx = inst._arg3;
                if(lidx >= 0xFFFFFFFF)
                    scprintf(_SC("null"));
                else {
                    GSInteger refidx;
                    GSObjectPtr val,key,refo;
                    while(((refidx=_table(_literals)->Next(false,refo,key,val))!= -1) && (_integer(val) != lidx)) {
                        refo = refidx;
                }
                DumpLiteral(key);
                scprintf(_SC("\n"));
            }
            }
        }
        else if(inst.op==_OP_LOADFLOAT) {
            scprintf(_SC("[%03d] %15s %d %f %d %d\n"), (GSInt32)n,g_InstrDesc[inst.op].name,inst._arg0,*((GSFloat*)&inst._arg1),inst._arg2,inst._arg3);
        }
    /*  else if(inst.op==_OP_ARITH){
            scprintf(_SC("[%03d] %15s %d %d %d %c\n"),n,g_InstrDesc[inst.op].name,inst._arg0,inst._arg1,inst._arg2,inst._arg3);
        }*/
        else {
            scprintf(_SC("[%03d] %15s %d %d %d %d\n"), (GSInt32)n,g_InstrDesc[inst.op].name,inst._arg0,inst._arg1,inst._arg2,inst._arg3);
        }
        n++;
    }
    scprintf(_SC("-----\n"));
    scprintf(_SC("stack size[%d]\n"), (GSInt32)func->_stacksize);
    scprintf(_SC("--------------------------------------------------------------------\n\n"));
}
#endif

GSInteger GSFuncState::GetNumericConstant(const GSInteger cons)
{
    return GetConstant(GSObjectPtr(cons));
}

GSInteger GSFuncState::GetNumericConstant(const GSFloat cons)
{
    return GetConstant(GSObjectPtr(cons));
}

GSInteger GSFuncState::GetConstant(const GSObject &cons)
{
    GSObjectPtr val;
    if(!_table(_literals)->Get(cons,val))
    {
        val = _nliterals;
        _table(_literals)->NewSlot(cons,val);
        _nliterals++;
        if(_nliterals > MAX_LITERALS) {
            val.Null();
            Error(_SC("internal compiler error: too many literals"));
        }
    }
    return _integer(val);
}

void GSFuncState::SetInstructionParams(GSInteger pos,GSInteger arg0,GSInteger arg1,GSInteger arg2,GSInteger arg3)
{
    _instructions[pos]._arg0=(unsigned char)*((GSUnsignedInteger *)&arg0);
    _instructions[pos]._arg1=(GSInt32)*((GSUnsignedInteger *)&arg1);
    _instructions[pos]._arg2=(unsigned char)*((GSUnsignedInteger *)&arg2);
    _instructions[pos]._arg3=(unsigned char)*((GSUnsignedInteger *)&arg3);
}

void GSFuncState::SetInstructionParam(GSInteger pos,GSInteger arg,GSInteger val)
{
    switch(arg){
        case 0:_instructions[pos]._arg0=(unsigned char)*((GSUnsignedInteger *)&val);break;
        case 1:case 4:_instructions[pos]._arg1=(GSInt32)*((GSUnsignedInteger *)&val);break;
        case 2:_instructions[pos]._arg2=(unsigned char)*((GSUnsignedInteger *)&val);break;
        case 3:_instructions[pos]._arg3=(unsigned char)*((GSUnsignedInteger *)&val);break;
    };
}

GSInteger GSFuncState::AllocStackPos()
{
    GSInteger npos=_vlocals.size();
    _vlocals.push_back(GSLocalVarInfo());
    if(_vlocals.size()>((GSUnsignedInteger)_stacksize)) {
        if(_stacksize>MAX_FUNC_STACKSIZE) Error(_SC("internal compiler error: too many locals"));
        _stacksize=_vlocals.size();
    }
    return npos;
}

GSInteger GSFuncState::PushTarget(GSInteger n)
{
    if(n!=-1){
        _targetstack.push_back(n);
        return n;
    }
    n=AllocStackPos();
    _targetstack.push_back(n);
    return n;
}

GSInteger GSFuncState::GetUpTarget(GSInteger n){
    return _targetstack[((_targetstack.size()-1)-n)];
}

GSInteger GSFuncState::TopTarget(){
    return _targetstack.back();
}
GSInteger GSFuncState::PopTarget()
{
    GSUnsignedInteger npos=_targetstack.back();
    assert(npos < _vlocals.size());
    GSLocalVarInfo &t = _vlocals[npos];
    if(GS_type(t._name)==OT_NULL){
        _vlocals.pop_back();
    }
    _targetstack.pop_back();
    return npos;
}

GSInteger GSFuncState::GetStackSize()
{
    return _vlocals.size();
}

GSInteger GSFuncState::CountOuters(GSInteger stacksize)
{
    GSInteger outers = 0;
    GSInteger k = _vlocals.size() - 1;
    while(k >= stacksize) {
        GSLocalVarInfo &lvi = _vlocals[k];
        k--;
        if(lvi._end_op == UINT_MINUS_ONE) { //this means is an outer
            outers++;
        }
    }
    return outers;
}

void GSFuncState::SetStackSize(GSInteger n)
{
    GSInteger size=_vlocals.size();
    while(size>n){
        size--;
        GSLocalVarInfo lvi = _vlocals.back();
        if(GS_type(lvi._name)!=OT_NULL){
            if(lvi._end_op == UINT_MINUS_ONE) { //this means is an outer
                _outers--;
            }
            lvi._end_op = GetCurrentPos();
            _localvarinfos.push_back(lvi);
        }
        _vlocals.pop_back();
    }
}

bool GSFuncState::IsConstant(const GSObject &name,GSObject &e)
{
    GSObjectPtr val;
    if(_table(_sharedstate->_consts)->Get(name,val)) {
        e = val;
        return true;
    }
    return false;
}

bool GSFuncState::IsLocal(GSUnsignedInteger stkpos)
{
    if(stkpos>=_vlocals.size())return false;
    else if(GS_type(_vlocals[stkpos]._name)!=OT_NULL)return true;
    return false;
}

GSInteger GSFuncState::PushLocalVariable(const GSObject &name)
{
    GSInteger pos=_vlocals.size();
    GSLocalVarInfo lvi;
    lvi._name=name;
    lvi._start_op=GetCurrentPos()+1;
    lvi._pos=_vlocals.size();
    _vlocals.push_back(lvi);
    if(_vlocals.size()>((GSUnsignedInteger)_stacksize))_stacksize=_vlocals.size();
    return pos;
}



GSInteger GSFuncState::GetLocalVariable(const GSObject &name)
{
    GSInteger locals=_vlocals.size();
    while(locals>=1){
        GSLocalVarInfo &lvi = _vlocals[locals-1];
        if(GS_type(lvi._name)==OT_STRING && _string(lvi._name)==_string(name)){
            return locals-1;
        }
        locals--;
    }
    return -1;
}

void GSFuncState::MarkLocalAsOuter(GSInteger pos)
{
    GSLocalVarInfo &lvi = _vlocals[pos];
    lvi._end_op = UINT_MINUS_ONE;
    _outers++;
}

GSInteger GSFuncState::GetOuterVariable(const GSObject &name)
{
    GSInteger outers = _outervalues.size();
    for(GSInteger i = 0; i<outers; i++) {
        if(_string(_outervalues[i]._name) == _string(name))
            return i;
    }
    GSInteger pos=-1;
    if(_parent) {
        pos = _parent->GetLocalVariable(name);
        if(pos == -1) {
            pos = _parent->GetOuterVariable(name);
            if(pos != -1) {
                _outervalues.push_back(GSOuterVar(name,GSObjectPtr(GSInteger(pos)),otOUTER)); //local
                return _outervalues.size() - 1;
            }
        }
        else {
            _parent->MarkLocalAsOuter(pos);
            _outervalues.push_back(GSOuterVar(name,GSObjectPtr(GSInteger(pos)),otLOCAL)); //local
            return _outervalues.size() - 1;


        }
    }
    return -1;
}

void GSFuncState::AddParameter(const GSObject &name)
{
    PushLocalVariable(name);
    _parameters.push_back(name);
}

void GSFuncState::AddLineInfos(GSInteger line,bool lineop,bool force)
{
    if(_lastline!=line || force){
        GSLineInfo li;
        li._line=line;li._op=(GetCurrentPos()+1);
        if(lineop)AddInstruction(_OP_LINE,0,line);
        if(_lastline!=line) {
            _lineinfos.push_back(li);
        }
        _lastline=line;
    }
}

void GSFuncState::DiscardTarget()
{
    GSInteger discardedtarget = PopTarget();
    GSInteger size = _instructions.size();
    if(size > 0 && _optimization){
        GSInstruction &pi = _instructions[size-1];//previous instruction
        switch(pi.op) {
        case _OP_SET:case _OP_NEWSLOT:case _OP_SETOUTER:case _OP_CALL:
            if(pi._arg0 == discardedtarget) {
                pi._arg0 = 0xFF;
            }
        }
    }
}

void GSFuncState::AddInstruction(GSInstruction &i)
{
    GSInteger size = _instructions.size();
    if(size > 0 && _optimization){ //simple optimizer
        GSInstruction &pi = _instructions[size-1];//previous instruction
        switch(i.op) {
        case _OP_JZ:
            if( pi.op == _OP_CMP && pi._arg1 < 0xFF) {
                pi.op = _OP_JCMP;
                pi._arg0 = (unsigned char)pi._arg1;
                pi._arg1 = i._arg1;
                return;
            }
            break;
        case _OP_SET:
        case _OP_NEWSLOT:
            if(i._arg0 == i._arg3) {
                i._arg0 = 0xFF;
            }
            break;
        case _OP_SETOUTER:
            if(i._arg0 == i._arg2) {
                i._arg0 = 0xFF;
            }
            break;
        case _OP_RETURN:
            if( _parent && i._arg0 != MAX_FUNC_STACKSIZE && pi.op == _OP_CALL && _returnexp < size-1) {
                pi.op = _OP_TAILCALL;
            } else if(pi.op == _OP_CLOSE){
                pi = i;
                return;
            }
        break;
        case _OP_GET:
            if( pi.op == _OP_LOAD && pi._arg0 == i._arg2 && (!IsLocal(pi._arg0))){
                pi._arg2 = (unsigned char)i._arg1;
                pi.op = _OP_GETK;
                pi._arg0 = i._arg0;

                return;
            }
        break;
        case _OP_PREPCALL:
            if( pi.op == _OP_LOAD  && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0))){
                pi.op = _OP_PREPCALLK;
                pi._arg0 = i._arg0;
                pi._arg2 = i._arg2;
                pi._arg3 = i._arg3;
                return;
            }
            break;
        case _OP_APPENDARRAY: {
            GSInteger aat = -1;
            switch(pi.op) {
            case _OP_LOAD: aat = AAT_LITERAL; break;
            case _OP_LOADINT: aat = AAT_INT; break;
            case _OP_LOADBOOL: aat = AAT_BOOL; break;
            case _OP_LOADFLOAT: aat = AAT_FLOAT; break;
            default: break;
            }
            if(aat != -1 && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0))){
                pi.op = _OP_APPENDARRAY;
                pi._arg0 = i._arg0;
                pi._arg2 = (unsigned char)aat;
                pi._arg3 = MAX_FUNC_STACKSIZE;
                return;
            }
                              }
            break;
        case _OP_MOVE:
            switch(pi.op) {
            case _OP_GET: case _OP_ADD: case _OP_SUB: case _OP_MUL: case _OP_DIV: case _OP_MOD: case _OP_BITW:
            case _OP_LOADINT: case _OP_LOADFLOAT: case _OP_LOADBOOL: case _OP_LOAD:

                if(pi._arg0 == i._arg1)
                {
                    pi._arg0 = i._arg0;
                    _optimization = false;
                    //_result_elimination = false;
                    return;
                }
            }

            if(pi.op == _OP_MOVE)
            {
                pi.op = _OP_DMOVE;
                pi._arg2 = i._arg0;
                pi._arg3 = (unsigned char)i._arg1;
                return;
            }
            break;
        case _OP_LOAD:
            if(pi.op == _OP_LOAD && i._arg1 < 256) {
                pi.op = _OP_DLOAD;
                pi._arg2 = i._arg0;
                pi._arg3 = (unsigned char)i._arg1;
                return;
            }
            break;
        case _OP_EQ:case _OP_NE:
            if(pi.op == _OP_LOAD && pi._arg0 == i._arg1 && (!IsLocal(pi._arg0) ))
            {
                pi.op = i.op;
                pi._arg0 = i._arg0;
                pi._arg2 = i._arg2;
                pi._arg3 = MAX_FUNC_STACKSIZE;
                return;
            }
            break;
        case _OP_LOADNULLS:
            if((pi.op == _OP_LOADNULLS && pi._arg0+pi._arg1 == i._arg0)) {

                pi._arg1 = pi._arg1 + 1;
                pi.op = _OP_LOADNULLS;
                return;
            }
            break;
        case _OP_LINE:
            if(pi.op == _OP_LINE) {
                _instructions.pop_back();
                _lineinfos.pop_back();
            }
            break;
        }
    }
    _optimization = true;
    _instructions.push_back(i);
}

GSObject GSFuncState::CreateString(const GSChar *s,GSInteger len)
{
    GSObjectPtr ns(GSString::Create(_sharedstate,s,len));
    _table(_strings)->NewSlot(ns,(GSInteger)1);
    return ns;
}

GSObject GSFuncState::CreateTable()
{
    GSObjectPtr nt(GSTable::Create(_sharedstate,0));
    _table(_strings)->NewSlot(nt,(GSInteger)1);
    return nt;
}

GSFunctionProto *GSFuncState::BuildProto()
{

    GSFunctionProto *f=GSFunctionProto::Create(_ss,_instructions.size(),
        _nliterals,_parameters.size(),_functions.size(),_outervalues.size(),
        _lineinfos.size(),_localvarinfos.size(),_defaultparams.size());

    GSObjectPtr refidx,key,val;
    GSInteger idx;

    f->_stacksize = _stacksize;
    f->_sourcename = _sourcename;
    f->_bgenerator = _bgenerator;
    f->_name = _name;

    while((idx=_table(_literals)->Next(false,refidx,key,val))!=-1) {
        f->_literals[_integer(val)]=key;
        refidx=idx;
    }

    for(GSUnsignedInteger nf = 0; nf < _functions.size(); nf++) f->_functions[nf] = _functions[nf];
    for(GSUnsignedInteger np = 0; np < _parameters.size(); np++) f->_parameters[np] = _parameters[np];
    for(GSUnsignedInteger no = 0; no < _outervalues.size(); no++) f->_outervalues[no] = _outervalues[no];
    for(GSUnsignedInteger nl = 0; nl < _localvarinfos.size(); nl++) f->_localvarinfos[nl] = _localvarinfos[nl];
    for(GSUnsignedInteger ni = 0; ni < _lineinfos.size(); ni++) f->_lineinfos[ni] = _lineinfos[ni];
    for(GSUnsignedInteger nd = 0; nd < _defaultparams.size(); nd++) f->_defaultparams[nd] = _defaultparams[nd];

    memcpy(f->_instructions,&_instructions[0],_instructions.size()*sizeof(GSInstruction));

    f->_varparams = _varparams;

    return f;
}

GSFuncState *GSFuncState::PushChildState(GSSharedState *ss)
{
    GSFuncState *child = (GSFuncState *)GS_malloc(sizeof(GSFuncState));
    new (child) GSFuncState(ss,this,_errfunc,_errtarget);
    _childstates.push_back(child);
    return child;
}

void GSFuncState::PopChildState()
{
    GSFuncState *child = _childstates.back();
    GS_delete(child,GSFuncState);
    _childstates.pop_back();
}

GSFuncState::~GSFuncState()
{
    while(_childstates.size() > 0)
    {
        PopChildState();
    }
}

#endif

