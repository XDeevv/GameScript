/*  see copyright notice in GameScript.h */
#ifndef _GSSTRING_H_
#define _GSSTRING_H_

inline GSHash _hashstr (const GSChar *s, size_t l)
{
	GSHash h = (GSHash)l;  /* seed */
	size_t step = (l >> 5) + 1;  /* if string is too long, don't hash all its chars */
	size_t l1;
	for (l1 = l; l1 >= step; l1 -= step)
		h = h ^ ((h << 5) + (h >> 2) + ((unsigned short)s[l1 - 1]));
	return h;
}

inline GSHash _hashstr2(const GSChar* as, size_t al, const GSChar* bs, size_t bl)
{
    size_t l = al + bl;
    GSHash h = (GSHash)l;  /* seed */
    GSInteger step = (GSInteger)((l >> 5) + 1);  /* if string is too long, don't hash all its chars */
    GSInteger l1 = (GSInteger)l;
    for (; l1 >= step; l1 -= step) {
        GSInteger idx = l1 - 1 - al;
        if (idx < 0) {
            break;
        }
        h = h ^ ((h << 5) + (h >> 2) + ((unsigned short)bs[idx]));
    }
    for (; l1 >= step; l1 -= step) {
        GSInteger idx = l1 - 1;
        h = h ^ ((h << 5) + (h >> 2) + ((unsigned short)as[idx]));
    }
    return h;
}

struct GSString : public GSRefCounted
{
    GSString(){}
    ~GSString(){}
public:
    static GSString *Create(GSSharedState *ss, const GSChar *, GSInteger len = -1 );
    static GSString* Concat(GSSharedState* ss, const GSChar* a, GSInteger alen, const GSChar* b, GSInteger blen);
    GSInteger Next(const GSObjectPtr &refpos, GSObjectPtr &outkey, GSObjectPtr &outval);
    void Release();
    GSSharedState *_sharedstate;
    GSString *_next; //chain for the string table
    GSInteger _len;
    GSHash _hash;
    GSChar _val[1];
};



#endif //_GSSTRING_H_

