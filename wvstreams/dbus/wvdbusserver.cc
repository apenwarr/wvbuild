/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 2005-2006 Net Integration Technologies, Inc.
 * 
 * Pathfinder Software:
 *   Copyright (C) 2007, Carillon Information Security Inc.
 *
 * This library is licensed under the LGPL, please read LICENSE for details.
 *
 */ 
#include "wvdbusserver.h"
#include "wvdbusconn.h"
#include "wvstrutils.h"
#include "wvuid.h"
#include "wvtcplistener.h"
#include "wvdelayedcallback.h"
#undef interface // windows
#include <dbus/dbus.h>
#include "wvx509.h"


class WvDBusServerAuth : public IWvDBusAuth
{
    enum State { NullWait, AuthWait, BeginWait };
    State state;
    wvuid_t client_uid;
public:
    WvDBusServerAuth();
    virtual bool authorize(WvDBusConn &c);

    virtual wvuid_t get_uid() { return client_uid; }
};


WvDBusServerAuth::WvDBusServerAuth()
{
    state = NullWait;
    client_uid = WVUID_INVALID;
}


bool WvDBusServerAuth::authorize(WvDBusConn &c)
{
    c.log("State=%s\n", state);
    if (state == NullWait)
    {
	char buf[1];
	size_t len = c.read(buf, 1);
	if (len == 1 && buf[0] == '\0')
	{
	    state = AuthWait;
	    // fall through
	}
	else if (len > 0)
	    c.seterr("Client didn't start with NUL byte");
	else
	    return false; // no data yet, come back later
    }
    
    const char *line = c.in();
    if (!line)
	return false; // not done yet
    
    WvStringList words;
    words.split(line);
    WvString cmd(words.popstr());
    
    if (state == AuthWait)
    {
	if (!strcasecmp(cmd, "AUTH"))
	{
	    // FIXME actually check authentication information!
	    WvString typ(words.popstr());
            if (!strcasecmp(typ, "EXTERNAL"))
            {
                WvString uid = 
                    WvHexDecoder().strflushstr(words.popstr());
                if (!!uid)
                {
                    // FIXME: Check that client is on the same machine!
                    client_uid = uid.num();
                }
		
		state = BeginWait;
		c.out("OK f00f\r\n");
            }
	    else
	    {
		// Some clients insist that we reject something because
		// their state machine can't handle us accepting just the
		// "AUTH " command.
		c.out("REJECTED EXTERNAL\r\n");
		// no change in state
	    }
	}
	else
	    c.seterr("AUTH command expected: '%s'", line);
    }
    else if (state == BeginWait)
    {
	if (!strcasecmp(cmd, "BEGIN"))
	    return true; // done
	else
	    c.seterr("BEGIN command expected: '%s'", line);
    }

    return false;
}


WvDBusServer::WvDBusServer()
    : log("DBus Server", WvLog::Debug)
{
    // user must now call listen() at least once.
    add(&listeners, false, "listeners");
}


WvDBusServer::~WvDBusServer()
{
    close();
    zap();
}


void WvDBusServer::listen(WvStringParm moniker)
{
    IWvListener *listener = IWvListener::create(moniker);
    log(WvLog::Info, "Listening on '%s'\n", *listener->src());
    if (!listener->isok())
	log(WvLog::Info, "Can't listen: %s\n",
	    listener->errstr());
    listener->onaccept(wv::bind(&WvDBusServer::new_connection_cb,
				this, _1));
    listeners.add(listener, true, "listener");
}


bool WvDBusServer::isok() const
{
    if (geterr())
	return false;
    
    WvIStreamList::Iter i(listeners);
    for (i.rewind(); i.next(); )
	if (!i->isok())
	    return false;
    return WvIStreamList::isok();
}


int WvDBusServer::geterr() const
{
    return WvIStreamList::geterr();
}


WvString WvDBusServer::get_addr()
{
    // FIXME assumes tcp
    WvIStreamList::Iter i(listeners);
    for (i.rewind(); i.next(); )
	if (i->isok())
	    return WvString("tcp:%s", *i->src());
    return WvString();
}


void WvDBusServer::register_name(WvStringParm name, WvDBusConn *conn)
{
    name_to_conn[name] = conn;
}


void WvDBusServer::unregister_name(WvStringParm name, WvDBusConn *conn)
{
    assert(name_to_conn[name] == conn);
    name_to_conn.erase(name);
}


void WvDBusServer::unregister_conn(WvDBusConn *conn)
{
    {
	std::map<WvString,WvDBusConn*>::iterator i;
	for (i = name_to_conn.begin(); i != name_to_conn.end(); )
	{
	    if (i->second == conn)
	    {
		name_to_conn.erase(i->first);
		i = name_to_conn.begin();
	    }
	    else
		++i;
	}
    }
    
    all_conns.unlink(conn);
}


bool WvDBusServer::do_server_msg(WvDBusConn &conn, WvDBusMsg &msg)
{
    WvString method(msg.get_member());
    
    if (msg.get_path() == "/org/freedesktop/DBus/Local")
    {
	if (method == "Disconnected")
	    return true; // nothing to do until their *stream* disconnects
    }
    
    if (msg.get_dest() != "org.freedesktop.DBus") return false;

    // dbus-daemon seems to ignore the path as long as the service is right
    //if (msg.get_path() != "/org/freedesktop/DBus") return false;
    
    // I guess it's for us!
    
    if (method == "Hello")
    {
	log("hello_cb\n");
	msg.reply().append(conn.uniquename()).send(conn);
	return true;
    }
    else if (method == "RequestName")
    {
	WvDBusMsg::Iter args(msg);
	WvString _name = args.getnext();
	// uint32_t flags = args.getnext(); // supplied, but ignored
	
	log("request_name_cb(%s)\n", _name);
	register_name(_name, &conn);
	
	msg.reply().append((uint32_t)DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	    .send(conn);
	return true;
    }
    else if (method == "ReleaseName")
    {
	WvDBusMsg::Iter args(msg);
	WvString _name = args.getnext();
	
	log("release_name_cb(%s)\n", _name);
	unregister_name(_name, &conn);
	
	msg.reply().append((uint32_t)DBUS_RELEASE_NAME_REPLY_RELEASED)
	    .send(conn);
	return true;
    }
    else if (method == "NameHasOwner")
    {
	WvDBusMsg::Iter args(msg);
	WvString known_name = args.getnext();
	WvDBusConn *serv = name_to_conn[known_name];
	msg.reply().append(!!serv).send(conn);
	return true;
    }
    else if (method == "GetNameOwner")
    {
	WvDBusMsg::Iter args(msg);
	WvString known_name = args.getnext();
	WvDBusConn *serv = name_to_conn[known_name];
	if (serv)
	    msg.reply().append(serv->uniquename()).send(conn);
	else
	    WvDBusError(msg, "org.freedesktop.DBus.Error.NameHasNoOwner", 
			"No match for name '%s'", known_name).send(conn);
	return true;
    }
    else if (method == "AddMatch")
    {
	// we just proxy every signal to everyone for now
	msg.reply().send(conn);
	return true;
    }
    else if (method == "StartServiceByName")
    {
	// we don't actually support this, but returning an error message
	// confuses perl's Net::DBus library, at least.
	msg.reply().send(conn);
	return true;
    }
    else if (method == "GetConnectionUnixUser" || 
	     method == "GetConnectionUnixUserName")
    {
	WvDBusMsg::Iter args(msg);
	WvString _name = args.getnext();
        WvDBusConn *target = name_to_conn[_name];
	
        if (!target)
	{
            WvDBusError(msg, "org.freedesktop.DBus.Error.Failed", 
                "No connection found for name '%s'.", _name).send(conn);
	    return true;
	}
	
	wvuid_t client_uid = target->get_uid();
	
        if (client_uid == WVUID_INVALID)
	{
            WvDBusError(msg, "org.freedesktop.DBus.Error.Failed", 
			"No user associated with connection '%s'.",
			target->uniquename()).send(conn);
	    return true;
	}
	
	log("Found unix user for '%s', uid is %s.\n", _name, client_uid);
	
	if (method == "GetConnectionUnixUser")
	{
	    WvString s(client_uid);
	    msg.reply().append((uint32_t)atoll(s)).send(conn);
	    return true;
	}
	else if (method == "GetConnectionUnixUserName")
	{
	    WvString username = wv_username_from_uid(client_uid);
	    if (!username)
	    {
		WvDBusError(msg, "org.freedesktop.DBus.Error.Failed",
			    "No username for uid='%s'", client_uid)
		    .send(conn);
		return true;
	    }
	    
	    msg.reply().append(username).send(conn);
	    return true;
	}
	else
	    assert(false); // should never happen
            
        assert(false);
        return false;
    }
    else if (method == "GetConnectionCert" ||
	    method == "GetConnectionCertFingerprint")
    {
	WvDBusMsg::Iter args(msg);
	WvString connid = args.getnext();

	WvDBusConn *c = name_to_conn[connid];

	WvString ret = c ? c->getattr("peercert") : WvString::null;
	if (ret.isnull())
	    WvDBusError(msg, "org.freedesktop.DBus.Error.Failed",
			    "Connection %s did not present a certificate",
			    connid).send(conn);
	else
	{
	    if (method == "GetConnectionCertFingerprint") 
	    {
		WvX509 tempcert;
		// We can assume it's valid because our SSL conn authenticated
		tempcert.decode(WvX509::CertPEM, ret);
		ret = tempcert.get_fingerprint();
	    }
	    msg.reply().append(ret).send(conn);
	}

	return true;
    }
    else
    {
	WvDBusError(msg, "org.freedesktop.DBus.Error.UnknownMethod", 
		    "Unknown dbus method '%s'", method).send(conn);
	return true; // but we've handled it, since it belongs to us
    }
}


bool WvDBusServer::do_bridge_msg(WvDBusConn &conn, WvDBusMsg &msg)
{
    // if we get here, nobody handled the message internally, so we can try
    // to proxy it.
    if (!!msg.get_dest()) // don't handle blank (broadcast) paths here
    {
	std::map<WvString,WvDBusConn*>::iterator i 
	    = name_to_conn.find(msg.get_dest());
	WvDBusConn *dconn = (i == name_to_conn.end()) ? NULL : i->second;
	log("Proxying #%s -> %s\n",
	    msg.get_serial(),
	    dconn ? dconn->uniquename() : WvString("(UNKNOWN)"));
	dbus_message_set_sender(msg, conn.uniquename().cstr());
	if (dconn)
	    dconn->send(msg);
	else
	{
	    log(WvLog::Warning,
		"Proxy: no connection for '%s'\n", msg.get_dest());
	    return false;
	}
        return true;
    }
    
    return false;
}


bool WvDBusServer::do_broadcast_msg(WvDBusConn &conn, WvDBusMsg &msg)
{
    if (!msg.get_dest())
    {
	log("Broadcasting #%s\n", msg.get_serial());
	
	// note: we broadcast messages even back to the connection where
	// they originated.  I'm not sure this is necessarily ideal, but if
	// you don't do that then an app can't signal objects that might be
	// inside itself.
	WvDBusConnList::Iter i(all_conns);
	for (i.rewind(); i.next(); )
	    i->send(msg);
        return true;
    }
    return false;
}


bool WvDBusServer::do_gaveup_msg(WvDBusConn &conn, WvDBusMsg &msg)
{
    WvDBusError(msg, "org.freedesktop.DBus.Error.NameHasNoOwner", 
		"No running service named '%s'", msg.get_dest()).send(conn);
    return true;
}


void WvDBusServer::conn_closed(WvStream &s)
{
    WvDBusConn *c = (WvDBusConn *)&s;
    unregister_conn(c);
    this->release();
}


void WvDBusServer::new_connection_cb(IWvStream *s)
{
    WvDBusConn *c = new WvDBusConn(s, new WvDBusServerAuth, false);
    c->addRef();
    this->addRef();
    all_conns.append(c, true);
    register_name(c->uniquename(), c);

    /* The delayed callback here should be explained.  The
     * 'do_broadcast_msg' function sends out data along all connections.
     * Unfortunately, this is a prime time to figure out a connection died.
     * A dying connection is removed from the all_conns list... but we are
     * still in do_broadcast_msg, and using an iterator to go over this list.
     * The consequences of this were not pleasant, at best.  Wrapping cb in a
     * delayedcallback will always safely remove a connection.
     */
    IWvStreamCallback mycb = wv::bind(&WvDBusServer::conn_closed, this,
				 wv::ref(*c));
    c->setclosecallback(wv::delayed(mycb));

    c->add_callback(WvDBusConn::PriSystem,
		    wv::bind(&WvDBusServer::do_server_msg, this,
			     wv::ref(*c), _1));
    c->add_callback(WvDBusConn::PriBridge, 
		    wv::bind(&WvDBusServer::do_bridge_msg, this,
			     wv::ref(*c), _1));
    c->add_callback(WvDBusConn::PriBroadcast,
		    wv::bind(&WvDBusServer::do_broadcast_msg, this,
			     wv::ref(*c), _1));
    c->add_callback(WvDBusConn::PriGaveUp,
		    wv::bind(&WvDBusServer::do_gaveup_msg, this,
			     wv::ref(*c), _1));
    
    append(c, true, "wvdbus servconn");
}
