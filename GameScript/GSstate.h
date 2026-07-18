/*  see copyright notice in GameScript.h */
#ifndef _GSSTATE_H_
#define _GSSTATE_H_

#include "GSutils.h"
#include "GSobject.h"
struct GSString;
struct GSTable;
//max number of character for a printed number
#define NUMBER_MAX_CHAR 50

struct GSStringTable
{
    GSStringTable(GSSharedState*ss);
    ~GSStringTable();
    GSString *Add(const GSChar *,GSInteger len);
    GSString* Concat(const GSChar* a, GSInteger alen, const GSChar* b, GSInteger blen);
    void Remove(GSString *);
private:
    void Resize(GSInteger size);
    void AllocNodes(GSInteger size);
    GSString **_strings;
    GSUnsignedInteger _numofslots;
    GSUnsignedInteger _slotused;
    GSSharedState *_sharedstate;
};

struct RefTable {
    struct RefNode {
        GSObjectPtr obj;
        GSUnsignedInteger refs;
        struct RefNode *next;
    };
    RefTable();
    ~RefTable();
    void AddRef(GSObject &obj);
    GSBool Release(GSObject &obj);
    GSUnsignedInteger GetRefCount(GSObject &obj);
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
#endif
    void Finalize();
private:
    RefNode *Get(GSObject &obj,GSHash &mainpos,RefNode **prev,bool add);
    RefNode *Add(GSHash mainpos,GSObject &obj);
    void Resize(GSUnsignedInteger size);
    void AllocNodes(GSUnsignedInteger size);
    GSUnsignedInteger _numofslots;
    GSUnsignedInteger _slotused;
    RefNode *_nodes;
    RefNode *_freelist;
    RefNode **_buckets;
};

#define ADD_STRING(ss,str,len) ss->_stringtable->Add(str,len)
#define REMOVE_STRING(ss,bstr) ss->_stringtable->Remove(bstr)

struct GSObjectPtr;

struct GSSharedState
{
    GSSharedState();
    ~GSSharedState();
    void Init();
public:
    GSChar* GetScratchPad(GSInteger size);
    GSInteger GetMetaMethodIdxByName(const GSObjectPtr &name);
#ifndef NO_GARBAGE_COLLECTOR
    GSInteger CollectGarbage(GSVM *vm);
    void RunMark(GSVM *vm,GSCollectable **tchain);
    GSInteger ResurrectUnreachable(GSVM *vm);
    static void MarkObject(GSObjectPtr &o,GSCollectable **chain);
#endif
    GSObjectPtrVec *_metamethods;
    GSObjectPtr _metamethodsmap;
    GSObjectPtrVec *_systemstrings;
    GSObjectPtrVec *_types;
    GSStringTable *_stringtable;
    RefTable _refs_table;
    GSObjectPtr _registry;
    GSObjectPtr _consts;
    GSObjectPtr _constructoridx;
#ifndef NO_GARBAGE_COLLECTOR
    GSCollectable *_gc_chain;
#endif
    GSObjectPtr _root_vm;
    GSObjectPtr _table_default_delegate;
    static const GSRegFunction _table_default_delegate_funcz[];
    GSObjectPtr _array_default_delegate;
    static const GSRegFunction _array_default_delegate_funcz[];
    GSObjectPtr _string_default_delegate;
    static const GSRegFunction _string_default_delegate_funcz[];
    GSObjectPtr _number_default_delegate;
    static const GSRegFunction _number_default_delegate_funcz[];
    GSObjectPtr _generator_default_delegate;
    static const GSRegFunction _generator_default_delegate_funcz[];
    GSObjectPtr _closure_default_delegate;
    static const GSRegFunction _closure_default_delegate_funcz[];
    GSObjectPtr _thread_default_delegate;
    static const GSRegFunction _thread_default_delegate_funcz[];
    GSObjectPtr _class_default_delegate;
    static const GSRegFunction _class_default_delegate_funcz[];
    GSObjectPtr _instance_default_delegate;
    static const GSRegFunction _instance_default_delegate_funcz[];
    GSObjectPtr _weakref_default_delegate;
    static const GSRegFunction _weakref_default_delegate_funcz[];

    GSCOMPILERERROR _compilererrorhandler;
    GSPRINTFUNCTION _printfunc;
    GSPRINTFUNCTION _errorfunc;
    bool _debuginfo;
    bool _notifyallexceptions;
    GSUserPointer _foreignptr;
    GSRELEASEHOOK _releasehook;
private:
    GSChar *_scratchpad;
    GSInteger _scratchpadsize;
};

#define _sp(s) (_sharedstate->GetScratchPad(s))
#define _spval (_sharedstate->GetScratchPad(-1))

#define _table_ddel     _table(_sharedstate->_table_default_delegate)
#define _array_ddel     _table(_sharedstate->_array_default_delegate)
#define _string_ddel    _table(_sharedstate->_string_default_delegate)
#define _number_ddel    _table(_sharedstate->_number_default_delegate)
#define _generator_ddel _table(_sharedstate->_generator_default_delegate)
#define _closure_ddel   _table(_sharedstate->_closure_default_delegate)
#define _thread_ddel    _table(_sharedstate->_thread_default_delegate)
#define _class_ddel     _table(_sharedstate->_class_default_delegate)
#define _instance_ddel  _table(_sharedstate->_instance_default_delegate)
#define _weakref_ddel   _table(_sharedstate->_weakref_default_delegate)

bool CompileTypemask(GSIntVec &res,const GSChar *typemask);


#endif //_GSSTATE_H_

