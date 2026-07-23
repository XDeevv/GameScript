/*  see copyright notice in GameScript.h */
#ifndef _GSFUNCSTATE_H_
#define _GSFUNCSTATE_H_
///////////////////////////////////
#include "GSutils.h"

struct GSFuncState
{
    GSFuncState(GSSharedState *ss,GSFuncState *parent,CompilerErrorFunc efunc,void *ed);
    ~GSFuncState();
#ifdef _DEBUG_DUMP
    void Dump(GSFunctionProto *func);
#endif
    void Error(const GSChar *err);
    GSFuncState *PushChildState(GSSharedState *ss);
    void PopChildState();
    void AddInstruction(GSOpcode _op,GSInteger arg0=0,GSInteger arg1=0,GSInteger arg2=0,GSInteger arg3=0){GSInstruction i(_op,arg0,arg1,arg2,arg3);AddInstruction(i);}
    void AddInstruction(GSInstruction &i);
    void SetInstructionParams(GSInteger pos,GSInteger arg0,GSInteger arg1,GSInteger arg2=0,GSInteger arg3=0);
    void SetInstructionParam(GSInteger pos,GSInteger arg,GSInteger val);
    GSInstruction &GetInstruction(GSInteger pos){return _instructions[pos];}
    void PopInstructions(GSInteger size){for(GSInteger i=0;i<size;i++)_instructions.pop_back();}
    void SetStackSize(GSInteger n);
    GSInteger CountOuters(GSInteger stacksize);
    void SnoozeOpt(){_optimization=false;}
    void AddDefaultParam(GSInteger trg) { _defaultparams.push_back(trg); }
    GSInteger GetDefaultParamCount() { return _defaultparams.size(); }
    GSInteger GetCurrentPos(){return _instructions.size()-1;}
    GSInteger GetNumericConstant(const GSInteger cons);
    GSInteger GetNumericConstant(const GSFloat cons);
    GSInteger PushLocalVariable(const GSObject &name, GSInteger type = -1, GSInteger array_size = -1);
    void AddParameter(const GSObject &name);
    //void AddOuterValue(const GSObject &name);
    GSInteger GetLocalVariable(const GSObject &name);
    void MarkLocalAsOuter(GSInteger pos);
    GSInteger GetOuterVariable(const GSObject &name);
    GSInteger GenerateCode();
    GSInteger GetStackSize();
    GSInteger CalcStackFrameSize();
    void AddLineInfos(GSInteger line,bool lineop,bool force=false);
    GSFunctionProto *BuildProto();
    GSInteger AllocStackPos();
    GSInteger PushTarget(GSInteger n=-1);
    GSInteger PopTarget();
    GSInteger TopTarget();
    GSInteger GetUpTarget(GSInteger n);
    void DiscardTarget();
    bool IsLocal(GSUnsignedInteger stkpos);
    GSObject CreateString(const GSChar *s,GSInteger len = -1);
    GSObject CreateTable();
    bool IsConstant(const GSObject &name,GSObject &e);
    GSInteger _returnexp;
    GSLocalVarInfoVec _vlocals;
    GSIntVec _targetstack;
    GSInteger _stacksize;
    bool _varparams;
    bool _bgenerator;
    GSIntVec _unresolvedbreaks;
    GSIntVec _unresolvedcontinues;
    GSObjectPtrVec _functions;
    GSObjectPtrVec _parameters;
    GSOuterVarVec _outervalues;
    GSInstructionVec _instructions;
    GSLocalVarInfoVec _localvarinfos;
    GSObjectPtr _literals;
    GSObjectPtr _strings;
    GSObjectPtr _name;
    GSObjectPtr _sourcename;
    GSInteger _nliterals;
    GSLineInfoVec _lineinfos;
    GSFuncState *_parent;
    GSIntVec _scope_blocks;
    GSIntVec _breaktargets;
    GSIntVec _continuetargets;
    GSIntVec _defaultparams;
    GSInteger _lastline;
    GSInteger _traps; //contains number of nested exception traps
    GSInteger _outers;
    bool _optimization;
    GSSharedState *_sharedstate;
    GSvector<GSFuncState*> _childstates;
    GSInteger GetConstant(const GSObject &cons);
private:
    CompilerErrorFunc _errfunc;
    void *_errtarget;
    GSSharedState *_ss;
};


#endif //_GSFUNCSTATE_H_


