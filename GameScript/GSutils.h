/*  see copyright notice in GameScript.h */
#ifndef _GSUTILS_H_
#define _GSUTILS_H_

void *GS_vm_malloc(GSUnsignedInteger size);
void *GS_vm_realloc(void *p,GSUnsignedInteger oldsize,GSUnsignedInteger size);
void GS_vm_free(void *p,GSUnsignedInteger size);

#define GS_new(__ptr,__type) {__ptr=(__type *)GS_vm_malloc(sizeof(__type));new (__ptr) __type;}
#define GS_delete(__ptr,__type) {__ptr->~__type();GS_vm_free(__ptr,sizeof(__type));}
#define GS_MALLOC(__size) GS_vm_malloc((__size));
#define GS_FREE(__ptr,__size) GS_vm_free((__ptr),(__size));
#define GS_REALLOC(__ptr,__oldsize,__size) GS_vm_realloc((__ptr),(__oldsize),(__size));

#define GS_aligning(v) (((size_t)(v) + (GS_ALIGNMENT-1)) & (~(GS_ALIGNMENT-1)))
#define GS_max(a, b) ((a) > (b) ? (a) : (b))

//GSvector mini vector class, supports objects by value
template<typename T> class GSvector
{
public:
    GSvector()
    {
        _vals = NULL;
        _size = 0;
        _allocated = 0;
    }
    GSvector(const GSvector<T>& v)
    {
        copy(v);
    }
    void copy(const GSvector<T>& v)
    {
        if(_size) {
            resize(0); //destroys all previous stuff
        }
        //resize(v._size);
        if(v._size > _allocated) {
            _realloc(v._size);
        }
        for(GSUnsignedInteger i = 0; i < v._size; i++) {
            new ((void *)&_vals[i]) T(v._vals[i]);
        }
        _size = v._size;
    }
    ~GSvector()
    {
        if(_allocated) {
            for(GSUnsignedInteger i = 0; i < _size; i++)
                _vals[i].~T();
            GS_FREE(_vals, (_allocated * sizeof(T)));
        }
    }
    void reserve(GSUnsignedInteger newsize) { _realloc(newsize); }
    void resize(GSUnsignedInteger newsize, const T& fill = T())
    {
        if(newsize > _allocated)
            _realloc(newsize);
        if(newsize > _size) {
            while(_size < newsize) {
                new ((void *)&_vals[_size]) T(fill);
                _size++;
            }
        }
        else{
            for(GSUnsignedInteger i = newsize; i < _size; i++) {
                _vals[i].~T();
            }
            _size = newsize;
        }
    }
    void shrinktofit() { if(_size > 4) { _realloc(_size); } }
    T& top() const { return _vals[_size - 1]; }
    inline GSUnsignedInteger size() const { return _size; }
    bool empty() const { return (_size <= 0); }
    inline T &push_back(const T& val = T())
    {
        if(_allocated <= _size)
            _realloc(_size * 2);
        return *(new ((void *)&_vals[_size++]) T(val));
    }
    inline void pop_back()
    {
        _size--; _vals[_size].~T();
    }
    void insert(GSUnsignedInteger idx, const T& val)
    {
        resize(_size + 1);
        for(GSUnsignedInteger i = _size - 1; i > idx; i--) {
            _vals[i] = _vals[i - 1];
        }
        _vals[idx] = val;
    }
    void remove(GSUnsignedInteger idx)
    {
        _vals[idx].~T();
        if(idx < (_size - 1)) {
            memmove((void*)&_vals[idx], &_vals[idx+1], sizeof(T) * (_size - idx - 1));
        }
        _size--;
    }
    GSUnsignedInteger capacity() { return _allocated; }
    inline T &back() const { return _vals[_size - 1]; }
    inline T& operator[](GSUnsignedInteger pos) const{ return _vals[pos]; }
    T* _vals;
private:
    void _realloc(GSUnsignedInteger newsize)
    {
        newsize = (newsize > 0)?newsize:4;
        _vals = (T*)GS_REALLOC(_vals, _allocated * sizeof(T), newsize * sizeof(T));
        _allocated = newsize;
    }
    GSUnsignedInteger _size;
    GSUnsignedInteger _allocated;
};

#endif //_GSUTILS_H_

