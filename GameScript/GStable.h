/*  see copyright notice in GameScript.h */
#ifndef _GSTABLE_H_
#define _GSTABLE_H_
/*
* The following code is based on Lua 4.0 (Copyright 1994-2002 Tecgraf, PUC-Rio.)
* http://www.lua.org/copyright.html#4
* http://www.lua.org/source/4.0.1/src_ltable.c.html
*/

#include "GSstring.h"


#define hashptr(p)  ((GSHash)(((GSInteger)p) >> 3))

inline GSHash HashObj(const GSObject &key)
{
    switch(GS_type(key)) {
        case OT_STRING:     return _string(key)->_hash;
        case OT_FLOAT:      return (GSHash)((GSInteger)_float(key));
        case OT_BOOL: case OT_INTEGER:  return (GSHash)((GSInteger)_integer(key));
        default:            return hashptr(key._unVal.pRefCounted);
    }
}

struct GSTable : public GSDelegable
{
private:
    struct _HashNode
    {
        _HashNode() { next = NULL; }
        GSObjectPtr val;
        GSObjectPtr key;
        _HashNode *next;
    };
    _HashNode *_firstfree;
    _HashNode *_nodes;
    GSInteger _numofnodes;
    GSInteger _usednodes;

///////////////////////////
    void AllocNodes(GSInteger nSize);
    void Rehash(bool force);
    GSTable(GSSharedState *ss, GSInteger nInitialSize);
    void _ClearNodes();
public:
    static GSTable* Create(GSSharedState *ss,GSInteger nInitialSize)
    {
        GSTable *newtable = (GSTable*)GS_MALLOC(sizeof(GSTable));
        new (newtable) GSTable(ss, nInitialSize);
        newtable->_delegate = NULL;
        return newtable;
    }
    void Finalize();
    GSTable *Clone();
    ~GSTable()
    {
        SetDelegate(NULL);
        REMOVE_FROM_CHAIN(&_sharedstate->_gc_chain, this);
        for (GSInteger i = 0; i < _numofnodes; i++) _nodes[i].~_HashNode();
        GS_FREE(_nodes, _numofnodes * sizeof(_HashNode));
    }
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    GSObjectType GetType() {return OT_TABLE;}
#endif
    inline _HashNode *_Get(const GSObjectPtr &key,GSHash hash)
    {
        _HashNode *n = &_nodes[hash];
        do{
            if(_rawval(n->key) == _rawval(key) && GS_type(n->key) == GS_type(key)){
                return n;
            }
        }while((n = n->next));
        return NULL;
    }
    //for compiler use
    inline bool GetStr(const GSChar* key,GSInteger keylen,GSObjectPtr &val)
    {
        GSHash hash = _hashstr(key,keylen);
        _HashNode *n = &_nodes[hash & (_numofnodes - 1)];
        _HashNode *res = NULL;
        do{
            if(GS_type(n->key) == OT_STRING && (scstrcmp(_stringval(n->key),key) == 0)){
                res = n;
                break;
            }
        }while((n = n->next));
        if (res) {
            val = _realval(res->val);
            return true;
        }
        return false;
    }
    bool Get(const GSObjectPtr &key,GSObjectPtr &val);
    void Remove(const GSObjectPtr &key);
    bool Set(const GSObjectPtr &key, const GSObjectPtr &val);
    //returns true if a new slot has been created false if it was already present
    bool NewSlot(const GSObjectPtr &key,const GSObjectPtr &val);
    GSInteger Next(bool getweakrefs,const GSObjectPtr &refpos, GSObjectPtr &outkey, GSObjectPtr &outval);

    GSInteger CountUsed(){ return _usednodes;}
    void Clear();
    void Release()
    {
        GS_delete(this, GSTable);
    }

};

#endif //_GSTABLE_H_

