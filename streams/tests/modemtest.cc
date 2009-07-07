/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * WvModem test program.  Acts almost like a comm program, using WvLog to
 * display everything received from the modem.  It should respond to AT
 * commands.
 */
#include "wvmodem.h"
#include "wvistreamlist.h"
#include "wvlog.h"
#include "strutils.h"

int main(int argc, char **argv)
{
    WvLog log("modemtest"), modemlog("Rx");

    if (argc < 2)
    {
	log("usage: modemtest <devname>\n");
	return 1;
    }
    
    WvModem modem(argv[1], 19200);
    unsigned char buf[1024];
    size_t len;
    bool last_carrier = false, carrier;
    
    WvIStreamList l;
    l.append(wvcon, false, "wvcon");
    l.append(&modem, false, "modem");
    
    while (modem.isok() && wvcon->isok())
    {
	carrier = modem.carrier();
	if (last_carrier != carrier)
	{
	    log("Modem %s\n", carrier ? "CONNECTED" : "DISCONNECTED" );
	    last_carrier = carrier;
	}
	
	if (!l.select(100))
	    continue;
	
	if (wvcon->select(0))
	{
	    len = wvcon->read(buf, sizeof(buf));
	    replace_char(buf, '\n', '\r', len);
	    modem.write(buf, len);
	}
	else if (modem.select(0))
	{
	    len = modem.read(buf, sizeof(buf));
	    modemlog.write(buf, len);
	}
    }
    
    if (!modem.isok() && modem.geterr())
	log(WvLog::Error, "Modem device: %s\n", modem.errstr());
    
    return 0;
}
