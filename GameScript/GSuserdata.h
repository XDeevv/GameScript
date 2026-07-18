/*  see copyright notice in GameScript.h */
#ifndef _GSUSERDATA_H_
#define _GSUSERDATA_H_

struct GSUserData : GSDelegable
{
    GSUserData(GSSharedState *ss){ _delegate = 0; _hook = NULL; INIT_CHAIN(); ADD_TO_CHAIN(&_ss(this)->_gc_chain, this); }
    ~GSUserData()
    {
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain, this);
        SetDelegate(NULL);
    }
    static GSUserData* Create(GSSharedState *ss, GSInteger size)
    {
        GSUserData* ud = (GSUserData*)GS_MALLOC(GS_aligning(sizeof(GSUserData))+size);
        new (ud) GSUserData(ss);
        ud->_size = size;
        ud->_typetag = 0;
        return ud;
    }
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    void Finalize(){SetDelegate(NULL);}
    GSObjectType GetType(){ return OT_USERDATA;}
#endif
    void Release() {
        if (_hook) _hook((GSUserPointer)GS_aligning(this + 1),_size);
        GSInteger tsize = _size;
        this->~GSUserData();
        GS_FREE(this, GS_aligning(sizeof(GSUserData)) + tsize);
    }


    GSInteger _size;
    GSRELEASEHOOK _hook;
    GSUserPointer _typetag;
    //GSChar _val[1];
};

#endif //_GSUSERDATA_H_

