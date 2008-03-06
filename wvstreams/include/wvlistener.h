/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * A base implementation for "listeners", streams that spawn other streams
 * from (presumably) incoming connections.
 */ 
#ifndef __WVLISTENER_H
#define __WVLISTENER_H

#include "iwvlistener.h"
#include "wvstreamclone.h"  // FIXME needed *only* for CompatCallback

class WvListener : public IWvListener
{
    IMPLEMENT_IOBJECT(WvListener);
public:
    IWvStream *cloned;
    IWvListenerCallback acceptor;
    
    WvListener(IWvStream *_cloned);
    virtual ~WvListener();
    
    virtual IWvListenerCallback onaccept(IWvListenerCallback _cb);
    void runonce(time_t msec_delay);
    
    //
    // IWvStream default implementation.
    //
    virtual void close()
        { if (cloned) cloned->close(); }
    virtual bool isok() const
        { return cloned && cloned->isok(); }
    
    virtual void callback()
    {  
	if (acceptor)
	{
	    IWvStream *s = accept();
	    if (s) acceptor(s);
	}
    }
    
    int getfd() const
        { return getrfd(); }
    virtual int getrfd() const
        { return cloned ? cloned->getrfd() : -1; }
    virtual int getwfd() const
        { return cloned ? cloned->getwfd() : -1; }

    virtual const WvAddr *src() const
        { return cloned ? cloned->src() : NULL; }
    
    virtual void pre_select(SelectInfo &si)
        { if (cloned) cloned->pre_select(si); }
    virtual bool post_select(SelectInfo &si)
        { return cloned ? cloned->post_select(si) : false; }
    
    virtual size_t read(void *buf, size_t count)
        { return 0; }
    virtual size_t write(const void *buf, size_t count)
        { return 0; }
    virtual size_t read(WvBuf &outbuf, size_t count)
        { return 0; }
    virtual size_t write(WvBuf &inbuf, size_t count = INT_MAX)
        { return 0; }
    virtual void noread()
        { }
    virtual void nowrite()
        { }
    virtual void maybe_autoclose()
        { }
    virtual bool isreadable()
        { return false; }
    virtual bool iswritable()
        { return false; }
    virtual bool flush(time_t msec_timeout)
        { return false; }
    virtual bool should_flush()
        { return false; }
    virtual IWvStreamCallback setreadcallback(IWvStreamCallback _cb)
        { return 0; }
    virtual IWvStreamCallback setwritecallback(IWvStreamCallback _cb)
        { return 0; }
    virtual IWvStreamCallback setexceptcallback(IWvStreamCallback _cb)
        { return 0; }
    virtual IWvStreamCallback setclosecallback(IWvStreamCallback _cb)
        { return 0; }
    virtual const char *wsname() const
        { return "Listener"; }
    virtual void set_wsname(WvStringParm name)
        { }
    void set_wsname(WVSTRING_FORMAT_DECL)
        { set_wsname(WvString(WVSTRING_FORMAT_CALL)); }
    virtual const char *wstype() const
        { return "Listener"; }
    virtual WSID wsid() const
        { return 0; }
    virtual void outbuf_limit(size_t size)
        { }
};

#endif // __WVLISTENER_H
