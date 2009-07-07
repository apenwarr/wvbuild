/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * WvTimeStream test.  Should take exactly ten seconds to run, but 
 * tests how well the time stream handles being executed in bursts.
 */

#include "wvtimestream.h"
#include "wvlog.h"
#include <sys/time.h>

WvLog mylog("timetest", WvLog::Info);
bool want_to_quit = false;
unsigned int count = 0;

void timer_callback(WvStream& s)
{
    if (s.alarm_was_ticking)
    {
	mylog("X ");
	s.alarm(200);
    }
    else
    {
	if (!(count % 10))
	    mylog("\n");

	mylog("%02s ", count);

	if (++count >= 100)
	    want_to_quit = true;
    }
}

int main()
{
    WvTimeStream t;
    
    free(malloc(1));
    
    mylog("Artificial burstiness test - should take exactly 10 seconds\n");

    t.setcallback(wv::bind(timer_callback, wv::ref(t)));
    t.set_timer(100);
    t.alarm(200);

    while (!want_to_quit)
    {
	if (t.select(-1))
	    t.callback();

	/*
	 * FIXME: It should be okay to sleep more than 100 ms here,
	 * but it isn't. The time stream knows it is late, and will
	 * force the select() timeout to zero to try catching up, but
	 * since we're sleeping outside of it, there's nothing it can
	 * do. If it could call the callback more than once per loop
	 * iteration, it could be fixed, but I'm not sure how to do
	 * that.
	 */
	//usleep((1 + (rand() % 500)) * 1000);
    }
    
    return 0;
}

