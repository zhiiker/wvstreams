/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * Manages a UniConf daemon session.
 */
#include "uniconfdaemonconn.h"
#include "uniconfdaemon.h"
#include "wvtclstring.h"


/***** UniConfDaemonConn *****/

UniConfDaemonConn::UniConfDaemonConn(WvStream *_s, const UniConf &_root) :
    UniClientConn(_s),
    root(_root), watches(NUM_WATCHES)
{
    writecmd(EVENT_HELLO, "{UniConf Server ready}");

    root.add_callback(wvcallback(UniConfCallback, *this,
            UniConfDaemonConn::deltacallback), NULL, true);
}


UniConfDaemonConn::~UniConfDaemonConn()
{
    root.del_callback(wvcallback(UniConfCallback, *this,
            UniConfDaemonConn::deltacallback), NULL, true);
}


void UniConfDaemonConn::execute()
{
    UniClientConn::execute();
    for (;;)
    {
        UniClientConn::Command command = readcmd();
        if (command == UniClientConn::NONE)
            break;

        // parse and execute command
        WvString arg1(readarg());
        WvString arg2(readarg());
        switch (command)
        {
            case UniClientConn::INVALID:
                do_malformed();
                break;
            
            case UniClientConn::REQ_NOOP:
                do_noop();
                break;

            case UniClientConn::REQ_GET:
            {
                if (arg1.isnull())
                    do_malformed();
                else
                    do_get(arg1);
                break;
            }
            
            case UniClientConn::REQ_SET:
                if (arg1.isnull() || arg2.isnull())
                    do_malformed();
                else
                    do_set(arg1, arg2);
                break;

            case UniClientConn::REQ_REMOVE:
                if (arg1.isnull())
                    do_malformed();
                else
                    do_remove(arg1);
                break;

            case UniClientConn::REQ_SUBTREE:
                if (arg1.isnull())
                    do_malformed();
                else
                    do_subtree(arg1);
                break;

            case UniClientConn::REQ_HASCHILDREN:
                if (arg1.isnull())
                    do_malformed();
                else
                    do_haschildren(arg1);
                break;

            case UniClientConn::REQ_QUIT:
                do_quit();
                break;

            case UniClientConn::REQ_HELP:
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
    writefail("malformed request");
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
        writeonevalue(key, value);
}


void UniConfDaemonConn::do_set(const UniConfKey &key, WvStringParm value)
{
    root[key].set(value);
}


void UniConfDaemonConn::do_remove(const UniConfKey &key)
{
    root[key].remove();
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


void UniConfDaemonConn::do_haschildren(const UniConfKey &key)
{
    bool haschild = root[key].haschildren();
    WvString msg("%s %s", wvtcl_escape(key), haschild ? "TRUE" : "FALSE");
    writecmd(REPLY_CHILD, msg);
}


void UniConfDaemonConn::do_quit()
{
    writeok();
    close();
}


void UniConfDaemonConn::do_help()
{
    for (int i = 0; i < UniClientConn::NUM_COMMANDS; ++i)
        writetext(UniClientConn::cmdinfos[i].description);
    writeok();
}


void UniConfDaemonConn::deltacallback(const UniConf &key, void *userdata)
{
    WvString value(key.get());
    WvString msg;

    if (value.isnull())
        msg = WvString("%s", wvtcl_escape(key.fullkey()));
    else
        msg = WvString("%s %s", wvtcl_escape(key.fullkey()),
                                wvtcl_escape(key.get()));

    writecmd(UniClientConn::EVENT_NOTICE, msg);
}
