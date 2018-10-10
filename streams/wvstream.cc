/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * Unified support for streams, that is, sequences of bytes that may or
 * may not be ready for read/write at any given time.
 * 
 * We provide typical read and write routines, as well as a select() function
 * for each stream.
 */
#include <time.h>
#include <sys/types.h>
#include <assert.h>
#define __WVSTREAM_UNIT_TEST 1
#include "wvstream.h"
#include "wvtimeutils.h"
#include "wvcont.h"
#include "wvstreamsdebugger.h"
#include "wvstrutils.h"
#include "wvistreamlist.h"
#include "wvlinkerhack.h"
#include "wvmoniker.h"
#include "wvneeds-sockets.h"

#ifdef _WIN32
#undef ENOBUFS
#define ENOBUFS WSAENOBUFS
#undef errno
#define errno GetLastError()
#ifdef __GNUC__
#include <sys/socket.h>
#endif
#include "streams.h"
#else
#include <errno.h>
#endif

#include <map>

using std::make_pair;
using std::map;


// enable this to add some read/write trace messages (this can be VERY
// verbose)
#if 0
# ifndef _MSC_VER
#  define TRACE(x, y...) fprintf(stderr, x, ## y); fflush(stderr);
# else
#  define TRACE printf
# endif
#else
# ifndef _MSC_VER
#  define TRACE(x, y...)
# else
#  define TRACE
# endif
#endif

WvStream *WvStream::globalstream = NULL;

UUID_MAP_BEGIN(WvStream)
  UUID_MAP_ENTRY(IObject)
  UUID_MAP_ENTRY(IWvStream)
  UUID_MAP_END


static map<WSID, WvStream*> *wsid_map;
static WSID next_wsid_to_try;


WV_LINK(WvStream);

static IWvStream *create_null(WvStringParm, IObject *)
{
    return new WvStream();
}

static WvMoniker<IWvStream> reg("null",  create_null);


IWvStream *IWvStream::create(WvStringParm moniker, IObject *obj)
{
    IWvStream *s = wvcreate<IWvStream>(moniker, obj);
    if (!s)
    {
	s = new WvStream();
	s->seterr_both(EINVAL, "Unknown moniker '%s'", moniker);
	WVRELEASE(obj); // we're not going to use it after all
    }
    return s;
}


static bool is_prefix_insensitive(const char *str, const char *prefix)
{
    size_t len = strlen(prefix);
    return strlen(str) >= len && strncasecmp(str, prefix, len) == 0;
}


static const char *strstr_insensitive(const char *haystack, const char *needle)
{
    while (*haystack != '\0')
    {
        if (is_prefix_insensitive(haystack, needle))
            return haystack;
        ++haystack;
    }
    return NULL;
}


static bool contains_insensitive(const char *haystack, const char *needle)
{
    return strstr_insensitive(haystack, needle) != NULL;
}


static const char *list_format = "%6s%s%2s%s%3s%s%3s%s%6s%s%20s%s%s";
static inline const char *Yes_No(bool val)
{
    return val? "Yes": "No";
}


void WvStream::debugger_streams_display_header(WvStringParm cmd,
        WvStreamsDebugger::ResultCallback result_cb)
{
    WvStringList result;
    result.append(list_format, "--WSID", "-", "RC", "-", "-Ok", "-", "-Cs", "-", "-AlRem", "-",
            "----------------Type", "-", "Name--------------------");
    result_cb(cmd, result);
}


// Set to fit in 6 chars
static WvString friendly_ms(time_t ms)
{
    if (ms <= 0)
        return WvString("(%s)", ms);
    else if (ms < 1000)
        return WvString("%sms", ms);
    else if (ms < 60*1000)
        return WvString("%ss", ms/1000);
    else if (ms < 60*60*1000)
        return WvString("%sm", ms/(60*1000));
    else if (ms <= 24*60*60*1000)
        return WvString("%sh", ms/(60*60*1000));
    else
        return WvString("%sd", ms/(24*60*60*1000));
}

void WvStream::debugger_streams_display_one_stream(WvStream *s,
        WvStringParm cmd,
        WvStreamsDebugger::ResultCallback result_cb)
{
    WvStringList result;
    s->addRef();
    unsigned refcount = s->release();
    result.append(list_format,
            s->wsid(), " ",
            refcount, " ",
            Yes_No(s->isok()), " ",
            Yes_No(s->uses_continue_select), " ",
            friendly_ms(s->alarm_remaining()), " ",
            s->wstype(), " ",
            s->wsname());
    result_cb(cmd, result);
}


void WvStream::debugger_streams_maybe_display_one_stream(WvStream *s,
        WvStringParm cmd,
        const WvStringList &args,
        WvStreamsDebugger::ResultCallback result_cb)
{
    bool show = args.isempty();
    WvStringList::Iter arg(args);
    for (arg.rewind(); arg.next(); )
    {
        WSID wsid;
        bool is_num = wvstring_to_num(*arg, wsid);
        
        if (is_num)
        {
            if (s->wsid() == wsid)
            {
                show = true;
                break;
            }
        }
        else
        {
            if ((s->wsname() && contains_insensitive(s->wsname(), *arg))
	     || (s->wstype() && contains_insensitive(s->wstype(), *arg)))
            {
                show = true;
                break;
            }
        }
    }
    if (show)
        debugger_streams_display_one_stream(s, cmd, result_cb);
}


WvString WvStream::debugger_streams_run_cb(WvStringParm cmd,
        WvStringList &args,
        WvStreamsDebugger::ResultCallback result_cb, void *)
{
    debugger_streams_display_header(cmd, result_cb);
    if (wsid_map)
    {
	map<WSID, WvStream*>::iterator it;

	for (it = wsid_map->begin(); it != wsid_map->end(); ++it)
            debugger_streams_maybe_display_one_stream(it->second, cmd, args,
						      result_cb);
    }
    
    return WvString::null;
}


WvString WvStream::debugger_close_run_cb(WvStringParm cmd,
        WvStringList &args,
        WvStreamsDebugger::ResultCallback result_cb, void *)
{
    if (args.isempty())
        return WvString("Usage: %s <WSID>", cmd);
    WSID wsid;
    WvString wsid_str = args.popstr();
    if (!wvstring_to_num(wsid_str, wsid))
        return WvString("Invalid WSID '%s'", wsid_str);
    IWvStream *s = WvStream::find_by_wsid(wsid);
    if (!s)
        return WvString("No such stream");
    s->close();
    return WvString::null;
}


void WvStream::add_debugger_commands()
{
    WvStreamsDebugger::add_command("streams", 0, debugger_streams_run_cb, 0);
    WvStreamsDebugger::add_command("close", 0, debugger_close_run_cb, 0);
}


WvStream::WvStream():
    read_requires_writable(NULL),
    write_requires_readable(NULL),
    uses_continue_select(false),
    personal_stack_size(131072),
    alarm_was_ticking(false),
    stop_read(false),
    stop_write(false),
    closed(false),
    readcb(wv::bind(&WvStream::legacy_callback, this)),
    max_outbuf_size(0),
    outbuf_delayed_flush(false),
    is_auto_flush(true),
    want_to_flush(true),
    is_flushing(false),
    queue_min(0),
    autoclose_time(0),
    alarm_time(wvtime_zero),
    last_alarm_check(wvtime_zero)
{
    TRACE("Creating wvstream %p\n", this);
    
    static bool first = true;
    if (first)
    {
        first = false;
        WvStream::add_debugger_commands();
    }
    
    // Choose a wsid;
    if (!wsid_map)
        wsid_map = new map<WSID, WvStream*>;
    WSID first_wsid_tried = next_wsid_to_try;
    do
    {
        if (wsid_map->find(next_wsid_to_try) == wsid_map->end())
            break;
        ++next_wsid_to_try;
    } while (next_wsid_to_try != first_wsid_tried);
    my_wsid = next_wsid_to_try++;
    bool inserted = wsid_map->insert(make_pair(my_wsid, this)).second;
    assert(inserted);
    
#ifdef _WIN32
    WSAData wsaData;
    int result = WSAStartup(MAKEWORD(2,0), &wsaData); 
    assert(result == 0);
#endif
}


// FIXME: interfaces (IWvStream) shouldn't have implementations!
IWvStream::IWvStream()
{
}


IWvStream::~IWvStream()
{
}


WvStream::~WvStream()
{
    TRACE("destroying %p\n", this);
    close();
    
    // if this assertion fails, then uses_continue_select is true, but you
    // didn't call terminate_continue_select() or close() before destroying
    // your object.  Shame on you!
    assert(!uses_continue_select || !call_ctx);
    
    call_ctx = 0; // finish running the suspended callback, if any

    assert(wsid_map);
    wsid_map->erase(my_wsid);
    if (wsid_map->empty())
    {
        delete wsid_map;
        wsid_map = NULL;
    }
    
    // eventually, streams will auto-add themselves to the globallist.  But
    // even before then, it'll never be useful for them to be on the
    // globallist *after* they get destroyed, so we might as well auto-remove
    // them already.  It's harmless for people to try to remove them twice.
    WvIStreamList::globallist.unlink(this);
    
    TRACE("done destroying %p\n", this);
}


void WvStream::close()
{
    TRACE("flushing in wvstream...\n");
    flush(2000); // fixme: should not hardcode this stuff
    TRACE("(flushed)\n");

    closed = true;
    
    if (!!closecb)
    {
        IWvStreamCallback cb = closecb;
        closecb = 0; // ensure callback is only called once
        cb();
    }
    
    // I would like to delete call_ctx here, but then if someone calls
    // close() from *inside* a continuable callback, we explode.  Oops!
    //call_ctx = 0; // destroy the context, if necessary
}


void WvStream::autoforward(WvStream &s)
{
    setcallback(wv::bind(autoforward_callback, wv::ref(*this), wv::ref(s)));
    read_requires_writable = &s;
}


void WvStream::noautoforward()
{
    setcallback(0);
    read_requires_writable = NULL;
}


void WvStream::autoforward_callback(WvStream &input, WvStream &output)
{
    char buf[1024];
    size_t len;
    
    len = input.read(buf, sizeof(buf));
    output.write(buf, len);
}


void WvStream::_callback()
{
    execute();
    if (!!callfunc)
	callfunc();
}


void *WvStream::_callwrap(void *)
{
    _callback();
    return NULL;
}


void WvStream::callback()
{
    TRACE("(?)");
    
    // if the alarm has gone off and we're calling callback... good!
    if (alarm_remaining() == 0)
    {
	alarm_time = wvtime_zero;
	alarm_was_ticking = true;
    }
    else
	alarm_was_ticking = false;
    
    assert(!uses_continue_select || personal_stack_size >= 1024);

#define TEST_CONTINUES_HARSHLY 0
#if TEST_CONTINUES_HARSHLY
#ifndef _WIN32
# warning "Using WvCont for *all* streams for testing!"
#endif
    if (1)
#else
    if (uses_continue_select && personal_stack_size >= 1024)
#endif
    {
	if (!call_ctx) // no context exists yet!
	{
	    call_ctx = WvCont(wv::bind(&WvStream::_callwrap, this, _1),
			      personal_stack_size);
	}
	
	call_ctx(NULL);
    }
    else
	_callback();

    // if this assertion fails, a derived class's virtual execute() function
    // didn't call its parent's execute() function, and we didn't make it
    // all the way back up to WvStream::execute().  This doesn't always
    // matter right now, but it could lead to obscure bugs later, so we'll
    // enforce it.
}


bool WvStream::isok() const
{
    return !closed && WvErrorBase::isok();
}


void WvStream::seterr(int _errnum)
{
    if (!geterr()) // no pre-existing error
    {
        WvErrorBase::seterr(_errnum);
        close();
    }
}


size_t WvStream::read(WvBuf &outbuf, size_t count)
{
    // for now, just wrap the older read function
    size_t free = outbuf.free();
    if (count > free)
        count = free;

    WvDynBuf tmp;
    unsigned char *buf = tmp.alloc(count);
    size_t len = read(buf, count);
    tmp.unalloc(count - len);
    outbuf.merge(tmp);
    return len;
}


size_t WvStream::write(WvBuf &inbuf, size_t count)
{
    // for now, just wrap the older write function
    size_t avail = inbuf.used();
    if (count > avail)
        count = avail;
    const unsigned char *buf = inbuf.get(count);
    size_t len = write(buf, count);
    inbuf.unget(count - len);
    return len;
}


size_t WvStream::read(void *buf, size_t count)
{
    assert(!count || buf);
    
    size_t bufu, i;
    unsigned char *newbuf;

    bufu = inbuf.used();
    if (bufu < queue_min)
    {
	newbuf = inbuf.alloc(queue_min - bufu);
	assert(newbuf);
	i = uread(newbuf, queue_min - bufu);
	inbuf.unalloc(queue_min - bufu - i);
	
	bufu = inbuf.used();
    }
    
    if (bufu < queue_min)
    {
	maybe_autoclose();
	return 0;
    }
        
    // if buffer is empty, do a hard read
    if (!bufu)
	bufu = uread(buf, count);
    else
    {
	// otherwise just read from the buffer
	if (bufu > count)
	    bufu = count;
    
	memcpy(buf, inbuf.get(bufu), bufu);
    }
    
    TRACE("read  obj 0x%08x, bytes %d/%d\n", (unsigned int)this, bufu, count);
    maybe_autoclose();
    return bufu;
}


size_t WvStream::write(const void *buf, size_t count)
{
    assert(!count || buf);
    if (!isok() || !buf || !count || stop_write) return 0;
    
    size_t wrote = 0;
    if (!outbuf_delayed_flush && !outbuf.used())
    {
	wrote = uwrite(buf, count);
        count -= wrote;
        buf = (const unsigned char *)buf + wrote;
	// if (!count) return wrote; // short circuit if no buffering needed
    }
    if (max_outbuf_size != 0)
    {
        size_t canbuffer = max_outbuf_size - outbuf.used();
        if (count > canbuffer)
            count = canbuffer; // can't write the whole amount
    }
    if (count != 0)
    {
        outbuf.put(buf, count);
        wrote += count;
    }

    if (should_flush())
    {
        if (is_auto_flush)
            flush(0);
        else 
            flush_outbuf(0);
    }

    return wrote;
}


void WvStream::noread()
{
    stop_read = true;
    maybe_autoclose();
}


void WvStream::nowrite()
{
    stop_write = true;
    maybe_autoclose();
}


void WvStream::maybe_autoclose()
{
    if (stop_read && stop_write && !outbuf.used() && !inbuf.used() && !closed)
	close();
}


bool WvStream::isreadable()
{
    return isok() && select(0, true, false, false);
}


bool WvStream::iswritable()
{
    return !stop_write && isok() && select(0, false, true, false);
}


char *WvStream::blocking_getline(time_t wait_msec, int separator,
				 int readahead)
{
    assert(separator >= 0);
    assert(separator <= 255);
    
    //assert(uses_continue_select || wait_msec == 0);

    WvTime timeout_time(0);
    if (wait_msec > 0)
        timeout_time = msecadd(wvtime(), wait_msec);
    
    maybe_autoclose();

    // if we get here, we either want to wait a bit or there is data
    // available.
    while (isok())
    {
	// fprintf(stderr, "(inbuf used = %d)\n", inbuf.used()); fflush(stderr);
        queuemin(0);
    
        // if there is a newline already, we have enough data.
        if (inbuf.strchr(separator) > 0)
	    break;
	else if (!isok() || stop_read)    // uh oh, stream is in trouble.
	    break;

        // make select not return true until more data is available
        queuemin(inbuf.used() + 1);

        // compute remaining timeout
        if (wait_msec > 0)
        {
            wait_msec = msecdiff(timeout_time, wvtime());
            if (wait_msec < 0)
                wait_msec = 0;
        }
	
	// FIXME: this is blocking_getline.  It shouldn't
	// call continue_select()!
        bool hasdata;
        if (wait_msec != 0 && uses_continue_select)
            hasdata = continue_select(wait_msec);
        else
            hasdata = select(wait_msec, true, false);
        
	if (!isok())
            break;

        if (hasdata)
        {
            // read a few bytes
	    WvDynBuf tmp;
            unsigned char *buf = tmp.alloc(readahead);
	    assert(buf);
            size_t len = uread(buf, readahead);
            tmp.unalloc(readahead - len);
	    inbuf.put(tmp.get(len), len);
            hasdata = len > 0; // enough?
        }

	if (!isok())
	    break;
	
        if (!hasdata && wait_msec == 0)
	    return NULL; // handle timeout
    }
    if (!inbuf.used())
	return NULL;

    // return the appropriate data
    size_t i = 0;
    i = inbuf.strchr(separator);
    if (i > 0) {
	char *eol = (char *)inbuf.mutablepeek(i - 1, 1);
	assert(eol && *eol == separator);
	*eol = 0;
	return const_cast<char*>((const char *)inbuf.get(i));
    } else {
	// handle "EOF without newline" condition
	// FIXME: it's very silly that buffers can't return editable
	// char* arrays.
	inbuf.alloc(1)[0] = 0; // null-terminate it
	return const_cast<char *>((const char *)inbuf.get(inbuf.used()));
    }
}


char *WvStream::continue_getline(time_t wait_msec, int separator,
				 int readahead)
{
    assert(false && "not implemented, come back later!");
    assert(uses_continue_select);
    return NULL;
}


void WvStream::drain()
{
    char buf[1024];
    while (isreadable())
	read(buf, sizeof(buf));
}


bool WvStream::flush(time_t msec_timeout)
{
    if (is_flushing) return false;
    
    TRACE("%p flush starts\n", this);

    is_flushing = true;
    want_to_flush = true;
    bool done = flush_internal(msec_timeout) // any other internal buffers
	&& flush_outbuf(msec_timeout);  // our own outbuf
    is_flushing = false;

    TRACE("flush stops (%d)\n", done);
    return done;
}


bool WvStream::should_flush()
{
    return want_to_flush;
}


bool WvStream::flush_outbuf(time_t msec_timeout)
{
    TRACE("%p flush_outbuf starts (isok=%d)\n", this, isok());
    bool outbuf_was_used = outbuf.used();
    
    // do-nothing shortcut for speed
    // FIXME: definitely makes a "measurable" difference...
    //   but is it worth the risk?
    if (!outbuf_was_used && !autoclose_time && !outbuf_delayed_flush)
    {
	maybe_autoclose();
	return true;
    }
    
    WvTime stoptime = msecadd(wvtime(), msec_timeout);
    
    // flush outbuf
    while (outbuf_was_used && isok())
    {
//	fprintf(stderr, "%p: fd:%d/%d, used:%d\n", 
//		this, getrfd(), getwfd(), outbuf.used());
	
	size_t attempt = outbuf.optgettable();
	size_t real = uwrite(outbuf.get(attempt), attempt);
	
	// WARNING: uwrite() may have messed up our outbuf!
	// This probably only happens if uwrite() closed the stream because
	// of an error, so we'll check isok().
	if (isok() && real < attempt)
	{
	    TRACE("flush_outbuf: unget %d-%d\n", attempt, real);
	    assert(outbuf.ungettable() >= attempt - real);
	    outbuf.unget(attempt - real);
	}
	
	// since post_select() can call us, and select() calls post_select(),
	// we need to be careful not to call select() if we don't need to!
	// post_select() will only call us with msec_timeout==0, and we don't
	// need to do select() in that case anyway.
	if (!msec_timeout)
	    break;
	if (msec_timeout >= 0 
	  && (stoptime < wvtime() || !select(msec_timeout, false, true)))
	    break;
	
	outbuf_was_used = outbuf.used();
    }

    // handle autoclose
    if (autoclose_time && isok())
    {
	time_t now = time(NULL);
	TRACE("Autoclose enabled for 0x%p - now-time=%ld, buf %d bytes\n", 
	      this, now - autoclose_time, outbuf.used());
	if ((flush_internal(0) && !outbuf.used()) || now > autoclose_time)
	{
	    autoclose_time = 0; // avoid infinite recursion!
	    close();
	}
    }

    TRACE("flush_outbuf: after autoclose chunk\n");
    if (outbuf_delayed_flush && !outbuf_was_used)
        want_to_flush = false;
    
    TRACE("flush_outbuf: now isok=%d\n", isok());

    // if we can't flush the outbuf, at least empty it!
    if (outbuf_was_used && !isok())
	outbuf.zap();

    maybe_autoclose();
    TRACE("flush_outbuf stops\n");
    
    return !outbuf_was_used;
}


bool WvStream::flush_internal(time_t msec_timeout)
{
    // once outbuf emptied, that's it for most streams
    return true;
}


int WvStream::getrfd() const
{
    return -1;
}


int WvStream::getwfd() const
{
    return -1;
}


void WvStream::flush_then_close(int msec_timeout)
{
    time_t now = time(NULL);
    autoclose_time = now + (msec_timeout + 999) / 1000;
    
    TRACE("Autoclose SETUP for 0x%p - buf %d bytes, timeout %ld sec\n", 
	    this, outbuf.used(), autoclose_time - now);

    // as a fast track, we _could_ close here: but that's not a good idea,
    // since flush_then_close() deals with obscure situations, and we don't
    // want the caller to use it incorrectly.  So we make things _always_
    // break when the caller forgets to call select() later.
    
    flush(0);
}


void WvStream::pre_select(SelectInfo &si)
{
    maybe_autoclose();
    
    time_t alarmleft = alarm_remaining();
    
    if (!isok() || (!si.inherit_request && alarmleft == 0))
    {
	si.msec_timeout = 0;
	return; // alarm has rung
    }

    if (!si.inherit_request)
    {
	si.wants.readable |= static_cast<bool>(readcb);
	si.wants.writable |= static_cast<bool>(writecb);
	si.wants.isexception |= static_cast<bool>(exceptcb);
    }
    
    // handle read-ahead buffering
    if (si.wants.readable && inbuf.used() && inbuf.used() >= queue_min)
    {
	si.msec_timeout = 0; // already ready
	return;
    }
    if (alarmleft >= 0
      && (alarmleft < si.msec_timeout || si.msec_timeout < 0))
	si.msec_timeout = alarmleft + 10;
}


bool WvStream::post_select(SelectInfo &si)
{
    if (!si.inherit_request)
    {
	si.wants.readable |= static_cast<bool>(readcb);
	si.wants.writable |= static_cast<bool>(writecb);
	si.wants.isexception |= static_cast<bool>(exceptcb);
    }
    
    // FIXME: need sane buffer flush support for non FD-based streams
    // FIXME: need read_requires_writable and write_requires_readable
    //        support for non FD-based streams
    
    // note: flush(nonzero) might call select(), but flush(0) never does,
    // so this is safe.
    if (should_flush())
	flush(0);
    if (!si.inherit_request && alarm_remaining() == 0)
	return true; // alarm ticked
    if ((si.wants.readable || (!si.inherit_request && readcb))
	&& inbuf.used() && inbuf.used() >= queue_min)
	return true; // already ready
    return false;
}


void WvStream::_build_selectinfo(SelectInfo &si, time_t msec_timeout,
    bool readable, bool writable, bool isexcept, bool forceable)
{
    FD_ZERO(&si.read);
    FD_ZERO(&si.write);
    FD_ZERO(&si.except);
    
    if (forceable)
    {
	si.wants.readable = static_cast<bool>(readcb);
	si.wants.writable = static_cast<bool>(writecb);
	si.wants.isexception = static_cast<bool>(exceptcb);
    }
    else
    {
	si.wants.readable = readable;
	si.wants.writable = writable;
	si.wants.isexception = isexcept;
    }
    
    si.max_fd = -1;
    si.msec_timeout = msec_timeout;
    si.inherit_request = ! forceable;
    si.global_sure = false;

    wvstime_sync();

    pre_select(si);
    if (globalstream && forceable && (globalstream != this))
    {
	WvStream *s = globalstream;
	globalstream = NULL; // prevent recursion
	s->xpre_select(si, SelectRequest(false, false, false));
	globalstream = s;
    }
}


int WvStream::_do_select(SelectInfo &si)
{
    // prepare timeout
    timeval tv;
    tv.tv_sec = si.msec_timeout / 1000;
    tv.tv_usec = (si.msec_timeout % 1000) * 1000;
    
    // block
    int sel = 0;
#ifdef _WIN32
    // selecting on an empty set of sockets doesn't cause a delay in win32.
    if (si.max_fd < 0)
	Sleep(si.msec_timeout >= 0 ? si.msec_timeout : 1000000000);
    else
#endif
    sel = ::select(si.max_fd+1, &si.read, &si.write, &si.except,
		   si.msec_timeout >= 0 ? &tv : (timeval*)NULL);
#ifdef _WIN32
    // On Windows, if all 3 fd_sets are empty, select returns SOCKET_ERROR.
    // WSAEINVAL tells us this was the case.
    // http://msdn.microsoft.com/en-us/library/ms740141(VS.85).aspx
    if (sel == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL)
	sel = 0;
#endif

    // handle errors.
    //   EAGAIN and EINTR don't matter because they're totally normal.
    //   ENOBUFS is hopefully transient.
    //   EBADF is kind of gross and might imply that something is wrong,
    //      but it happens sometimes...
    if (sel < 0 
      && errno != EAGAIN && errno != EINTR 
      && errno != EBADF
      && errno != ENOBUFS
      )
    {
        seterr(errno);
    }
    TRACE("select() returned %d\n", sel);
    return sel;
}


bool WvStream::_process_selectinfo(SelectInfo &si, bool forceable)
{
    // We cannot move the clock backward here, because timers that
    // were expired in pre_select could then not be expired anymore,
    // and while time going backward is rather unsettling in general,
    // for it to be happening between pre_select and post_select is
    // just outright insanity.
    wvstime_sync_forward();
        
    bool sure = post_select(si);
    if (globalstream && forceable && (globalstream != this))
    {
	WvStream *s = globalstream;
	globalstream = NULL; // prevent recursion
	si.global_sure = s->xpost_select(si, SelectRequest(false, false, false))
					 || si.global_sure;
	globalstream = s;
    }
    return sure;
}


bool WvStream::_select(time_t msec_timeout, bool readable, bool writable,
		       bool isexcept, bool forceable)
{
    // Detect use of deleted stream
    assert(wsid_map && (wsid_map->find(my_wsid) != wsid_map->end()));
        
    SelectInfo si;
    _build_selectinfo(si, msec_timeout, readable, writable, isexcept,
		      forceable);
    
    bool sure = false;
    int sel = _do_select(si);
    if (sel >= 0)
        sure = _process_selectinfo(si, forceable); 
    if (si.global_sure && globalstream && forceable && (globalstream != this))
	globalstream->callback();

    return sure;
}


IWvStream::SelectRequest WvStream::get_select_request()
{
    return IWvStream::SelectRequest(static_cast<bool>(readcb), static_cast<bool>(writecb),
				    static_cast<bool>(exceptcb));
}


void WvStream::force_select(bool readable, bool writable, bool isexception)
{
    if (readable)
	readcb = wv::bind(&WvStream::legacy_callback, this);
    if (writable)
	writecb = wv::bind(&WvStream::legacy_callback, this);
    if (isexception)
	exceptcb = wv::bind(&WvStream::legacy_callback, this);
}


void WvStream::undo_force_select(bool readable, bool writable, bool isexception)
{
    if (readable)
	readcb = 0;
    if (writable)
	writecb = 0;
    if (isexception)
	exceptcb = 0;
}


void WvStream::alarm(time_t msec_timeout)
{
    if (msec_timeout >= 0)
        alarm_time = msecadd(wvstime(), msec_timeout);
    else
	alarm_time = wvtime_zero;
}


time_t WvStream::alarm_remaining()
{
    if (alarm_time.tv_sec)
    {
	WvTime now = wvstime();

	// Time is going backward!
	if (now < last_alarm_check)
	{
#if 0 // okay, I give up.  Time just plain goes backwards on some systems.
	    // warn only if it's a "big" difference (sigh...)
	    if (msecdiff(last_alarm_check, now) > 200)
		fprintf(stderr, " ************* TIME WENT BACKWARDS! "
			"(%ld:%ld %ld:%ld)\n",
			last_alarm_check.tv_sec, last_alarm_check.tv_usec,
			now.tv_sec, now.tv_usec);
#endif
	    alarm_time = tvdiff(alarm_time, tvdiff(last_alarm_check, now));
	}

	last_alarm_check = now;

        time_t remaining = msecdiff(alarm_time, now);
        if (remaining < 0)
            remaining = 0;
        return remaining;
    }
    return -1;
}


bool WvStream::continue_select(time_t msec_timeout)
{
    assert(uses_continue_select);
    
    // if this assertion triggers, you probably tried to do continue_select()
    // while inside terminate_continue_select().
    assert(call_ctx);
    
    if (msec_timeout >= 0)
	alarm(msec_timeout);

    alarm(msec_timeout);
    WvCont::yield();
    alarm(-1); // cancel the still-pending alarm, or it might go off later!
    
    // when we get here, someone has jumped back into our task.
    // We have to select(0) here because it's possible that the alarm was 
    // ticking _and_ data was available.  This is aggravated especially if
    // msec_delay was zero.  Note that running select() here isn't
    // inefficient, because if the alarm was expired then pre_select()
    // returned true anyway and short-circuited the previous select().
    TRACE("hello-%p\n", this);
    return !alarm_was_ticking || select(0, static_cast<bool>(readcb),
					static_cast<bool>(writecb), static_cast<bool>(exceptcb));
}


void WvStream::terminate_continue_select()
{
    close();
    call_ctx = 0; // destroy the context, if necessary
}


const WvAddr *WvStream::src() const
{
    return NULL;
}


void WvStream::setcallback(IWvStreamCallback _callfunc)
{ 
    callfunc = _callfunc;
    call_ctx = 0; // delete any in-progress WvCont
}


void WvStream::legacy_callback()
{
    execute();
    if (!!callfunc)
	callfunc();
}


IWvStreamCallback WvStream::setreadcallback(IWvStreamCallback _callback)
{
    IWvStreamCallback tmp = readcb;

    readcb = _callback;

    return tmp;
}


IWvStreamCallback WvStream::setwritecallback(IWvStreamCallback _callback)
{
    IWvStreamCallback tmp = writecb;

    writecb = _callback;

    return tmp;
}


IWvStreamCallback WvStream::setexceptcallback(IWvStreamCallback _callback)
{
    IWvStreamCallback tmp = exceptcb;

    exceptcb = _callback;

    return tmp;
}


IWvStreamCallback WvStream::setclosecallback(IWvStreamCallback _callback)
{
    IWvStreamCallback tmp = closecb;
    if (isok())
	closecb = _callback;
    else
    {
	// already closed?  notify immediately!
	closecb = 0;
	if (!!_callback)
	    _callback();
    }
    return tmp;
}


void WvStream::unread(WvBuf &unreadbuf, size_t count)
{
    WvDynBuf tmp;
    tmp.merge(unreadbuf, count);
    tmp.merge(inbuf);
    inbuf.zap();
    inbuf.merge(tmp);
}


IWvStream *WvStream::find_by_wsid(WSID wsid)
{
    IWvStream *retval = NULL;

    if (wsid_map)
    {
	map<WSID, WvStream*>::iterator it = wsid_map->find(wsid);

	if (it != wsid_map->end())
	    retval = it->second;
    }

    return retval;
}
