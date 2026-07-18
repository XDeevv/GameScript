/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#include "GSopcodes.h"
#include "GSvm.h"
#include "GSfuncproto.h"
#include "GSclosure.h"
#include "GSstring.h"
#include "GStable.h"
#include "GSarray.h"
#include "GSuserdata.h"
#include "GSclass.h"

GSSharedState::GSSharedState()
{
    _compilererrorhandler = NULL;
    _printfunc = NULL;
    _errorfunc = NULL;
    _debuginfo = false;
    _notifyallexceptions = false;
    _foreignptr = NULL;
    _releasehook = NULL;
}

#define newsysstring(s) {   \
    _systemstrings->push_back(GSString::Create(this,s));    \
    }

#define newmetamethod(s) {  \
    _metamethods->push_back(GSString::Create(this,s));  \
    _table(_metamethodsmap)->NewSlot(_metamethods->back(),(GSInteger)(_metamethods->size()-1)); \
    }

bool CompileTypemask(GSIntVec &res,const GSChar *typemask)
{
    GSInteger i = 0;
    GSInteger mask = 0;
    while(typemask[i] != 0) {
        switch(typemask[i]) {
            case 'o': mask |= _RT_NULL; break;
            case 'i': mask |= _RT_INTEGER; break;
            case 'f': mask |= _RT_FLOAT; break;
            case 'n': mask |= (_RT_FLOAT | _RT_INTEGER); break;
            case 's': mask |= _RT_STRING; break;
            case 't': mask |= _RT_TABLE; break;
            case 'a': mask |= _RT_ARRAY; break;
            case 'u': mask |= _RT_USERDATA; break;
            case 'c': mask |= (_RT_CLOSURE | _RT_NATIVECLOSURE); break;
            case 'b': mask |= _RT_BOOL; break;
            case 'g': mask |= _RT_GENERATOR; break;
            case 'p': mask |= _RT_USERPOINTER; break;
            case 'v': mask |= _RT_THREAD; break;
            case 'x': mask |= _RT_INSTANCE; break;
            case 'y': mask |= _RT_CLASS; break;
            case 'r': mask |= _RT_WEAKREF; break;
            case '.': mask = -1; res.push_back(mask); i++; mask = 0; continue;
            case ' ': i++; continue; //ignores spaces
            default:
                return false;
        }
        i++;
        if(typemask[i] == '|') {
            i++;
            if(typemask[i] == 0)
                return false;
            continue;
        }
        res.push_back(mask);
        mask = 0;

    }
    return true;
}

GSTable *CreateDefaultDelegate(GSSharedState *ss,const GSRegFunction *funcz)
{
    GSInteger i=0;
    GSTable *t=GSTable::Create(ss,0);
    while(funcz[i].name!=0){
        GSNativeClosure *nc = GSNativeClosure::Create(ss,funcz[i].f,0);
        nc->_nparamscheck = funcz[i].nparamscheck;
        nc->_name = GSString::Create(ss,funcz[i].name);
        if(funcz[i].typemask && !CompileTypemask(nc->_typecheck,funcz[i].typemask))
            return NULL;
        t->NewSlot(GSString::Create(ss,funcz[i].name),nc);
        i++;
    }
    return t;
}

void GSSharedState::Init()
{
    _scratchpad=NULL;
    _scratchpadsize=0;
#ifndef NO_GARBAGE_COLLECTOR
    _gc_chain=NULL;
#endif
    _stringtable = (GSStringTable*)GS_MALLOC(sizeof(GSStringTable));
    new (_stringtable) GSStringTable(this);
    GS_new(_metamethods,GSObjectPtrVec);
    GS_new(_systemstrings,GSObjectPtrVec);
    GS_new(_types,GSObjectPtrVec);
    _metamethodsmap = GSTable::Create(this,MT_LAST-1);
    //adding type strings to avoid memory trashing
    //types names
    newsysstring(_SC("null"));
    newsysstring(_SC("table"));
    newsysstring(_SC("array"));
    newsysstring(_SC("closure"));
    newsysstring(_SC("string"));
    newsysstring(_SC("userdata"));
    newsysstring(_SC("integer"));
    newsysstring(_SC("float"));
    newsysstring(_SC("userpointer"));
    newsysstring(_SC("function"));
    newsysstring(_SC("generator"));
    newsysstring(_SC("thread"));
    newsysstring(_SC("class"));
    newsysstring(_SC("instance"));
    newsysstring(_SC("bool"));
    //meta methods
    newmetamethod(MM_ADD);
    newmetamethod(MM_SUB);
    newmetamethod(MM_MUL);
    newmetamethod(MM_DIV);
    newmetamethod(MM_UNM);
    newmetamethod(MM_MODULO);
    newmetamethod(MM_SET);
    newmetamethod(MM_GET);
    newmetamethod(MM_TYPEOF);
    newmetamethod(MM_NEXTI);
    newmetamethod(MM_CMP);
    newmetamethod(MM_CALL);
    newmetamethod(MM_CLONED);
    newmetamethod(MM_NEWSLOT);
    newmetamethod(MM_DELSLOT);
    newmetamethod(MM_TOSTRING);
    newmetamethod(MM_NEWMEMBER);
    newmetamethod(MM_INHERITED);

    _constructoridx = GSString::Create(this,_SC("constructor"));
    _registry = GSTable::Create(this,0);
    _consts = GSTable::Create(this,0);
    _table_default_delegate = CreateDefaultDelegate(this,_table_default_delegate_funcz);
    _array_default_delegate = CreateDefaultDelegate(this,_array_default_delegate_funcz);
    _string_default_delegate = CreateDefaultDelegate(this,_string_default_delegate_funcz);
    _number_default_delegate = CreateDefaultDelegate(this,_number_default_delegate_funcz);
    _closure_default_delegate = CreateDefaultDelegate(this,_closure_default_delegate_funcz);
    _generator_default_delegate = CreateDefaultDelegate(this,_generator_default_delegate_funcz);
    _thread_default_delegate = CreateDefaultDelegate(this,_thread_default_delegate_funcz);
    _class_default_delegate = CreateDefaultDelegate(this,_class_default_delegate_funcz);
    _instance_default_delegate = CreateDefaultDelegate(this,_instance_default_delegate_funcz);
    _weakref_default_delegate = CreateDefaultDelegate(this,_weakref_default_delegate_funcz);
}

GSSharedState::~GSSharedState()
{
    if(_releasehook) { _releasehook(_foreignptr,0); _releasehook = NULL; }
    _constructoridx.Null();
    _table(_registry)->Finalize();
    _table(_consts)->Finalize();
    _table(_metamethodsmap)->Finalize();
    _registry.Null();
    _consts.Null();
    _metamethodsmap.Null();
    while(!_systemstrings->empty()) {
        _systemstrings->back().Null();
        _systemstrings->pop_back();
    }
    _thread(_root_vm)->Finalize();
    _root_vm.Null();
    _table_default_delegate.Null();
    _array_default_delegate.Null();
    _string_default_delegate.Null();
    _number_default_delegate.Null();
    _closure_default_delegate.Null();
    _generator_default_delegate.Null();
    _thread_default_delegate.Null();
    _class_default_delegate.Null();
    _instance_default_delegate.Null();
    _weakref_default_delegate.Null();
    _refs_table.Finalize();
#ifndef NO_GARBAGE_COLLECTOR
    GSCollectable *t = _gc_chain;
    GSCollectable *nx = NULL;
    if(t) {
        t->_uiRef++;
        while(t) {
            t->Finalize();
            nx = t->_next;
            if(nx) nx->_uiRef++;
            if(--t->_uiRef == 0)
                t->Release();
            t = nx;
        }
    }
    assert(_gc_chain==NULL); //just to proove a theory
    while(_gc_chain){
        _gc_chain->_uiRef++;
        _gc_chain->Release();
    }
#endif

    GS_delete(_types,GSObjectPtrVec);
    GS_delete(_systemstrings,GSObjectPtrVec);
    GS_delete(_metamethods,GSObjectPtrVec);
    GS_delete(_stringtable,GSStringTable);
    if(_scratchpad)GS_FREE(_scratchpad,_scratchpadsize);
}


GSInteger GSSharedState::GetMetaMethodIdxByName(const GSObjectPtr &name)
{
    if(GS_type(name) != OT_STRING)
        return -1;
    GSObjectPtr ret;
    if(_table(_metamethodsmap)->Get(name,ret)) {
        return _integer(ret);
    }
    return -1;
}

#ifndef NO_GARBAGE_COLLECTOR

void GSSharedState::MarkObject(GSObjectPtr &o,GSCollectable **chain)
{
    switch(GS_type(o)){
    case OT_TABLE:_table(o)->Mark(chain);break;
    case OT_ARRAY:_array(o)->Mark(chain);break;
    case OT_USERDATA:_userdata(o)->Mark(chain);break;
    case OT_CLOSURE:_closure(o)->Mark(chain);break;
    case OT_NATIVECLOSURE:_nativeclosure(o)->Mark(chain);break;
    case OT_GENERATOR:_generator(o)->Mark(chain);break;
    case OT_THREAD:_thread(o)->Mark(chain);break;
    case OT_CLASS:_class(o)->Mark(chain);break;
    case OT_INSTANCE:_instance(o)->Mark(chain);break;
    case OT_OUTER:_outer(o)->Mark(chain);break;
    case OT_FUNCPROTO:_funcproto(o)->Mark(chain);break;
    default: break; //shutup compiler
    }
}

void GSSharedState::RunMark(GSVM* GS_UNUSED_ARG(vm),GSCollectable **tchain)
{
    GSVM *vms = _thread(_root_vm);

    vms->Mark(tchain);

    _refs_table.Mark(tchain);
    MarkObject(_registry,tchain);
    MarkObject(_consts,tchain);
    MarkObject(_metamethodsmap,tchain);
    MarkObject(_table_default_delegate,tchain);
    MarkObject(_array_default_delegate,tchain);
    MarkObject(_string_default_delegate,tchain);
    MarkObject(_number_default_delegate,tchain);
    MarkObject(_generator_default_delegate,tchain);
    MarkObject(_thread_default_delegate,tchain);
    MarkObject(_closure_default_delegate,tchain);
    MarkObject(_class_default_delegate,tchain);
    MarkObject(_instance_default_delegate,tchain);
    MarkObject(_weakref_default_delegate,tchain);

}

GSInteger GSSharedState::ResurrectUnreachable(GSVM *vm)
{
    GSInteger n=0;
    GSCollectable *tchain=NULL;

    RunMark(vm,&tchain);

    GSCollectable *resurrected = _gc_chain;
    GSCollectable *t = resurrected;

    _gc_chain = tchain;

    GSArray *ret = NULL;
    if(resurrected) {
        ret = GSArray::Create(this,0);
        GSCollectable *rlast = NULL;
        while(t) {
            rlast = t;
            GSObjectType type = t->GetType();
            if(type != OT_FUNCPROTO && type != OT_OUTER) {
                GSObject GSo;
                GSo._type = type;
                GSo._unVal.pRefCounted = t;
                ret->Append(GSo);
            }
            t = t->_next;
            n++;
        }

        assert(rlast->_next == NULL);
        rlast->_next = _gc_chain;
        if(_gc_chain)
        {
            _gc_chain->_prev = rlast;
        }
        _gc_chain = resurrected;
    }

    t = _gc_chain;
    while(t) {
        t->UnMark();
        t = t->_next;
    }

    if(ret) {
        GSObjectPtr temp = ret;
        vm->Push(temp);
    }
    else {
        vm->PushNull();
    }
    return n;
}

GSInteger GSSharedState::CollectGarbage(GSVM *vm)
{
    GSInteger n = 0;
    GSCollectable *tchain = NULL;

    RunMark(vm,&tchain);

    GSCollectable *t = _gc_chain;
    GSCollectable *nx = NULL;
    if(t) {
        t->_uiRef++;
        while(t) {
            t->Finalize();
            nx = t->_next;
            if(nx) nx->_uiRef++;
            if(--t->_uiRef == 0)
                t->Release();
            t = nx;
            n++;
        }
    }

    t = tchain;
    while(t) {
        t->UnMark();
        t = t->_next;
    }
    _gc_chain = tchain;

    return n;
}
#endif

#ifndef NO_GARBAGE_COLLECTOR
void GSCollectable::AddToChain(GSCollectable **chain,GSCollectable *c)
{
    c->_prev = NULL;
    c->_next = *chain;
    if(*chain) (*chain)->_prev = c;
    *chain = c;
}

void GSCollectable::RemoveFromChain(GSCollectable **chain,GSCollectable *c)
{
    if(c->_prev) c->_prev->_next = c->_next;
    else *chain = c->_next;
    if(c->_next)
        c->_next->_prev = c->_prev;
    c->_next = NULL;
    c->_prev = NULL;
}
#endif

GSChar* GSSharedState::GetScratchPad(GSInteger size)
{
    GSInteger newsize;
    if (size > 0) {
        if (_scratchpadsize < size) {
            newsize = (GSInteger)((GSUnsignedInteger)size + (size >> 1));
            newsize = GS_max(newsize, size); //check for overflow
            _scratchpad = (GSChar*)GS_REALLOC(_scratchpad, _scratchpadsize, newsize);
            _scratchpadsize = newsize;

        }
        else if ((_scratchpadsize >> 5) >= size) {
            newsize = _scratchpadsize >> 1;
            newsize = GS_max(newsize, size); //check for overflow
            _scratchpad = (GSChar*)GS_REALLOC(_scratchpad, _scratchpadsize, newsize);
            _scratchpadsize = newsize;
        }
    }
    return _scratchpad;
}

RefTable::RefTable()
{
    AllocNodes(4);
}

void RefTable::Finalize()
{
    RefNode *nodes = _nodes;
    for(GSUnsignedInteger n = 0; n < _numofslots; n++) {
        nodes->obj.Null();
        nodes++;
    }
}

RefTable::~RefTable()
{
    GS_FREE(_buckets,(_numofslots * sizeof(RefNode *)) + (_numofslots * sizeof(RefNode)));
}

#ifndef NO_GARBAGE_COLLECTOR
void RefTable::Mark(GSCollectable **chain)
{
    RefNode *nodes = (RefNode *)_nodes;
    for(GSUnsignedInteger n = 0; n < _numofslots; n++) {
        if(GS_type(nodes->obj) != OT_NULL) {
            GSSharedState::MarkObject(nodes->obj,chain);
        }
        nodes++;
    }
}
#endif

void RefTable::AddRef(GSObject &obj)
{
    GSHash mainpos;
    RefNode *prev;
    RefNode *ref = Get(obj,mainpos,&prev,true);
    ref->refs++;
}

GSUnsignedInteger RefTable::GetRefCount(GSObject &obj)
{
     GSHash mainpos;
     RefNode *prev;
     RefNode *ref = Get(obj,mainpos,&prev,false);
     if (ref)
         return ref->refs;
     return 0;
}


GSBool RefTable::Release(GSObject &obj)
{
    GSHash mainpos;
    RefNode *prev;
    RefNode *ref = Get(obj,mainpos,&prev,false);
    if(ref) {
        if(--ref->refs == 0) {
            GSObjectPtr o = ref->obj;
            if(prev) {
                prev->next = ref->next;
            }
            else {
                _buckets[mainpos] = ref->next;
            }
            ref->next = _freelist;
            _freelist = ref;
            _slotused--;
            ref->obj.Null();
            //<<FIXME>>test for shrink?
            return GSTrue;
        }
    }
    else {
        assert(0);
    }
    return GSFalse;
}

void RefTable::Resize(GSUnsignedInteger size)
{
    RefNode **oldbucks = _buckets;
    RefNode *t = _nodes;
    GSUnsignedInteger oldnumofslots = _numofslots;
    AllocNodes(size);
    //rehash
    GSUnsignedInteger nfound = 0;
    for(GSUnsignedInteger n = 0; n < oldnumofslots; n++) {
        if(GS_type(t->obj) != OT_NULL) {
            //add back;
            assert(t->refs != 0);
            RefNode *nn = Add(::HashObj(t->obj)&(_numofslots-1),t->obj);
            nn->refs = t->refs;
            t->obj.Null();
            nfound++;
        }
        t++;
    }
    assert(nfound == oldnumofslots);
    GS_FREE(oldbucks,(oldnumofslots * sizeof(RefNode *)) + (oldnumofslots * sizeof(RefNode)));
}

RefTable::RefNode *RefTable::Add(GSHash mainpos,GSObject &obj)
{
    RefNode *t = _buckets[mainpos];
    RefNode *newnode = _freelist;
    newnode->obj = obj;
    _buckets[mainpos] = newnode;
    _freelist = _freelist->next;
    newnode->next = t;
    assert(newnode->refs == 0);
    _slotused++;
    return newnode;
}

RefTable::RefNode *RefTable::Get(GSObject &obj,GSHash &mainpos,RefNode **prev,bool add)
{
    RefNode *ref;
    mainpos = ::HashObj(obj)&(_numofslots-1);
    *prev = NULL;
    for (ref = _buckets[mainpos]; ref; ) {
        if(_rawval(ref->obj) == _rawval(obj) && GS_type(ref->obj) == GS_type(obj))
            break;
        *prev = ref;
        ref = ref->next;
    }
    if(ref == NULL && add) {
        if(_numofslots == _slotused) {
            assert(_freelist == 0);
            Resize(_numofslots*2);
            mainpos = ::HashObj(obj)&(_numofslots-1);
        }
        ref = Add(mainpos,obj);
    }
    return ref;
}

void RefTable::AllocNodes(GSUnsignedInteger size)
{
    RefNode **bucks;
    RefNode *nodes;
    bucks = (RefNode **)GS_MALLOC((size * sizeof(RefNode *)) + (size * sizeof(RefNode)));
    nodes = (RefNode *)&bucks[size];
    RefNode *temp = nodes;
    GSUnsignedInteger n;
    for(n = 0; n < size - 1; n++) {
        bucks[n] = NULL;
        temp->refs = 0;
        new (&temp->obj) GSObjectPtr;
        temp->next = temp+1;
        temp++;
    }
    bucks[n] = NULL;
    temp->refs = 0;
    new (&temp->obj) GSObjectPtr;
    temp->next = NULL;
    _freelist = nodes;
    _nodes = nodes;
    _buckets = bucks;
    _slotused = 0;
    _numofslots = size;
}
//////////////////////////////////////////////////////////////////////////
//GSStringTable
/*
* The following code is based on Lua 4.0 (Copyright 1994-2002 Tecgraf, PUC-Rio.)
* http://www.lua.org/copyright.html#4
* http://www.lua.org/source/4.0.1/src_lstring.c.html
*/

GSStringTable::GSStringTable(GSSharedState *ss)
{
    _sharedstate = ss;
    AllocNodes(4);
    _slotused = 0;
}

GSStringTable::~GSStringTable()
{
    GS_FREE(_strings,sizeof(GSString*)*_numofslots);
    _strings = NULL;
}

void GSStringTable::AllocNodes(GSInteger size)
{
    _numofslots = size;
    _strings = (GSString**)GS_MALLOC(sizeof(GSString*)*_numofslots);
    memset(_strings,0,sizeof(GSString*)*_numofslots);
}

GSString* GSStringTable::Concat(const GSChar* a, GSInteger alen, const GSChar* b, GSInteger blen)
{
    GSHash newhash = ::_hashstr2(a, alen, b, blen);
    GSHash h = newhash & (_numofslots - 1);
    GSString* s;
    GSInteger len = alen + blen;
    for (s = _strings[h]; s; s = s->_next) {
        if (s->_len == len) {
            if ((!memcmp(a, s->_val, GS_rsl(alen)))
                && (!memcmp(b, &s->_val[alen], GS_rsl(blen)))) {
                return s; //found
            }
        }
    }
    //
    GSString* t = (GSString*)GS_MALLOC(GS_rsl(len) + sizeof(GSString));
    new (t) GSString;
    t->_sharedstate = _sharedstate;
    memcpy(t->_val, a, GS_rsl(alen));
    memcpy(&t->_val[alen], b, GS_rsl(blen));
    t->_val[len] = _SC('\0');
    t->_len = len;
    t->_hash = newhash;
    t->_next = _strings[h];
    _strings[h] = t;
    _slotused++;
#ifdef _DEBUG
    GSHash old_newhash = ::_hashstr(t->_val, t->_len);
    assert(old_newhash == newhash);
#endif
    if (_slotused > _numofslots)  /* too crowded? */
        Resize(_numofslots * 2);
    return t;
}

GSString *GSStringTable::Add(const GSChar *news,GSInteger len)
{
    if(len<0)
        len = (GSInteger)scstrlen(news);
    GSHash newhash = ::_hashstr(news,len);
    GSHash h = newhash&(_numofslots-1);
    GSString *s;
    for (s = _strings[h]; s; s = s->_next){
        if(s->_len == len && (!memcmp(news,s->_val,GS_rsl(len))))
            return s; //found
    }

    GSString *t = (GSString *)GS_MALLOC(GS_rsl(len)+sizeof(GSString));
    new (t) GSString;
    t->_sharedstate = _sharedstate;
    memcpy(t->_val,news,GS_rsl(len));
    t->_val[len] = _SC('\0');
    t->_len = len;
    t->_hash = newhash;
    t->_next = _strings[h];
    _strings[h] = t;
    _slotused++;
    if (_slotused > _numofslots)  /* too crowded? */
        Resize(_numofslots*2);
    return t;
}

void GSStringTable::Resize(GSInteger size)
{
    GSInteger oldsize=_numofslots;
    GSString **oldtable=_strings;
    AllocNodes(size);
    for (GSInteger i=0; i<oldsize; i++){
        GSString *p = oldtable[i];
        while(p){
            GSString *next = p->_next;
            GSHash h = p->_hash&(_numofslots-1);
            p->_next = _strings[h];
            _strings[h] = p;
            p = next;
        }
    }
    GS_FREE(oldtable,oldsize*sizeof(GSString*));
}

void GSStringTable::Remove(GSString *bs)
{
    GSString *s;
    GSString *prev=NULL;
    GSHash h = bs->_hash&(_numofslots - 1);

    for (s = _strings[h]; s; ){
        if(s == bs){
            if(prev)
                prev->_next = s->_next;
            else
                _strings[h] = s->_next;
            _slotused--;
            GSInteger slen = s->_len;
            s->~GSString();
            GS_FREE(s,sizeof(GSString) + GS_rsl(slen));
            return;
        }
        prev = s;
        s = s->_next;
    }
    assert(0);//if this fail something is wrong
}

