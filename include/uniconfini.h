/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * A generator for .ini files.
 */
#ifndef __UNICONFINI_H
#define __UNICONFINI_H

#include "uniconfgen.h"
#include "uniconftemp.h"
#include "wvlog.h"

/**
 * Loads and saves ".ini"-style files similar to those used by
 * Windows, but adapted to represent keys and values using Tcl
 * style lists.
 * 
 * To mount, use the moniker prefix "ini:" followed by the
 * path of the .ini file.
 * 
 */
class UniConfIniFileGen : public UniConfTempGen
{
    WvString filename;
    WvLog log;
    
public:
    /**
     * Creates a generator which can load/modify/save a .ini file.
     * @param filename the local path of the .ini file
     */
    UniConfIniFileGen(WvStringParm filename);

    virtual ~UniConfIniFileGen();
    
    /***** Overridden members *****/

    virtual bool commit(const UniConfKey &key, UniConfDepth::Type depth);
    virtual bool refresh(const UniConfKey &key, UniConfDepth::Type depth);

private:
    void save(WvStream &file, UniConfValueTree &parent);
    bool refreshcomparator(const UniConfValueTree *a,
        const UniConfValueTree *b, void *userdata);
};


#endif // __UNICONFINI_H
