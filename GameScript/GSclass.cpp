/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#include "GSvm.h"
#include "GStable.h"
#include "GSclass.h"
#include "GSfuncproto.h"
#include "GSclosure.h"



GSClass::GSClass(GSSharedState *ss,GSClass *base)
{
    _base = base;
    _typetag = 0;
    _hook = NULL;
    _udsize = 0;
    _locked = false;
    _constructoridx = -1;
    if(_base) {
        _constructoridx = _base->_constructoridx;
        _udsize = _base->_udsize;
        _defaultvalues.copy(base->_defaultvalues);
        _methods.copy(base->_methods);
        _COPY_VECTOR(_metamethods,base->_metamethods,MT_LAST);
        __ObjAddRef(_base);
    }
    _members = base?base->_members->Clone() : GSTable::Create(ss,0);
    __ObjAddRef(_members);

    INIT_CHAIN();
    ADD_TO_CHAIN(&_sharedstate->_gc_chain, this);
}

void GSClass::Finalize() {
    _attributes.Null();
    _NULL_GSOBJECT_VECTOR(_defaultvalues,_defaultvalues.size());
    _methods.resize(0);
    _NULL_GSOBJECT_VECTOR(_metamethods,MT_LAST);
    __ObjRelease(_members);
    if(_base) {
        __ObjRelease(_base);
    }
}

GSClass::~GSClass()
{
    REMOVE_FROM_CHAIN(&_sharedstate->_gc_chain, this);
    Finalize();
}

bool GSClass::NewSlot(GSSharedState *ss,const GSObjectPtr &key,const GSObjectPtr &val,bool bstatic)
{
    GSObjectPtr temp;
    bool belongs_to_static_table = GS_type(val) == OT_CLOSURE || GS_type(val) == OT_NATIVECLOSURE || bstatic;
    if(_locked && !belongs_to_static_table)
        return false; //the class already has an instance so cannot be modified
    if(_members->Get(key,temp) && _isfield(temp)) //overrides the default value
    {
        _defaultvalues[_member_idx(temp)].val = val;
        return true;
    }
	if (_members->CountUsed() >= MEMBER_MAX_COUNT) {
		return false;
	}
    if(belongs_to_static_table) {
        GSInteger mmidx;
        if((GS_type(val) == OT_CLOSURE || GS_type(val) == OT_NATIVECLOSURE) &&
            (mmidx = ss->GetMetaMethodIdxByName(key)) != -1) {
            _metamethods[mmidx] = val;
        }
        else {
            GSObjectPtr theval = val;
            if(_base && GS_type(val) == OT_CLOSURE) {
                theval = _closure(val)->Clone();
                _closure(theval)->_base = _base;
                __ObjAddRef(_base); //ref for the closure
            }
            if(GS_type(temp) == OT_NULL) {
                bool isconstructor;
                GSVM::IsEqual(ss->_constructoridx, key, isconstructor);
                if(isconstructor) {
                    _constructoridx = (GSInteger)_methods.size();
                }
                GSClassMember m;
                m.val = theval;
                _members->NewSlot(key,GSObjectPtr(_make_method_idx(_methods.size())));
                _methods.push_back(m);
            }
            else {
                _methods[_member_idx(temp)].val = theval;
            }
        }
        return true;
    }
    GSClassMember m;
    m.val = val;
    _members->NewSlot(key,GSObjectPtr(_make_field_idx(_defaultvalues.size())));
    _defaultvalues.push_back(m);
    return true;
}

GSInstance *GSClass::CreateInstance()
{
    if(!_locked) Lock();
    return GSInstance::Create(_opt_ss(this),this);
}

GSInteger GSClass::Next(const GSObjectPtr &refpos, GSObjectPtr &outkey, GSObjectPtr &outval)
{
    GSObjectPtr oval;
    GSInteger idx = _members->Next(false,refpos,outkey,oval);
    if(idx != -1) {
        if(_ismethod(oval)) {
            outval = _methods[_member_idx(oval)].val;
        }
        else {
            GSObjectPtr &o = _defaultvalues[_member_idx(oval)].val;
            outval = _realval(o);
        }
    }
    return idx;
}

bool GSClass::SetAttributes(const GSObjectPtr &key,const GSObjectPtr &val)
{
    GSObjectPtr idx;
    if(_members->Get(key,idx)) {
        if(_isfield(idx))
            _defaultvalues[_member_idx(idx)].attrs = val;
        else
            _methods[_member_idx(idx)].attrs = val;
        return true;
    }
    return false;
}

bool GSClass::GetAttributes(const GSObjectPtr &key,GSObjectPtr &outval)
{
    GSObjectPtr idx;
    if(_members->Get(key,idx)) {
        outval = (_isfield(idx)?_defaultvalues[_member_idx(idx)].attrs:_methods[_member_idx(idx)].attrs);
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////
void GSInstance::Init(GSSharedState *ss)
{
    _userpointer = NULL;
    _hook = NULL;
    __ObjAddRef(_class);
    _delegate = _class->_members;
    INIT_CHAIN();
    ADD_TO_CHAIN(&_sharedstate->_gc_chain, this);
}

GSInstance::GSInstance(GSSharedState *ss, GSClass *c, GSInteger memsize)
{
    _memsize = memsize;
    _class = c;
    GSUnsignedInteger nvalues = _class->_defaultvalues.size();
    for(GSUnsignedInteger n = 0; n < nvalues; n++) {
        new (&_values[n]) GSObjectPtr(_class->_defaultvalues[n].val);
    }
    Init(ss);
}

GSInstance::GSInstance(GSSharedState *ss, GSInstance *i, GSInteger memsize)
{
    _memsize = memsize;
    _class = i->_class;
    GSUnsignedInteger nvalues = _class->_defaultvalues.size();
    for(GSUnsignedInteger n = 0; n < nvalues; n++) {
        new (&_values[n]) GSObjectPtr(i->_values[n]);
    }
    Init(ss);
}

void GSInstance::Finalize()
{
    GSUnsignedInteger nvalues = _class->_defaultvalues.size();
    __ObjRelease(_class);
    _NULL_GSOBJECT_VECTOR(_values,nvalues);
}

GSInstance::~GSInstance()
{
    REMOVE_FROM_CHAIN(&_sharedstate->_gc_chain, this);
    if(_class){ Finalize(); } //if _class is null it was already finalized by the GC
}

bool GSInstance::GetMetaMethod(GSVM* GS_UNUSED_ARG(v),GSMetaMethod mm,GSObjectPtr &res)
{
    if(GS_type(_class->_metamethods[mm]) != OT_NULL) {
        res = _class->_metamethods[mm];
        return true;
    }
    return false;
}

bool GSInstance::InstanceOf(GSClass *trg)
{
    GSClass *parent = _class;
    while(parent != NULL) {
        if(parent == trg)
            return true;
        parent = parent->_base;
    }
    return false;
}

