#include "wvtest.h"

#define __WVSTREAM_UNIT_TEST 1
#include "wvstream.h"


#include "wvistreamlist.h"
#include "wvcont.h"
#include "wvtimeutils.h"

class ReadableStream : public WvStream
{
public:
    bool yes_readable;
    ReadableStream()
        { yes_readable = false; }
    
    virtual void pre_select(SelectInfo &si)
    {
	WvStream::pre_select(si);
	if (yes_readable && si.wants.readable)
	    si.msec_timeout = 0;
    }

    virtual bool post_select(SelectInfo &si)
    {
        bool ret = WvStream::post_select(si);
        if (yes_readable && si.wants.readable)
            return true;
        
        return ret;
    }
};

class CountStream : public WvStream
{
public:
    size_t rcount, wcount;
    bool yes_writable, block_writes;
    
    CountStream()
    { 
	printf("countstream initializing.\n");
	rcount = wcount = 0; yes_writable = block_writes = false;
	int num = geterr();
	printf("countstream error: %d (%s)\n", num, errstr().cstr());
	printf("countstream new error: %d\n", geterr());
    }
    
    virtual ~CountStream()
    	{ close(); }
    
    virtual size_t uread(void *buf, size_t count)
    {
	size_t ret = WvStream::uread(buf, count);
	rcount += ret;
	return ret;
    }
    
    virtual size_t uwrite(const void *buf, size_t count)
    {
	fprintf(stderr, "I'm uwrite! (%d)\n", count);
	if (block_writes)
	    return 0;
	size_t ret = WvStream::uwrite(buf, count);
	assert(ret == count);
	wcount += ret;
	return ret;
    }
    
    virtual bool post_select(SelectInfo &si)
    {
	printf("countstream post_select\n");
	int ret = WvStream::post_select(si);
	if (yes_writable 
	  && (si.wants.writable || (outbuf_used() && want_to_flush)))
	    return true;
	else
	    return ret;
    }
};



WVTEST_MAIN("buffered read/write")
{
    WvStream s;
    char buf[1024];
    
    // buffered reads and writes
    WVPASS(!s.isreadable());
    WVPASS(!s.iswritable());
    WVFAIL(s.read(buf, 1024) != 0);
    WVPASSEQ(s.write(buf, 1024), 1024);
    WVPASS(!s.iswritable());
    WVPASS(!s.isreadable());
    WVPASS(s.isok());
    
    // close() shouldn't have to wait to flush buffers, because plain
    // WvStream has no way to actually flush them.
    WvTime t1 = wvtime();
    s.close();
    WvTime t2 = wvtime();
    WVPASS(msecdiff(t2, t1) >= 0);
    WVPASS(msecdiff(t2, t1) < 1000);
	
    // after close()
    WVPASS(!s.isok());
}


// error tests
WVTEST_MAIN("errors")
{
    WvStream a, b;
    
    WVPASS(a.isok());
    WVPASS(!a.geterr());
    a.seterr(EBUSY);
    WVPASS(!a.isok());
    WVPASSEQ(a.geterr(), EBUSY);
    WVPASSEQ(a.errstr(), strerror(EBUSY));
    
    b.seterr("I'm glue!");
    WVPASS(!b.isok());
    WVPASSEQ(b.geterr(), -1);
    WVPASSEQ(b.errstr(), "I'm glue!");
}


// noread/nowrite behaviour
WVTEST_MAIN("noread/nowrite")
{
    WvStream s;
    char buf[1024];

    s.nowrite();
    WVPASS(s.isok());
    WVFAIL(s.write(buf, 1024) != 0);
    s.noread();
    WVPASS(!s.isok());
}


// getline tests
WVTEST_MAIN("getline")
{
    WvStream s;
    char buf[1024];
    
    WVPASS(!s.isreadable());
    s.inbuf_putstr("a\n b \r\nline");
    WVPASS(s.isreadable());
    s.noread();
    WVPASS(s.isreadable());
    
    WVPASSEQ(s.read(buf, 2), 2);
    char *line = s.getline();
    WVPASS(line);
    WVPASS(line && !strcmp(line, " b \r"));
    line = s.getline();
    WVPASS(line);
    WVPASS(line && !strcmp(line, "line"));
    WVPASS(!s.getline());
    
    WvTime t1 = wvtime();
    WVPASS(!s.blocking_getline(500));
    WvTime t2 = wvtime();
    WVPASS(msecdiff(t2, t1) >= 0);
    WVPASS(msecdiff(t2, t1) < 400); // noread().  shouldn't actually wait!
   
    WvStream t;
    t.inbuf_putstr("tremfodls\nd\ndopple");
    line = t.getline(0, '\n', 20);
    WVPASS(line && !strcmp(line, "tremfodls"));
    t.close();
    line = t.getline(0, '\n', 20);
    WVPASS(line && !strcmp(line, "d"));
    line = t.getline(0, '\n', 20);
    WVPASS(line && !strcmp(line, "dopple"));

    // FIXME: avoid aborting the entire test here on a freezeup!
#ifndef WIN32
    ::alarm(5); // crash after 5 seconds
    WVPASS(!s.blocking_getline(-1));
    ::alarm(0);
#endif 
}

// more noread/nowrite behaviour
WVTEST_MAIN("more noread/nowrite")
{
    WvStream s;
    
    s.inbuf_putstr("hello");
    s.write("yellow");
    WVPASS(s.isok());
    s.nowrite();
    WVPASS(s.isok());
    s.noread();
    WVPASS(s.isok());
    WVPASS(s.getline());
    WVFAIL(s.blocking_getline(-1));
    WVPASS(!s.isok());
}


static void val_cb(int *x)
{
    (*x)++;
}


static void val_icb(int &closeval)
{
    ++closeval;
}


// callback tests
WVTEST_MAIN("callbacks")
{
    int val = 0, closeval = 0;
    
    WvStream s;
    s.setcallback(wv::bind(val_cb, &val));
    s.setclosecallback(wv::bind(&val_icb, wv::ref(closeval)));
    
    WVPASS(!val);
    WVPASS(!closeval);
    s.runonce(0);
    WVPASS(!val);
    s.inbuf_putstr("gah");
    s.runonce(0);
    WVPASSEQ(val, 1); // callback works?
    s.runonce(0);
    WVPASSEQ(val, 2); // level triggered?
    s.getline();
    WVPASSEQ(val, 2); // but not by getline
    WVPASS(!closeval);
    s.inbuf_putstr("blah!");
    s.nowrite();
    s.noread();
    s.runonce(0);
    WVPASSEQ(val, 3);
    WVPASS(s.getline());
    s.runonce(0);
    WVPASSEQ(val, 3);
    WVPASSEQ(closeval, 1);
    s.runonce(0);
    WVPASSEQ(closeval, 1);
    s.close();
    WVPASSEQ(closeval, 1);
}


static void ccb_isok(WvStream &s)
{
    WVPASS(!s.isok());
}


WVTEST_MAIN("closecallback-isok")
{
    WvStream s;
    s.setclosecallback(wv::bind(ccb_isok, wv::ref(s)));
    s.close();
}


// autoforward and various buffer-related tests
WVTEST_MAIN("autoforward and buffers")
{
    WvStream a;
    CountStream b;
    int val = 0;
    
    fprintf(stderr, "a error: '%s'\n", a.errstr().cstr());
    fprintf(stderr, "b error: '%s'\n", b.errstr().cstr());
    
    a.autoforward(b);
    b.setcallback(wv::bind(val_cb, &val));
    
    WVPASS(a.isok());
    WVPASS(b.isok());
    
    a.inbuf_putstr("astr");
    WVPASSEQ(a.inbuf_used(), 4);
    a.runonce(0);
    WVPASS(!a.inbuf_used());
    b.runonce(0);
    WVPASSEQ(val, 0);
    WVPASSEQ(b.wcount, 4);
    a.noautoforward();
    a.inbuf_putstr("astr2");
    a.runonce(0);
    WVPASSEQ(a.inbuf_used(), 5);
    WVPASSEQ(b.wcount, 4);
    
    // delay_output tests
    a.autoforward(b);
    b.delay_output(true);
    a.runonce(0);
    WVPASS(!a.inbuf_used());
    WVPASSEQ(b.wcount, 4);
    WVPASSEQ(b.outbuf_used(), 5);
    b.runonce(0);
    WVFAIL(!b.outbuf_used());
    b.yes_writable = true;
    b.runonce(0);
    WVFAIL(!b.outbuf_used());
    b.flush(0);
    WVPASS(!b.outbuf_used());
    WVPASSEQ(b.wcount, 4+5);
    
    // autoforward() has lower precedence than drain()
    WVPASS(!a.inbuf_used());
    a.inbuf_putstr("googaa");
    a.drain();
    WVPASSEQ(b.wcount, 4+5);
    WVPASS(!a.inbuf_used());
    
    // queuemin() works
    a.inbuf_putstr("phleg");
    a.queuemin(2);
    WVPASS(a.isreadable());
    a.queuemin(6);
    WVFAIL(a.isreadable());
    a.drain();
    WVFAIL(a.getline());
    WVFAIL(a.isreadable());
    WVFAIL(!a.inbuf_used());
    a.inbuf_putstr("x");
    WVPASS(a.isreadable());
    WVPASSEQ(a.inbuf_used(), 6);
    a.drain();
    WVPASS(!a.inbuf_used());
}


// flush_then_close()
WVTEST_MAIN("flush_then_close")
{
    CountStream s;
    
    s.block_writes = true;
    s.write("abcdefg");
    WVPASSEQ(s.outbuf_used(), 7);
    s.flush(0);
    WVPASSEQ(s.outbuf_used(), 7);
    s.runonce(0);
    WVPASSEQ(s.outbuf_used(), 7);
    s.flush_then_close(20000);
    WVPASSEQ(s.outbuf_used(), 7);
    s.runonce(0);
    WVPASSEQ(s.outbuf_used(), 7);
    WVPASS(s.isok());
    s.yes_writable = true;
    s.block_writes = false;
    s.runonce(0);
    s.runonce(0);
    WVPASSEQ(s.outbuf_used(), 0);
    WVFAIL(s.isok());
}


// force_select and the globallist
WVTEST_MAIN("force_select and globallist")
{
    int val = 0;
    CountStream s, x;
    s.yes_writable = x.yes_writable = true;
    
    WVFAIL(s.select(0));
    WVPASS(s.select(0, false, true));
    s.inbuf_putstr("hello");
    WVPASS(s.select(0));
    WVFAIL(s.select(0, false, false));
    s.undo_force_select(true, false, false);
    WVFAIL(s.select(0));
    s.force_select(false, true, false);
    WVPASS(s.select(0));
    s.yes_writable = false;
    WVFAIL(s.select(0));
    
    x.setcallback(wv::bind(val_cb, &val));
    WvIStreamList::globallist.append(&x, false, "countstream");
    WVFAIL(s.select(0));
    WVPASS(!val);
    x.inbuf_putstr("yikes");
    x.force_select(false, true, false);
    WVPASS(!s.select(0));
    WVPASS(val == 1);
    WVPASS(!s.select(0));
    WVPASS(val == 2);
    WVPASS(!s.select(0, false, true));
    s.yes_writable = true;
    WVPASS(s.select(0, false, true));
    WVPASS(val == 2);
    s.runonce();
    WVPASS(val == 3);
    
    WvIStreamList::globallist.unlink(&x);
}


static void cont_cb(WvStream &s, int *x)
{
    int next = 1, sgn = 1;
    
    while (s.isok())
    {
	*x = sgn * next;
	next *= 2;
	sgn = s.continue_select(0) ? 1 : -1;
    }
}


WVTEST_MAIN("continue_select")
{
    WvStream a;
    int aval = 0;
    
    a.uses_continue_select = true;
    a.setcallback(wv::bind(cont_cb, wv::ref(a), &aval));
    
    a.runonce(0);
    WVPASS(aval == 0);
    a.inbuf_putstr("gak");
    a.runonce(0);
    WVPASS(aval == 1);
    a.runonce(0);
    WVPASS(aval == 2);
    a.drain();
    WVPASS(aval == 2);
    a.alarm(-1);
    a.runonce(0);
    WVPASS(aval == 2);
    a.alarm(0);
    a.runonce(0);
    WVPASS(aval == -4);
    
    a.terminate_continue_select();
}


static void cont_once(WvStream &s, int *i)
{
    (*i)++;
    s.continue_select(10);
    (*i)++;
    *i = -*i;
}


WVTEST_MAIN("continue_select and alarm()")
{
    int i = 1;
    ReadableStream s;
    s.uses_continue_select = true;
    s.setcallback(wv::bind(cont_once, wv::ref(s), &i));
    
    s.yes_readable = true;
    WVPASSEQ(i, 1);
    s.runonce(100);
    WVPASSEQ(i, 2);
    s.runonce(100);
    WVPASSEQ(i, -3);

    s.yes_readable = false;
    s.runonce(100);
    WVPASSEQ(i, -3);
    
    s.alarm(0);
    s.runonce(100);
    WVPASSEQ(i, -2);
    
    s.alarm(-1); // disabling the alarm should disable continue_select timeout
    s.runonce(100);
    WVPASSEQ(i, -2);
    
    s.terminate_continue_select();
}


static void seterr_cb(WvStream &s)
{
    s.seterr("my seterr_cb error");
}


WVTEST_MAIN("seterr() inside a continuable callback")
{
    WvStream s;
    s.setcallback(wv::bind(seterr_cb, wv::ref(s)));
    WVPASS(s.isok());
    s.runonce(0);
    WVPASS(s.isok());
    s.alarm(0);
    s.runonce(5);
    WVFAIL(s.isok());
    WVPASS(s.geterr() == -1);
    s.terminate_continue_select();
}


static void *wvcont_cb(WvStream &s, void *userdata)
{
    int next = 1, sgn = 1;
    int *val = (int *)userdata;
    
    while (s.isok())
    {
	*val = sgn * next;
	next *= 2;
	sgn = WvCont::yield() ? 1 : -1;
	printf("...returned from yield()\n");
    }
    
    *val = 4242;
    return NULL;
}


static void call_wvcont_cb(WvCont *cb, void *userdata)
{
    (*cb)(userdata);
}


WVTEST_MAIN("continue_select compatibility with WvCont")
{
    WvStream s;
    int sval = 0;
    
    s.uses_continue_select = true;
    
    {
	WvCont cont1(wv::bind(&wvcont_cb, wv::ref(s), _1));
	WvCont cont2(cont1), cont3(cont2);
	s.setcallback(wv::bind(&call_wvcont_cb, &cont3, &sval));
	
	s.inbuf_putstr("gak");
	WVPASS(sval == 0);
	s.runonce(0);
	WVPASS(sval == 1);
	s.runonce(0);
	WVPASS(sval == 2);
	s.close();
	WVPASS(!s.isok());
	s.runonce(0);
	s.setcallback(0);
    }
    
    // the WvCont should have now been destroyed!
    WVPASS(sval == 4242);
}


static void alarmcall(WvStream &s, int *userdata)
{
    WVPASS(s.alarm_was_ticking);
    val_cb(userdata);
}


// alarm()
WVTEST_MAIN("alarm")
{
    int val = 0;
    WvStream s;
    s.setcallback(wv::bind(alarmcall, wv::ref(s), &val));
    
    s.runonce(0);
    WVPASSEQ(val, 0);
    s.alarm(0);
    s.runonce(0);
    WVPASSEQ(val, 1);
    s.runonce(5);
    WVPASSEQ(val, 1);
    s.alarm(-5);
    WVPASS(s.alarm_remaining() == -1);
    s.runonce(0);
    WVPASSEQ(val, 1);
    s.alarm(100);
    time_t remain = s.alarm_remaining();
    WVPASS(remain > 0);
    WVPASS(remain <= 100);
    s.runonce(0);
    WVPASSEQ(val, 1);
    s.runonce(10000);
    WVPASSEQ(val, 2);
    WVPASSEQ(s.geterr(), 0);
    printf("alarm remaining: %d\n", (int)s.alarm_remaining());
    s.runonce(50);
    WVPASSEQ(val, 2);
    printf("alarm remaining: %d\n", (int)s.alarm_remaining());
 
    WvIStreamList l;
    l.append(&s, false, "alarmer");
    
    s.alarm(1);
    l.runonce(10);
    l.runonce(10);
    l.runonce(10);
    WVPASSEQ(val, 3);
}


static int rn = 0;


static void rcb2(WvStream &s)
{
    rn++;
    WVPASS(rn == 2);
    s.setcallback(0);
}


static void rcb(WvStream &s)
{
    rn++;
    WVPASS(rn == 1);
    s.setcallback(wv::bind(rcb2, wv::ref(s)));
    WVPASS(rn == 1);
    //s.continue_select(0);
    //WVPASS(rn == 1);
}


WVTEST_MAIN("self-redirection")
{
    WvStream s;
    s.uses_continue_select = true;
    s.setcallback(wv::bind(rcb, wv::ref(s)));
    s.inbuf_putstr("x");
    s.runonce(0);
    s.runonce(0);
    s.runonce(0);
    s.runonce(0);
    WVPASS(rn == 2);
    s.terminate_continue_select();
}
