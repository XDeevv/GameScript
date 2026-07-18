/*  see copyright notice in GameScript.h */
#ifndef _GSCLASS_H_
#define _GSCLASS_H_

struct GSInstance;

struct GSClassMember {
    GSObjectPtr val;
    GSObjectPtr attrs;
    void Null() {
        val.Null();
        attrs.Null();
    }
};

typedef GSvector<GSClassMember> GSClassMemberVec;

#define MEMBER_TYPE_METHOD 0x01000000
#define MEMBER_TYPE_FIELD 0x02000000
#define MEMBER_MAX_COUNT 0x00FFFFFF

#define _ismethod(o) (_integer(o)&MEMBER_TYPE_METHOD)
#define _isfield(o) (_integer(o)&MEMBER_TYPE_FIELD)
#define _make_method_idx(i) ((GSInteger)(MEMBER_TYPE_METHOD|i))
#define _make_field_idx(i) ((GSInteger)(MEMBER_TYPE_FIELD|i))
#define _member_type(o) (_integer(o)&0xFF000000)
#define _member_idx(o) (_integer(o)&0x00FFFFFF)

struct GSClass : public CHAINABLE_OBJ
{
    GSClass(GSSharedState *ss,GSClass *base);
public:
    static GSClass* Create(GSSharedState *ss,GSClass *base) {
        GSClass *newclass = (GSClass *)GS_MALLOC(sizeof(GSClass));
        new (newclass) GSClass(ss, base);
        return newclass;
    }
    ~GSClass();
    bool NewSlot(GSSharedState *ss, const GSObjectPtr &key,const GSObjectPtr &val,bool bstatic);
    bool Get(const GSObjectPtr &key,GSObjectPtr &val) {
        if(_members->Get(key,val)) {
            if(_isfield(val)) {
                GSObjectPtr &o = _defaultvalues[_member_idx(val)].val;
                val = _realval(o);
            }
            else {
                val = _methods[_member_idx(val)].val;
            }
            return true;
        }
        return false;
    }
    bool GetConstructor(GSObjectPtr &ctor)
    {
        if(_constructoridx != -1) {
            ctor = _methods[_constructoridx].val;
            return true;
        }
        return false;
    }
    bool SetAttributes(const GSObjectPtr &key,const GSObjectPtr &val);
    bool GetAttributes(const GSObjectPtr &key,GSObjectPtr &outval);
    void Lock() { _locked = true; if(_base) _base->Lock(); }
    void Release() {
        if (_hook) { _hook(_typetag,0);}
        GS_delete(this, GSClass);
    }
    void Finalize();
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable ** );
    GSObjectType GetType() {return OT_CLASS;}
#endif
    GSInteger Next(const GSObjectPtr &refpos, GSObjectPtr &outkey, GSObjectPtr &outval);
    GSInstance *CreateInstance();
    GSTable *_members;
    GSClass *_base;
    GSClassMemberVec _defaultvalues;
    GSClassMemberVec _methods;
    GSObjectPtr _metamethods[MT_LAST];
    GSObjectPtr _attributes;
    GSUserPointer _typetag;
    GSRELEASEHOOK _hook;
    bool _locked;
    GSInteger _constructoridx;
    GSInteger _udsize;
};

#define calcinstancesize(_theclass_) \
    (_theclass_->_udsize + GS_aligning(sizeof(GSInstance) +  (sizeof(GSObjectPtr)*(_theclass_->_defaultvalues.size()>0?_theclass_->_defaultvalues.size()-1:0))))

struct GSInstance : public GSDelegable
{
    void Init(GSSharedState *ss);
    GSInstance(GSSharedState *ss, GSClass *c, GSInteger memsize);
    GSInstance(GSSharedState *ss, GSInstance *c, GSInteger memsize);
public:
    static GSInstance* Create(GSSharedState *ss,GSClass *theclass) {

        GSInteger size = calcinstancesize(theclass);
        GSInstance *newinst = (GSInstance *)GS_MALLOC(size);
        new (newinst) GSInstance(ss, theclass,size);
        if(theclass->_udsize) {
            newinst->_userpointer = ((unsigned char *)newinst) + (size - theclass->_udsize);
        }
        return newinst;
    }
    GSInstance *Clone(GSSharedState *ss)
    {
        GSInteger size = calcinstancesize(_class);
        GSInstance *newinst = (GSInstance *)GS_MALLOC(size);
        new (newinst) GSInstance(ss, this,size);
        if(_class->_udsize) {
            newinst->_userpointer = ((unsigned char *)newinst) + (size - _class->_udsize);
        }
        return newinst;
    }
    ~GSInstance();
    bool Get(const GSObjectPtr &key,GSObjectPtr &val)  {
        if(_class->_members->Get(key,val)) {
            if(_isfield(val)) {
                GSObjectPtr &o = _values[_member_idx(val)];
                val = _realval(o);
            }
            else {
                val = _class->_methods[_member_idx(val)].val;
            }
            return true;
        }
        return false;
    }
    bool Set(const GSObjectPtr &key,const GSObjectPtr &val) {
        GSObjectPtr idx;
        if(_class->_members->Get(key,idx) && _isfield(idx)) {
            _values[_member_idx(idx)] = val;
            return true;
        }
        return false;
    }
    void Release() {
        _uiRef++;
        if (_hook) { _hook(_userpointer,0);}
        _uiRef--;
        if(_uiRef > 0) return;
        GSInteger size = _memsize;
        this->~GSInstance();
        GS_FREE(this, size);
    }
    void Finalize();
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable ** );
    GSObjectType GetType() {return OT_INSTANCE;}
#endif
    bool InstanceOf(GSClass *trg);
    bool GetMetaMethod(GSVM *v,GSMetaMethod mm,GSObjectPtr &res);

    GSClass *_class;
    GSUserPointer _userpointer;
    GSRELEASEHOOK _hook;
    GSInteger _memsize;
    GSObjectPtr _values[1];
};

#endif //_GSCLASS_H_

