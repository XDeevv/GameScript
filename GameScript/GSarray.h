/*  see copyright notice in GameScript.h */
#ifndef _GSARRAY_H_
#define _GSARRAY_H_

struct GSArray : public CHAINABLE_OBJ
{
private:
    GSArray(GSSharedState *ss,GSInteger nsize){_values.resize(nsize); INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);}
    ~GSArray()
    {
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
    }
public:
    static GSArray* Create(GSSharedState *ss, GSInteger nInitialSize){
        GSArray *newarray=(GSArray*)GS_MALLOC(sizeof(GSArray));
        new (newarray) GSArray(ss, nInitialSize);
        
        newarray->_max_capacity = -1; 
        newarray->_element_type = -1; // ---> Initialize to -1 (dynamic)
        
        return newarray;
    }
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(GSCollectable **chain);
    GSObjectType GetType() {return OT_ARRAY;}
#endif
    void Finalize(){
        _values.resize(0);
    }
    bool Get(const GSInteger nidx,GSObjectPtr &val)
    {
        if(nidx>=0 && nidx<(GSInteger)_values.size()){
            GSObjectPtr &o = _values[nidx];
            val = _realval(o);
            return true;
        }
        else return false;
    }
    bool Set(const GSInteger nidx,const GSObjectPtr &val)
    {
        if(nidx>=0 && nidx<(GSInteger)_values.size()){
            _values[nidx]=val;
            return true;
        }
        else return false;
    }
    GSInteger Next(const GSObjectPtr &refpos,GSObjectPtr &outkey,GSObjectPtr &outval)
    {
        GSUnsignedInteger idx=TranslateIndex(refpos);
        while(idx<_values.size()){
            //first found
            outkey=(GSInteger)idx;
            GSObjectPtr &o = _values[idx];
            outval = _realval(o);
            //return idx for the next iteration
            return ++idx;
        }
        //nothing to iterate anymore
        return -1;
    }
    GSArray *Clone(){GSArray *anew=Create(_opt_ss(this),0); anew->_values.copy(_values); return anew; }
    GSInteger Size() const {return _values.size();}
    GSInteger _max_capacity;
    GSInteger _element_type;
    void Resize(GSInteger size)
    {
        GSObjectPtr _null;
        Resize(size,_null);
    }
    void Resize(GSInteger size,GSObjectPtr &fill) { _values.resize(size,fill); ShrinkIfNeeded(); }
    void Reserve(GSInteger size) { _values.reserve(size); }
    bool Append(const GSObject &o, GSVM *vm = NULL);
    void Extend(const GSArray *a);
    GSObjectPtr &Top(){return _values.top();}
    void Pop(){_values.pop_back(); ShrinkIfNeeded(); }
    bool Insert(GSInteger idx,const GSObject &val){
        if(idx < 0 || idx > (GSInteger)_values.size())
            return false;
        _values.insert(idx,val);
        return true;
    }
    void ShrinkIfNeeded() {
        if(_values.size() <= _values.capacity()>>2) //shrink the array
            _values.shrinktofit();
    }
    bool Remove(GSInteger idx){
        if(idx < 0 || idx >= (GSInteger)_values.size())
            return false;
        _values.remove(idx);
        ShrinkIfNeeded();
        return true;
    }
    void Release()
    {
        GS_delete(this,GSArray);
    }

    GSObjectPtrVec _values;
};
#endif //_GSARRAY_H_

