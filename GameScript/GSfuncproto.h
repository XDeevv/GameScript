/*  see copyright notice in GameScript.h */
#ifndef _GSFUNCTION_H_
#define _GSFUNCTION_H_

#include "GSopcodes.h"

enum GSOuterType {
    otLOCAL = 0,
    otOUTER = 1
};

struct GSOuterVar
{

    GSOuterVar(){}
    GSOuterVar(const GSObjectPtr &name,const GSObjectPtr &src,GSOuterType t)
    {
        _name = name;
        _src=src;
        _type=t;
    }
    GSOuterVar(const GSOuterVar &ov)
    {
        _type=ov._type;
        _src=ov._src;
        _name=ov._name;
    }
    GSOuterType _type;
    GSObjectPtr _name;
    GSObjectPtr _src;
};

struct GSLocalVarInfo
{
    GSLocalVarInfo():_start_op(0),_end_op(0),_pos(0){}
    GSLocalVarInfo(const GSLocalVarInfo &lvi)
    {
        _name=lvi._name;
        _start_op=lvi._start_op;
        _end_op=lvi._end_op;
        _pos=lvi._pos;
    }
    GSObjectPtr _name;
    GSUnsignedInteger _start_op;
    GSUnsignedInteger _end_op;
    GSUnsignedInteger _pos;
};

struct GSLineInfo { GSInteger _line;GSInteger _op; };

typedef GSvector<GSOuterVar> GSOuterVarVec;
typedef GSvector<GSLocalVarInfo> GSLocalVarInfoVec;
typedef GSvector<GSLineInfo> GSLineInfoVec;

#define _FUNC_SIZE(ni,nl,nparams,nfuncs,nouters,nlineinf,localinf,defparams) (sizeof(GSFunctionProto) \
        +((ni-1)*sizeof(GSInstruction))+(nl*sizeof(GSObjectPtr)) \
        +(nparams*sizeof(GSObjectPtr))+(nfuncs*sizeof(GSObjectPtr)) \
        +(nouters*sizeof(GSOuterVar))+(nlineinf*sizeof(GSLineInfo)) \
        +(localinf*sizeof(GSLocalVarInfo))+(defparams*sizeof(GSInteger)))


struct GSFunctionProto : public CHAINABLE_OBJ
{
private:
    GSFunctionProto(GSSharedState *ss);
    ~GSFunctionProto();

public:
    static GSFunctionProto *Create(GSSharedState *ss,GSInteger ninstructions,
        GSInteger nliterals,GSInteger nparameters,
        GSInteger nfunctions,GSInteger noutervalues,
        GSInteger nlineinfos,GSInteger nlocalvarinfos,GSInteger ndefaultparams)
    {
        GSFunctionProto *f;
        //I compact the whole class and members in a single memory allocation
        f = (GSFunctionProto *)GS_vm_malloc(_FUNC_SIZE(ninstructions,nliterals,nparameters,nfunctions,noutervalues,nlineinfos,nlocalvarinfos,ndefaultparams));
        new (f) GSFunctionProto(ss);
        f->_ninstructions = ninstructions;
        f->_literals = (GSObjectPtr*)&f->_instructions[ninstructions];
        f->_nliterals = nliterals;
        f->_parameters = (GSObjectPtr*)&f->_literals[nliterals];
        f->_nparameters = nparameters;
        f->_functions = (GSObjectPtr*)&f->_parameters[nparameters];
        f->_nfunctions = nfunctions;
        f->_outervalues = (GSOuterVar*)&f->_functions[nfunctions];
        f->_noutervalues = noutervalues;
        f->_lineinfos = (GSLineInfo *)&f->_outervalues[noutervalues];
        f->_nlineinfos = nlineinfos;
        f->_localvarinfos = (GSLocalVarInfo *)&f->_lineinfos[nlineinfos];
        f->_nlocalvarinfos = nlocalvarinfos;
        f->_defaultparams = (GSInteger *)&f->_localvarinfos[nlocalvarinfos];
        f->_ndefaultparams = ndefaultparams;

        _CONSTRUCT_VECTOR(GSObjectPtr,f->_nliterals,f->_literals);
        _CONSTRUCT_VECTOR(GSObjectPtr,f->_nparameters,f->_parameters);
        _CONSTRUCT_VECTOR(GSObjectPtr,f->_nfunctions,f->_functions);
        _CONSTRUCT_VECTOR(GSOuterVar,f->_noutervalues,f->_outervalues);
        //_CONSTRUCT_VECTOR(GSLineInfo,f->_nlineinfos,f->_lineinfos); //not required are 2 integers
        _CONSTRUCT_VECTOR(GSLocalVarInfo,f->_nlocalvarinfos,f->_localvarinfos);
        return f;
    }
    void Release(){
        _DESTRUCT_VECTOR(GSObjectPtr,_nliterals,_literals);
        _DESTRUCT_VECTOR(GSObjectPtr,_nparameters,_parameters);
        _DESTRUCT_VECTOR(GSObjectPtr,_nfunctions,_functions);
        _DESTRUCT_VECTOR(GSOuterVar,_noutervalues,_outervalues);
        //_DESTRUCT_VECTOR(GSLineInfo,_nlineinfos,_lineinfos); //not required are 2 integers
        _DESTRUCT_VECTOR(GSLocalVarInfo,_nlocalvarinfos,_localvarinfos);
        GSInteger size = _FUNC_SIZE(_ninstructions,_nliterals,_nparameters,_nfunctions,_noutervalues,_nlineinfos,_nlocalvarinfos,_ndefaultparams);
        this->~GSFunctionProto();
        GS_vm_free(this,size);
    }

    const GSChar* GetLocal(GSVM *v,GSUnsignedInteger stackbase,GSUnsignedInteger nseq,GSUnsignedInteger nop);
    GSInteger GetLine(GSInstruction *curr);
    bool Save(GSVM *v,GSUserPointer up,GSWRITEFUNC write);
    static bool Load(GSVM *v,GSUserPointer up,GSREADFUNC read,GSObjectPtr &ret);
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    void Finalize(){ _NULL_GSOBJECT_VECTOR(_literals,_nliterals); }
    GSObjectType GetType() {return OT_FUNCPROTO;}
#endif
    GSObjectPtr _sourcename;
    GSObjectPtr _name;
    GSInteger _stacksize;
    bool _bgenerator;
    GSInteger _varparams;

    GSInteger _nlocalvarinfos;
    GSLocalVarInfo *_localvarinfos;

    GSInteger _nlineinfos;
    GSLineInfo *_lineinfos;

    GSInteger _nliterals;
    GSObjectPtr *_literals;

    GSInteger _nparameters;
    GSObjectPtr *_parameters;

    GSInteger _nfunctions;
    GSObjectPtr *_functions;

    GSInteger _noutervalues;
    GSOuterVar *_outervalues;

    GSInteger _ndefaultparams;
    GSInteger *_defaultparams;

    GSInteger _ninstructions;
    GSInstruction _instructions[1];
};

#endif //_GSFUNCTION_H_

