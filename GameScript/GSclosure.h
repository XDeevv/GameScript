/*  see copyright notice in GameScript.h */
#ifndef _GSCLOSURE_H_
#define _GSCLOSURE_H_


#define _CALC_CLOSURE_SIZE(func) (sizeof(GSClosure) + (func->_noutervalues*sizeof(GSObjectPtr)) + (func->_ndefaultparams*sizeof(GSObjectPtr)))

struct GSFunctionProto;
struct GSClass;
struct GSClosure : public CHAINABLE_OBJ
{
private:
    GSClosure(GSSharedState *ss,GSFunctionProto *func){_function = func; __ObjAddRef(_function); _base = NULL; INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this); _env = NULL; _root=NULL;}
public:
    static GSClosure *Create(GSSharedState *ss,GSFunctionProto *func,GSWeakRef *root){
        GSInteger size = _CALC_CLOSURE_SIZE(func);
        GSClosure *nc=(GSClosure*)GS_MALLOC(size);
        new (nc) GSClosure(ss,func);
        nc->_outervalues = (GSObjectPtr *)(nc + 1);
        nc->_defaultparams = &nc->_outervalues[func->_noutervalues];
        nc->_root = root;
         __ObjAddRef(nc->_root);
        _CONSTRUCT_VECTOR(GSObjectPtr,func->_noutervalues,nc->_outervalues);
        _CONSTRUCT_VECTOR(GSObjectPtr,func->_ndefaultparams,nc->_defaultparams);
        return nc;
    }
    void Release(){
        GSFunctionProto *f = _function;
        GSInteger size = _CALC_CLOSURE_SIZE(f);
        _DESTRUCT_VECTOR(GSObjectPtr,f->_noutervalues,_outervalues);
        _DESTRUCT_VECTOR(GSObjectPtr,f->_ndefaultparams,_defaultparams);
        __ObjRelease(_function);
        this->~GSClosure();
        GS_vm_free(this,size);
    }
    void SetRoot(GSWeakRef *r)
    {
        __ObjRelease(_root);
        _root = r;
        __ObjAddRef(_root);
    }
    GSClosure *Clone()
    {
        GSFunctionProto *f = _function;
        GSClosure * ret = GSClosure::Create(_opt_ss(this),f,_root);
        ret->_env = _env;
        if(ret->_env) __ObjAddRef(ret->_env);
        _COPY_VECTOR(ret->_outervalues,_outervalues,f->_noutervalues);
        _COPY_VECTOR(ret->_defaultparams,_defaultparams,f->_ndefaultparams);
        return ret;
    }
    ~GSClosure();

    bool Save(GSVM *v,GSUserPointer up,GSWRITEFUNC write);
    static bool Load(GSVM *v,GSUserPointer up,GSREADFUNC read,GSObjectPtr &ret);
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    void Finalize(){
        GSFunctionProto *f = _function;
        _NULL_GSOBJECT_VECTOR(_outervalues,f->_noutervalues);
        _NULL_GSOBJECT_VECTOR(_defaultparams,f->_ndefaultparams);
    }
    GSObjectType GetType() {return OT_CLOSURE;}
#endif
    GSWeakRef *_env;
    GSWeakRef *_root;
    GSObjectPtrVec _imports;
    GSClass *_base;
    GSFunctionProto *_function;
    GSObjectPtr *_outervalues;
    GSObjectPtr *_defaultparams;
};

//////////////////////////////////////////////
struct GSOuter : public CHAINABLE_OBJ
{

private:
    GSOuter(GSSharedState *ss, GSObjectPtr *outer){_valptr = outer; _next = NULL; INIT_CHAIN(); ADD_TO_CHAIN(&_ss(this)->_gc_chain,this); }

public:
    static GSOuter *Create(GSSharedState *ss, GSObjectPtr *outer)
    {
        GSOuter *nc  = (GSOuter*)GS_MALLOC(sizeof(GSOuter));
        new (nc) GSOuter(ss, outer);
        return nc;
    }
    ~GSOuter() { REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this); }

    void Release()
    {
        this->~GSOuter();
        GS_vm_free(this,sizeof(GSOuter));
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    void Finalize() { _value.Null(); }
    GSObjectType GetType() {return OT_OUTER;}
#endif

    GSObjectPtr *_valptr;  /* pointer to value on stack, or _value below */
    GSInteger    _idx;     /* idx in stack array, for relocation */
    GSObjectPtr  _value;   /* value of outer after stack frame is closed */
    GSOuter     *_next;    /* pointer to next outer when frame is open   */
};

//////////////////////////////////////////////
struct GSGenerator : public CHAINABLE_OBJ
{
    enum GSGeneratorState{eRunning,eSuspended,eDead};
private:
    GSGenerator(GSSharedState *ss,GSClosure *closure){_closure=closure;_state=eRunning;_ci._generator=NULL;INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);}
public:
    static GSGenerator *Create(GSSharedState *ss,GSClosure *closure){
        GSGenerator *nc=(GSGenerator*)GS_MALLOC(sizeof(GSGenerator));
        new (nc) GSGenerator(ss,closure);
        return nc;
    }
    ~GSGenerator()
    {
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
    }
    void Kill(){
        _state=eDead;
        _stack.resize(0);
        _closure.Null();}
    void Release(){
        GS_delete(this,GSGenerator);
    }

    bool Yield(GSVM *v,GSInteger target);
    bool Resume(GSVM *v,GSObjectPtr &dest);
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    void Finalize(){_stack.resize(0);_closure.Null();}
    GSObjectType GetType() {return OT_GENERATOR;}
#endif
    GSObjectPtr _closure;
    GSObjectPtrVec _stack;
    GSVM::CallInfo _ci;
    ExceptionsTraps _etraps;
    GSGeneratorState _state;
};

#define _CALC_NATVIVECLOSURE_SIZE(noutervalues) (sizeof(GSNativeClosure) + (noutervalues*sizeof(GSObjectPtr)))

struct GSNativeClosure : public CHAINABLE_OBJ
{
private:
    GSNativeClosure(GSSharedState *ss,GSFUNCTION func){_function=func;INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this); _env = NULL;}
public:
    static GSNativeClosure *Create(GSSharedState *ss,GSFUNCTION func,GSInteger nouters)
    {
        GSInteger size = _CALC_NATVIVECLOSURE_SIZE(nouters);
        GSNativeClosure *nc=(GSNativeClosure*)GS_MALLOC(size);
        new (nc) GSNativeClosure(ss,func);
        nc->_outervalues = (GSObjectPtr *)(nc + 1);
        nc->_noutervalues = nouters;
        _CONSTRUCT_VECTOR(GSObjectPtr,nc->_noutervalues,nc->_outervalues);
        return nc;
    }
    GSNativeClosure *Clone()
    {
        GSNativeClosure * ret = GSNativeClosure::Create(_opt_ss(this),_function,_noutervalues);
        ret->_env = _env;
        if(ret->_env) __ObjAddRef(ret->_env);
        ret->_name = _name;
        _COPY_VECTOR(ret->_outervalues,_outervalues,_noutervalues);
        ret->_typecheck.copy(_typecheck);
        ret->_nparamscheck = _nparamscheck;
        return ret;
    }
    ~GSNativeClosure()
    {
        __ObjRelease(_env);
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
    }
    void Release(){
        GSInteger size = _CALC_NATVIVECLOSURE_SIZE(_noutervalues);
        _DESTRUCT_VECTOR(GSObjectPtr,_noutervalues,_outervalues);
        this->~GSNativeClosure();
        GS_free(this,size);
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    void Finalize() { _NULL_GSOBJECT_VECTOR(_outervalues,_noutervalues); }
    GSObjectType GetType() {return OT_NATIVECLOSURE;}
#endif
    GSInteger _nparamscheck;
    GSIntVec _typecheck;
    GSObjectPtr *_outervalues;
    GSUnsignedInteger _noutervalues;
    GSWeakRef *_env;
    GSFUNCTION _function;
    GSObjectPtr _name;
};



#endif //_GSCLOSURE_H_

