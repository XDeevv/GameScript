/*  see copyright notice in GameScript.h */
#ifndef _GSOBJECT_H_
#define _GSOBJECT_H_

#include "GSutils.h"

#ifdef _GS64
#define UINT_MINUS_ONE (0xFFFFFFFFFFFFFFFF)
#else
#define UINT_MINUS_ONE (0xFFFFFFFF)
#endif

#define GS_CLOSURESTREAM_HEAD (('S'<<24)|('Q'<<16)|('I'<<8)|('R'))
#define GS_CLOSURESTREAM_PART (('P'<<24)|('A'<<16)|('R'<<8)|('T'))
#define GS_CLOSURESTREAM_TAIL (('T'<<24)|('A'<<16)|('I'<<8)|('L'))

struct GSSharedState;

enum GSMetaMethod{
    MT_ADD=0,
    MT_SUB=1,
    MT_MUL=2,
    MT_DIV=3,
    MT_UNM=4,
    MT_MODULO=5,
    MT_SET=6,
    MT_GET=7,
    MT_TYPEOF=8,
    MT_NEXTI=9,
    MT_CMP=10,
    MT_CALL=11,
    MT_CLONED=12,
    MT_NEWSLOT=13,
    MT_DELSLOT=14,
    MT_TOSTRING=15,
    MT_NEWMEMBER=16,
    MT_INHERITED=17,
    MT_LAST = 18
};

#define MM_ADD      _SC("_add")
#define MM_SUB      _SC("_sub")
#define MM_MUL      _SC("_mul")
#define MM_DIV      _SC("_div")
#define MM_UNM      _SC("_unm")
#define MM_MODULO   _SC("_modulo")
#define MM_SET      _SC("_set")
#define MM_GET      _SC("_get")
#define MM_TYPEOF   _SC("_typeof")
#define MM_NEXTI    _SC("_nexti")
#define MM_CMP      _SC("_cmp")
#define MM_CALL     _SC("_call")
#define MM_CLONED   _SC("_cloned")
#define MM_NEWSLOT  _SC("_newslot")
#define MM_DELSLOT  _SC("_delslot")
#define MM_TOSTRING _SC("_tostring")
#define MM_NEWMEMBER _SC("_newmember")
#define MM_INHERITED _SC("_inherited")


#define _CONSTRUCT_VECTOR(type,size,ptr) { \
    for(GSInteger n = 0; n < ((GSInteger)size); n++) { \
            new (&ptr[n]) type(); \
        } \
}

#define _DESTRUCT_VECTOR(type,size,ptr) { \
    for(GSInteger nl = 0; nl < ((GSInteger)size); nl++) { \
            ptr[nl].~type(); \
    } \
}

#define _COPY_VECTOR(dest,src,size) { \
    for(GSInteger _n_ = 0; _n_ < ((GSInteger)size); _n_++) { \
        dest[_n_] = src[_n_]; \
    } \
}

#define _NULL_GSOBJECT_VECTOR(vec,size) { \
    for(GSInteger _n_ = 0; _n_ < ((GSInteger)size); _n_++) { \
        vec[_n_].Null(); \
    } \
}

#define MINPOWER2 4

struct GSRefCounted
{
    GSUnsignedInteger _uiRef;
    struct GSWeakRef *_weakref;
    GSRefCounted() { _uiRef = 0; _weakref = NULL; }
    virtual ~GSRefCounted();
    GSWeakRef *GetWeakRef(GSObjectType type);
    virtual void Release()=0;

};

struct GSWeakRef : GSRefCounted
{
    void Release();
    GSObject _obj;
};

#define _realval(o) (GS_type((o)) != OT_WEAKREF?(GSObject)o:_weakref(o)->_obj)

struct GSObjectPtr;

#define __AddRef(type,unval) if(ISREFCOUNTED(type)) \
        { \
            unval.pRefCounted->_uiRef++; \
        }

#define __Release(type,unval) if(ISREFCOUNTED(type) && ((--unval.pRefCounted->_uiRef)==0))  \
        {   \
            unval.pRefCounted->Release();   \
        }

#define __ObjRelease(obj) { \
    if((obj)) { \
        (obj)->_uiRef--; \
        if((obj)->_uiRef == 0) \
            (obj)->Release(); \
        (obj) = NULL;   \
    } \
}

#define __ObjAddRef(obj) { \
    (obj)->_uiRef++; \
}

#define is_delegable(t) (GS_type(t)&GSOBJECT_DELEGABLE)
#define raw_type(obj) _RAW_TYPE((obj)._type)

#define _integer(obj) ((obj)._unVal.nInteger)
#define _float(obj) ((obj)._unVal.fFloat)
#define _string(obj) ((obj)._unVal.pString)
#define _table(obj) ((obj)._unVal.pTable)
#define _array(obj) ((obj)._unVal.pArray)
#define _closure(obj) ((obj)._unVal.pClosure)
#define _generator(obj) ((obj)._unVal.pGenerator)
#define _nativeclosure(obj) ((obj)._unVal.pNativeClosure)
#define _userdata(obj) ((obj)._unVal.pUserData)
#define _userpointer(obj) ((obj)._unVal.pUserPointer)
#define _thread(obj) ((obj)._unVal.pThread)
#define _funcproto(obj) ((obj)._unVal.pFunctionProto)
#define _class(obj) ((obj)._unVal.pClass)
#define _instance(obj) ((obj)._unVal.pInstance)
#define _delegable(obj) ((GSDelegable *)(obj)._unVal.pDelegable)
#define _weakref(obj) ((obj)._unVal.pWeakRef)
#define _outer(obj) ((obj)._unVal.pOuter)
#define _refcounted(obj) ((obj)._unVal.pRefCounted)
#define _rawval(obj) ((obj)._unVal.raw)

#define _stringval(obj) (obj)._unVal.pString->_val
#define _userdataval(obj) ((GSUserPointer)GS_aligning((obj)._unVal.pUserData + 1))

#define tofloat(num) ((GS_type(num)==OT_INTEGER)?(GSFloat)_integer(num):_float(num))
#define tointeger(num) ((GS_type(num)==OT_FLOAT)?(GSInteger)_float(num):_integer(num))
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
#if defined(GSUSEDOUBLE) && !defined(_GS64) || !defined(GSUSEDOUBLE) && defined(_GS64)
#define GS_REFOBJECT_INIT() GS_OBJECT_RAWINIT()
#else
#define GS_REFOBJECT_INIT()
#endif

#define _REF_TYPE_DECL(type,_class,sym) \
    GSObjectPtr(_class * x) \
    { \
        GS_OBJECT_RAWINIT() \
        _type=type; \
        _unVal.sym = x; \
        assert(_unVal.pTable); \
        _unVal.pRefCounted->_uiRef++; \
    } \
    inline GSObjectPtr& operator=(_class *x) \
    {  \
        GSObjectType tOldType; \
        GSObjectValue unOldVal; \
        tOldType=_type; \
        unOldVal=_unVal; \
        _type = type; \
        GS_REFOBJECT_INIT() \
        _unVal.sym = x; \
        _unVal.pRefCounted->_uiRef++; \
        __Release(tOldType,unOldVal); \
        return *this; \
    }

#define _SCALAR_TYPE_DECL(type,_class,sym) \
    GSObjectPtr(_class x) \
    { \
        GS_OBJECT_RAWINIT() \
        _type=type; \
        _unVal.sym = x; \
    } \
    inline GSObjectPtr& operator=(_class x) \
    {  \
        __Release(_type,_unVal); \
        _type = type; \
        GS_OBJECT_RAWINIT() \
        _unVal.sym = x; \
        return *this; \
    }
struct GSObjectPtr : public GSObject
{
    GSObjectPtr()
    {
        GS_OBJECT_RAWINIT()
        _type=OT_NULL;
        _unVal.pUserPointer=NULL;
    }
    GSObjectPtr(const GSObjectPtr &o)
    {
        _type = o._type;
        _unVal = o._unVal;
        __AddRef(_type,_unVal);
    }
    GSObjectPtr(const GSObject &o)
    {
        _type = o._type;
        _unVal = o._unVal;
        __AddRef(_type,_unVal);
    }
    _REF_TYPE_DECL(OT_TABLE,GSTable,pTable)
    _REF_TYPE_DECL(OT_CLASS,GSClass,pClass)
    _REF_TYPE_DECL(OT_INSTANCE,GSInstance,pInstance)
    _REF_TYPE_DECL(OT_ARRAY,GSArray,pArray)
    _REF_TYPE_DECL(OT_CLOSURE,GSClosure,pClosure)
    _REF_TYPE_DECL(OT_NATIVECLOSURE,GSNativeClosure,pNativeClosure)
    _REF_TYPE_DECL(OT_OUTER,GSOuter,pOuter)
    _REF_TYPE_DECL(OT_GENERATOR,GSGenerator,pGenerator)
    _REF_TYPE_DECL(OT_STRING,GSString,pString)
    _REF_TYPE_DECL(OT_USERDATA,GSUserData,pUserData)
    _REF_TYPE_DECL(OT_WEAKREF,GSWeakRef,pWeakRef)
    _REF_TYPE_DECL(OT_THREAD,GSVM,pThread)
    _REF_TYPE_DECL(OT_FUNCPROTO,GSFunctionProto,pFunctionProto)

    _SCALAR_TYPE_DECL(OT_INTEGER,GSInteger,nInteger)
    _SCALAR_TYPE_DECL(OT_FLOAT,GSFloat,fFloat)
    _SCALAR_TYPE_DECL(OT_USERPOINTER,GSUserPointer,pUserPointer)

    GSObjectPtr(bool bBool)
    {
        GS_OBJECT_RAWINIT()
        _type = OT_BOOL;
        _unVal.nInteger = bBool?1:0;
    }
    inline GSObjectPtr& operator=(bool b)
    {
        __Release(_type,_unVal);
        GS_OBJECT_RAWINIT()
        _type = OT_BOOL;
        _unVal.nInteger = b?1:0;
        return *this;
    }

    ~GSObjectPtr()
    {
        __Release(_type,_unVal);
    }

    inline GSObjectPtr& operator=(const GSObjectPtr& obj)
    {
        GSObjectType tOldType;
        GSObjectValue unOldVal;
        tOldType=_type;
        unOldVal=_unVal;
        _unVal = obj._unVal;
        _type = obj._type;
        __AddRef(_type,_unVal);
        __Release(tOldType,unOldVal);
        return *this;
    }
    inline GSObjectPtr& operator=(const GSObject& obj)
    {
        GSObjectType tOldType;
        GSObjectValue unOldVal;
        tOldType=_type;
        unOldVal=_unVal;
        _unVal = obj._unVal;
        _type = obj._type;
        __AddRef(_type,_unVal);
        __Release(tOldType,unOldVal);
        return *this;
    }
    inline void Null()
    {
        GSObjectType tOldType = _type;
        GSObjectValue unOldVal = _unVal;
        _type = OT_NULL;
        _unVal.raw = (GSRawObjectVal)NULL;
        __Release(tOldType ,unOldVal);
    }
    private:
        GSObjectPtr(const GSChar *){} //safety
};


inline void _Swap(GSObject &a,GSObject &b)
{
    GSObjectType tOldType = a._type;
    GSObjectValue unOldVal = a._unVal;
    a._type = b._type;
    a._unVal = b._unVal;
    b._type = tOldType;
    b._unVal = unOldVal;
}

/////////////////////////////////////////////////////////////////////////////////////
#ifndef NO_GARBAGE_COLLECTOR
#define MARK_FLAG 0x80000000
struct GSCollectable : public GSRefCounted {
    GSCollectable *_next;
    GSCollectable *_prev;
    GSSharedState *_sharedstate;
    virtual GSObjectType GetType()=0;
    virtual void Release()=0;
    virtual void Mark(GSCollectable **chain)=0;
    void UnMark();
    virtual void Finalize()=0;
    static void AddToChain(GSCollectable **chain,GSCollectable *c);
    static void RemoveFromChain(GSCollectable **chain,GSCollectable *c);
};


#define ADD_TO_CHAIN(chain,obj) AddToChain(chain,obj)
#define REMOVE_FROM_CHAIN(chain,obj) {if(!(_uiRef&MARK_FLAG))RemoveFromChain(chain,obj);}
#define CHAINABLE_OBJ GSCollectable
#define INIT_CHAIN() {_next=NULL;_prev=NULL;_sharedstate=ss;}
#else

#define ADD_TO_CHAIN(chain,obj) ((void)0)
#define REMOVE_FROM_CHAIN(chain,obj) ((void)0)
#define CHAINABLE_OBJ GSRefCounted
#define INIT_CHAIN() ((void)0)
#endif

struct GSDelegable : public CHAINABLE_OBJ {
    bool SetDelegate(GSTable *m);
    virtual bool GetMetaMethod(GSVM *v,GSMetaMethod mm,GSObjectPtr &res);
    GSTable *_delegate;
};

GSUnsignedInteger TranslateIndex(const GSObjectPtr &idx);
typedef GSvector<GSObjectPtr> GSObjectPtrVec;
typedef GSvector<GSInteger> GSIntVec;
const GSChar *GetTypeName(const GSObjectPtr &obj1);
const GSChar *IdType2Name(GSObjectType type);



#endif //_GSOBJECT_H_

