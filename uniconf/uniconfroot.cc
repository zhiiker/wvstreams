/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * Defines the root management class for UniConf.  To create any kind of
 * UniConf tree, you'll need one of these.
 */
#include "uniconfroot.h"

/***** UniConfRootImpl *****/

UniConfRootImpl::UniConfRootImpl()
{
}


UniConfRootImpl::~UniConfRootImpl()
{
}


void UniConfRootImpl::addwatch(const UniConfKey &key,
    UniConfDepth::Type depth, UniConfWatch *watch)
{
}


void UniConfRootImpl::delwatch(const UniConfKey &key,
    UniConfDepth::Type depth, UniConfWatch *watch)
{
}


void UniConfRootImpl::deltacallback(const UniConfGen &gen,
    const UniConfKey &key, UniConfDepth::Type depth, void *userdata)
{
}
