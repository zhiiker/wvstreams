/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 2002 Net Integration Technologies, Inc.
 *
 * A UniConf generator that stores keys in memory.
 */
#ifndef __UNITEMPGEN_H
#define __UNITEMPGEN_H

#include "uniconfgen.h"
#include "uniconftree.h"

/**
 * A UniConf generator that stores keys in memory.
 * 
 * Maintains a dirtyness indicator that is set whenever the contents
 * are changed.  Also dispatches notifications on such changes.
 */
class UniTempGen : public UniConfGen
{
protected:
    class NodeIter;
    friend class NodeIter;

public:
    UniConfValueTree *root; /*!< the root of the tree */
    bool dirty; /*!< set whenever the tree actually changes */

    UniTempGen();
    virtual ~UniTempGen();

    /***** Overridden members *****/

    virtual WvString get(const UniConfKey &key);
    virtual bool set(const UniConfKey &key, WvStringParm value);
    virtual bool zap(const UniConfKey &key);
    virtual bool exists(const UniConfKey &key);
    virtual bool haschildren(const UniConfKey &key);
    virtual Iter *iterator(const UniConfKey &key);
};


/** An iterator over keys stored in a UniTempGen. */
class UniTempGen::NodeIter : public UniTempGen::Iter
{
protected:
    UniTempGen *xgen;
    UniConfValueTree::Iter xit;

public:
    NodeIter(UniTempGen *gen, const UniConfValueTree::Iter &it);
    virtual ~NodeIter();

    /***** Overridden methods *****/

    virtual void rewind();
    virtual bool next();
    virtual UniConfKey key() const;
};


#endif // __UNITEMPGEN_H