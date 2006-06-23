/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 2004-2006 Net Integration Technologies, Inc.
 * 
 */ 
#ifndef __WVDBUSSERVCONN_H
#define __WVDBUSSERVCONN_H
#include "wvdbusconn.h"
#include "wvdbusserver.h"
#include <stdint.h>


class WvDBusServConn : public WvDBusConn
{
public:
    WvDBusServConn(DBusConnection *c, WvDBusServer *s);
    virtual ~WvDBusServConn()
    {}

    virtual bool isok() const
    {
        return true; //conn;
    }

    virtual void add_listener(WvStringParm interface, WvStringParm path,
                                IWvDBusListener *listener);
//     virtual void add_method(WvStringParm interface, WvStringParm path,
//                     IWvDBusListener *listener);

private:
    void proxy_msg(WvDBusMsg &msg);
    void hello_cb(WvDBusReplyMsg &reply);
    void request_name_cb(WvDBusReplyMsg &reply, WvString name,
                         uint32_t flags);
    void release_name_cb(WvDBusReplyMsg &reply, WvString _name);
    WvDBusServer *server;
    WvLog log;
};

#endif // __WVDBUSSERVCONN_H
