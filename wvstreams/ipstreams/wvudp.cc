/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * WvUDPStream can send and receive packets on a connectionless UDP socket.
 * See wvudp.h for details.
 */
#include "wvudp.h"

#ifndef WIN32
#include <sys/socket.h>
#else
#define setsockopt(a,b,c,d,e) setsockopt(a,b,c, (const char*) d,e)
#define recvfrom(a,b,c,d,e,f) recvfrom(a, (char *) b, c, d, e, f)
#define sendto(a,b,c,d,e,f) sendto(a,(const char*) b,c,d,e,f)
#undef errno
#define errno GetLastError()
#endif

#include <fcntl.h>

#ifdef ISDARWIN
# define socklen_t int
#endif

WvUDPStream::WvUDPStream(const WvIPPortAddr &_local,
    const WvIPPortAddr &_rem) :
    localaddr(), remaddr(_rem)
{
    int x = 1;
    setfd(socket(PF_INET, SOCK_DGRAM, 0));
    if (getfd() < 0 
	|| setsockopt(getfd(), SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x)) < 0)
    {
	seterr(errno);
	return;
    }
    
    set_close_on_exec(true);
    set_nonblock(true);

    struct sockaddr *sa = _local.sockaddr();
    if (bind(getfd(), sa, _local.sockaddr_len()))
    {
	delete sa;
	seterr(errno);
	return;
    }
    delete sa;
    
    struct sockaddr_in nsa;
    socklen_t nsalen = sizeof(nsa);
    if (getsockname(getfd(), (sockaddr *)&nsa, &nsalen) < 0)
    {
	seterr(errno);
	return;
    }
    localaddr = WvIPPortAddr(&nsa);
    
    if (WvIPAddr(_rem) != WvIPAddr())
    {
	struct sockaddr *sa = _rem.sockaddr();
	if (connect(getfd(), sa, _rem.sockaddr_len()))
	{
	    delete sa;
	    seterr(errno);
	    return;
	}
	delete sa;
    }
}


WvUDPStream::~WvUDPStream()
{
}


const WvAddr *WvUDPStream::src() const
{
    return &remaddr;
}


const WvAddr *WvUDPStream::local() const
{
    return &localaddr;
}


size_t WvUDPStream::uread(void *buf, size_t count)
{
    if (!isok() || !buf || !count) return 0;
    
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int in = recvfrom(getfd(), buf, count, 0, (sockaddr *)&from, &fromlen);
    
    if (in >= 0)
	remaddr = WvIPPortAddr(&from);

    // errors in UDP are ignored
    return in < 0 ? 0 : in;
}


size_t WvUDPStream::uwrite(const void *buf, size_t count)
{
    if (!isok() || !buf || !count) return 0;
     
    // pretend everything worked if there is nowhere to send data
    if (remaddr.is_zero()) return count;
    
    struct sockaddr *to = remaddr.sockaddr();
    size_t tolen = remaddr.sockaddr_len();
    int out;
    
    out = sendto(getfd(), buf, count, 0, to, tolen);
    
    if (out < 0 && errno == EACCES) // permission denied
	seterr(EACCES);
    
    delete to;
    
    // errors in UDP are ignored
    // pretend that the write always succeeds even if the kernel
    // complains since we don't want datagrams backing up in the
    // buffer and forming merged datagrams as a result
    return out < 0 ? 0 : out;
}


void WvUDPStream::enable_broadcasts()
{
    int value = 1;
    
    if (!isok()) return;
    
    setsockopt(getfd(), SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));
}
