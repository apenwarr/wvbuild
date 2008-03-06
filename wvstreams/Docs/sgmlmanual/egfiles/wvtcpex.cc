/*
 * A WvTCP example.
 *
 * Telnets to your local SMTP port, or any other port given
 * on the command line.
 */

#include "wvtcp.h"
#include "wvistreamlist.h"
#include "wvlog.h"

int main(int argc, char **argv)
{
    WvLog err("tcp", WvLog::Error);
    WvTCPConn sock(WvString(argc==2 ? argv[1] : "0.0.0.0:25"));

    wvcon->autoforward(sock);
    sock.autoforward(*wvcon);

    WvIStreamList l;
    l.add_after(l.tail, wvcon, false);
    l.add_after(l.tail, &sock, false);

    while (wvcon->isok() && sock.isok())
    {
	if (l.select(-1))
	    l.callback();
    }

    if (!wvcon->isok() && wvcon->geterr())
	err("stdin: %s\n", wvcon->errstr());
    else if (!sock.isok() && sock.geterr())
	err("socket: %s\n", sock.errstr());

    return 0;
}
