/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * A hash table container.
 */
#ifndef __WVHASHTABLE_H
#define __WVHASHTABLE_H

#include "wvlinklist.h"
#include "wvstring.h"

// predefined hashing functions (note: string hashes are case-insensitive)
unsigned WvHash(WvStringParm s);
unsigned WvHash(const char *s);
unsigned WvHash(const int &i);

/**
 * A small, efficient, type-safe hash table (also known as dictionary)
 * container class.
 * 
 * These are typically used to store a reasonably large number
 * of objects in no particular order and find them quickly when needed.
 * 
 * Two semi-distinct types of hash tables are supported:  tables and
 * dictionaries.
 * 
 * TABLE EXAMPLE:
 * 
 *   DeclareWvTable(WvString);
 *   int main()
 *   {
 *       WvString s("foo"), s2("blue"), s3("foo");
 *       WvStringTable t(10);  // suggested size: 10 elements
 *       t.add(&s); t.add(&s2);
 *       printf("%s %s\n", t[s]->str, t[s3]->str); // prints "foo" "foo"
 *   }
 * 
 * 
 * Here, the WvStrings "foo" and "blue" are stored in the table, and then
 * "foo" is looked up twice.  Both table searches return &amp;s.
 * The suggested table size of 10 elements places no upper bound on
 * the maximum number of elements, but optimizes the hash table for
 * holding roughly 10 elements.
 * 
 * To match an element, the WvString operator== function is used.  That
 * means this particular example is rather contrived since if you already
 * have the search string, you do not need to find it in the table.
 * Objects with more specific operator== might have more luck.
 * 
 * DICTIONARY EXAMPLE:
 * 
 *   class IntStr
 *   {
 *       int *key;
 *       WvString data;
 *   }
 *   DeclareWvDict(IntStr, int, key[0]);
 *   IntStrDict d(100);
 * 
 * 
 * Here, we are creating a dictionary that stores strings indexed by
 * integers.  d[5] might return the address of IntStr number 5, which
 * in turn contains WvString number 5.  When matching elements in this case,
 * a comparison is only done on key[0] of the two elements; thus, it is
 * not the operator== of IntStr that is important, but rather the operator==
 * for int.  (In this case, the operator== of int is predefined.)
 * 
 * The only reason *key is declared as a pointer in this example is to
 * demonstrate how to use pointer-based keys with our syntax.  In this case
 * it would certainly make more sense to use int key; and
 * DeclareWvDict(IntStr, key).  Note though, that int *key; and
 * DeclareWvDict(IntStr, key) is almost certainly not what you want, since
 * it would compare the POINTERS, not their values.
 * 
 * The interface of this class resembles that of WvList by design.
 * In particular, we support iterators in a similar way.
 * 
 * @see WvList<T>
 */

/**
 * The untyped base class of WvHashTable<T>.
 * 
 * Putting common code in here allows us to prevent it from being
 * replicated by each template instantiation of WvHashTable<T>.
 * 
 */

class WvHashTableBase
{
protected:
    
    WvHashTableBase(unsigned _numslots);
    // Copy constructor - not defined anywhere!
    WvHashTableBase(const WvHashTableBase &t); 
    virtual ~WvHashTableBase() {}; 
    WvHashTableBase& operator= (const WvHashTableBase &t);
    void setup()
        { /* default: do nothing */ }
    void shutdown()
        { /* default: do nothing */ }
    WvLink *prevlink(WvListBase *slots, const void *data, unsigned hash);
    void *genfind(WvListBase *slots, const void *data, unsigned hash);

    virtual bool compare(const void *key, const void *elem) const = 0;
public:
    unsigned numslots;
    WvListBase *wvslots;

    /**
     * Returns the number of elements in the hash table.
     * @return the number of elements
     */
    size_t count() const;

    /**
     * Returns true if the hash table is empty.
     * @return true if empty
     */
    bool isempty() const;

    // base class for the auto-declared hash table iterators
    class IterBase
    {
    public:
	WvHashTableBase *tbl;
	unsigned tblindex;
	WvLink *link;
	
	IterBase(WvHashTableBase &_tbl) : tbl(& _tbl)
            { }
        IterBase(const IterBase &other) : tbl(other.tbl),
            tblindex(other.tblindex), link(other.link)
            { }
	void rewind()
            { tblindex = 0; link = &tbl->wvslots[0].head; }
	WvLink *next();
	WvLink *cur() const
            { return link; }
    };
};


// Default comparison function used by WvHashTable
template <class K>
struct OpEqComp
{
    static bool compare(const K *key1, const K *key2)
        { return *key1 == *key2; }
};


template <
    class T,                                            // element type
    class K,                                            // key type
    class Accessor,                                     // element to key
    template <class> class Comparator = OpEqComp        // comparison func
>
class WvHashTable : public WvHashTableBase
{
protected:
    typedef Comparator<K> MyComparator; 

    unsigned hash(const T *data)
	{ return WvHash(*Accessor::get_key(data)); }

    virtual bool compare(const void *key, const void *elem) const
        { return MyComparator::compare((const K *)key,
                Accessor::get_key((const T *)elem)); }

public:
    /**
     * Creates a hash table.
     *
     * @param numslots the suggested number of slots
     */
    WvHashTable(unsigned _numslots) : WvHashTableBase(_numslots)
        { wvslots = new WvList<T>[numslots]; setup(); }

    WvList<T> *sl()
	{ return (WvList<T> *)wvslots; }

    virtual ~WvHashTable()
        { shutdown(); delete[] sl(); }

    void add(T *data, bool auto_free)
        { sl()[hash(data) % numslots].append(data, auto_free); }

    T *operator[] (const K &key)
        { return (T *)genfind(wvslots, &key, WvHash(key)); }

    void remove(const T *data)
    {
	unsigned h = hash(data);
        WvLink *l = prevlink(wvslots, Accessor::get_key(data), h);
	if (l && l->next) sl()[h % numslots].unlink_after(l);
    }

    void zap()
    {
	delete[] sl();
	wvslots = new WvList<T>[numslots];
    }

    class Iter : public WvHashTableBase::IterBase
    {
    public:
	Iter(WvHashTable &_tbl) : IterBase(_tbl)
	    { }
        Iter(const Iter &other) : IterBase(other)
            { }
	T *ptr() const
	    { return (T *)link->data; }
	WvIterStuff(T);
    };

    typedef class WvSorter<T, WvHashTableBase, WvHashTableBase::IterBase>
	Sorter;
};


// ******************************************
// WvDict and WvTable


#define DeclareWvDict2(_classname_,  _type_, _ftype_, _field_) 	        \
	__WvDict_base(_classname_, _type_, _ftype_, &obj->_field_)

#define DeclareWvDict(_type_, _ftype_, _field_) 			\
        DeclareWvDict2(_type_##Dict, _type_, _ftype_, _field_)

#define DeclareWvTable2(_classname_, _type_)                            \
        __WvDict_base(_classname_, _type_, _type_, obj)

#define DeclareWvTable(_type_)                                          \
        DeclareWvTable2(_type_##Table, _type_)


#define __WvDict_base(_classname_, _type_, _ftype_, _field_)	        \
    template <class T, class K>                                         \
    struct _classname_##Accessor 					\
    { 									\
        static const K *get_key(const T *obj)                    \
	    { return _field_; } 		                        \
    }; 									\
									\
    typedef WvHashTable<_type_, _ftype_,                                \
             _classname_##Accessor<_type_, _ftype_> > _classname_



// ******************************************
// WvMap

// *****************************
// WvPair

// Type specification to facilitate auto_free
// Object type - ignores auto_free
template<typename TKey, typename _TData>
class WvPair
{
    typedef _TData TData;
public:
    TKey key;
    TData data;
    WvPair(const TKey &_key, const TData &_data, bool _auto_free)
        : key(_key), data(_data) { };
};


// Pointer type
template<typename TKey, typename _TData>
class WvPair<TKey, _TData*>
{
    typedef _TData* TData;
public:
    TKey key;
    TData data;
    WvPair(const TKey &_key, const TData &_data, bool _auto_free)
        : key(_key), data(_data), auto_free(_auto_free) { };
    virtual ~WvPair()
        { if (auto_free) delete data; };
protected:
    bool auto_free;
};


// *****************************
// Main map template
template
<
    typename TKey,
    typename TData,
    template <class> class Comparator = OpEqComp
>
class WvMap : public WvHashTable<WvPair<TKey, TData>, TKey,
        WvMap<TKey, TData, Comparator>, Comparator>
{
protected: 
    typedef WvPair<TKey, TData> MyPair;
    typedef WvMap<TKey, TData, Comparator> MyMap;
    typedef WvHashTable<MyPair, TKey, MyMap, Comparator> MyHashTable;

    // accessor mothod for WvHashTable to use
    friend class MyHashTable;
    static const TKey *get_key(const MyPair *obj)
        { return &obj->key; }

public:
    WvMap(int s) : MyHashTable(s)    { };
    /* May return NULL!! */ 
    TData *find(const TKey &key)
    {
        MyPair* p = MyHashTable::operator[](key);
        return p ? &p->data : (TData*)NULL;
    }
    TData *operator[](const TKey &key)
        { return find(key); }
    void add(const TKey &key, const TData &data, bool auto_free = false)
        { MyHashTable::add(new MyPair(key, data, auto_free), true); }
    void remove(const TKey &key)
        { MyHashTable::remove(MyHashTable::operator[](key)); } 
    typedef typename MyHashTable::Iter Iter;
}; 



#endif // __WVHASHTABLE_H
