/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * Manages a UniConf daemon session.
 */
#include "uniconfdaemonconn.h"
#include "uniconfdaemon.h"
#include "wvtclstring.h"

UniConfDaemonConn::UniConfDaemonConn(WvStream *_s, const UniConf &_root) :
    UniConfConn(_s),
    root(_root)
{
    writecmd(EVENT_HELLO, "{UniConf Server ready}");
}


UniConfDaemonConn::~UniConfDaemonConn()
{
}


void UniConfDaemonConn::execute()
{
    UniConfConn::execute();
    for (;;)
    {
        UniConfConn::Command command = readcmd();
        if (command == UniConfConn::NONE)
            break;

        // parse and execute command
        WvString arg1(wvtcl_getword(payloadbuf, " "));
        WvString arg2(wvtcl_getword(payloadbuf, " "));
        switch (command)
        {
            case UniConfConn::INVALID:
                do_malformed();
                break;
            
            case UniConfConn::REQ_NOOP:
                do_noop();
                break;

            case UniConfConn::REQ_GET:
            {
                if (arg1.isnull())
                    do_malformed();
                else
                    do_get(arg1);
                break;
            }
            
            case UniConfConn::REQ_SET:
                if (arg1.isnull() || arg2.isnull())
                    do_malformed();
                else
                    do_set(arg1, arg2);
                break;

            case UniConfConn::REQ_REMOVE:
                if (arg1.isnull())
                    do_malformed();
                else
                    do_remove(arg1);
                break;

            case UniConfConn::REQ_ZAP:
                if (arg1.isnull())
                    do_malformed();
                else
                    do_zap(arg1);
                break;

            case UniConfConn::REQ_SUBTREE:
                if (arg1.isnull())
                    do_malformed();
                else
                    do_subtree(arg1);
                break;

            case UniConfConn::REQ_ADDWATCH:
                if (arg1.isnull() || arg2.isnull())
                    do_malformed();
                else
                {
                    UniConfDepth::Type depth = UniConfDepth::fromname(arg2);
                    if (depth == -1)
                        do_malformed();
                    else
                        do_addwatch(arg1, depth);
                }
                break;

            case UniConfConn::REQ_DELWATCH:
                if (arg1.isnull() || arg2.isnull())
                    do_malformed();
                else
                {
                    UniConfDepth::Type depth = UniConfDepth::fromname(arg2);
                    if (depth == -1)
                        do_malformed();
                    else
                        do_delwatch(arg1, depth);
                }
                break;

            case UniConfConn::REQ_QUIT:
                do_quit();
                break;

            case UniConfConn::REQ_HELP:
                do_help();
                break;

            default:
                do_malformed();
                break;
        }
    }
}


void UniConfDaemonConn::do_malformed()
{
    writefail("malformed");
}


void UniConfDaemonConn::do_noop()
{
    writeok();
}


void UniConfDaemonConn::do_get(const UniConfKey &key)
{
    WvString value(root[key].get());
    if (value.isnull())
        writefail();
    else
    {
        writevalue(key, value);
        writeok();
    }
}


void UniConfDaemonConn::do_set(const UniConfKey &key, WvStringParm value)
{
    if (root[key].set(value))
        writeok();
    else
        writefail();
}


void UniConfDaemonConn::do_remove(const UniConfKey &key)
{
    if (root[key].remove())
        writeok();
    else
        writefail();
}


void UniConfDaemonConn::do_zap(const UniConfKey &key)
{
    if (root[key].zap())
        writeok();
    else
        writefail();
}


void UniConfDaemonConn::do_subtree(const UniConfKey &key)
{
    UniConf cfg(root[key]);
    if (cfg.exists())
    {
        UniConf::Iter it(cfg);
        for (it.rewind(); it.next(); )
            writevalue(it->fullkey(), it->get());
        writeok();
    }
    else
        writefail();
}


void UniConfDaemonConn::do_addwatch(const UniConfKey &key,
    UniConfDepth::Type depth)
{
    // FIXME: not implemented
}


void UniConfDaemonConn::do_delwatch(const UniConfKey &key,
    UniConfDepth::Type depth)
{
    // FIXME: not implemented
}


void UniConfDaemonConn::do_quit()
{
    writeok();
    close();
}


void UniConfDaemonConn::do_help()
{
    for (int i = 0; i < UniConfConn::NUM_COMMANDS; ++i)
        writetext(UniConfConn::cmdinfos[i].description);
    writeok();
}
