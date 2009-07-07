/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * See wvtimestream.h.
 */
#include "wvtimestream.h"

WvTimeStream::WvTimeStream():
    last(wvtime_zero), next(wvtime_zero), ms_per_tick(0)
{
}


void WvTimeStream::set_timer(time_t msec)
{
    WvTime now = wvstime();

    ms_per_tick = msec > 0 ? msec : 0;
    next = msecadd(now, ms_per_tick);
    last = now;
}


bool WvTimeStream::isok() const
{
    return true;
}


void WvTimeStream::pre_select(SelectInfo &si)
{
    WvTime now;
    time_t diff;
    WvStream::pre_select(si);
    
    //fprintf(stderr, "%p: timestream pre_select mspt=%ld msto=%ld\n",
    //     this, ms_per_tick, si.msec_timeout);

    if (ms_per_tick)
    {
	now = wvstime();
	
	/* Are we going back in time? If so, adjust the due time. */
	if (now < last)
	    next = tvdiff(next, tvdiff(last, now));
	
	last = now;

	if (next <= now)
	{
	    si.msec_timeout = 0;
	    return;
	}

	diff = msecdiff(next, now);
	diff = diff < 0 ? 0 : diff;
	if (diff < si.msec_timeout || si.msec_timeout < 0)
	    si.msec_timeout = diff;
    }
}


bool WvTimeStream::post_select(SelectInfo &si)
{
    WvTime now = wvstime();

    return WvStream::post_select(si) || (ms_per_tick && next <= now);
}


void WvTimeStream::execute()
{
    WvStream::execute();

    /* Schedule our next timer event, unless alarm_is_ticking, which
     * would mean that we're here because someone used alarm() rather
     * than because our timer expired. */
    if (!alarm_was_ticking)
    {
	WvTime now = wvstime();

        next = msecadd(next, ms_per_tick);
        
        if (msecdiff(next, now) > ms_per_tick * 100 || 
            msecdiff(now, next) > ms_per_tick * 100)
        {
            // reset if we fall forward or behind WAY too excessively
            // This is usually due to a change in system time
            last = now;
            next = msecadd(last, ms_per_tick);
        }
        else if (msecdiff(next, now) > ms_per_tick * 10)
	    // compensate if we fall behind too excessively
	    next = msecadd(now, ms_per_tick);
    }
}
