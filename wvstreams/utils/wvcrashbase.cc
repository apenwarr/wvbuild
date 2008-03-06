/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 2005 Net Integration Technologies, Inc.
 *
 * Routines to save messages that can be logged when a program crashes.
 */
#include "wvcrash.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

IWvStream *WvCrashInfo::in_stream = NULL;
const char *WvCrashInfo::in_stream_id = NULL;
enum WvCrashInfo::InStreamState WvCrashInfo::in_stream_state = UNUSED;

// FIXME: this file mostly only works in Linux
#ifdef __linux

#ifdef __USE_GNU
static const char *argv0 = program_invocation_short_name;
#else
static const char *argv0 = "UNKNOWN";
#endif // __USE_GNU

// Reserve enough buffer for a screenful of programme.
static const int buffer_size = 2048;
static char will_msg[buffer_size];
static char assert_msg[buffer_size];

static const int ring_buffer_order = wvcrash_ring_buffer_order;
static const int ring_buffer_size = wvcrash_ring_buffer_size;
static const int ring_buffer_mask = ring_buffer_size - 1;
static char ring_buffer[ring_buffer_size+1];
static int ring_buffer_start = 0, ring_buffer_used = 0;

extern "C"
{
    // Support assert().
    void __assert_fail(const char *__assertion, const char *__file,
		       unsigned int __line, const char *__function)
    {
	// Set the assert message that WvCrash will dump.
	snprintf(assert_msg, buffer_size,
		 "%s: %s:%u: %s: Assertion `%s' failed.\n",
		 argv0, __file, __line, __function, __assertion);
	assert_msg[buffer_size - 1] = '\0';

	// Emulate the GNU C library's __assert_fail().
	fprintf(stderr, "%s: %s:%u: %s: Assertion `%s' failed.\n",
		argv0, __file, __line, __function, __assertion);
	abort();
    }


    // Wrapper for standards compliance.
    void __assert(const char *__assertion, const char *__file,
		  unsigned int __line, const char *__function)
    {
	__assert_fail(__assertion, __file, __line, __function);
    }


    // Support the GNU assert_perror() extension.
    void __assert_perror_fail(int __errnum, const char *__file,
			      unsigned int __line, const char *__function)
    {
	// Set the assert message that WvCrash will dump.
	snprintf(assert_msg, buffer_size,
		 "%s: %s:%u: %s: Unexpected error: %s.\n",
		 argv0, __file, __line, __function, strerror(__errnum));
	assert_msg[buffer_size - 1] = '\0';

	// Emulate the GNU C library's __assert_perror_fail().
	fprintf(stderr, "%s: %s:%u: %s: Unexpected error: %s.\n",
		argv0, __file, __line, __function, strerror(__errnum));
	abort();
    }
} // extern "C"


// This function is meant to support people who wish to leave a last will
// and testament in the WvCrash.
void wvcrash_leave_will(const char *will)
{
    if (will)
    {
	strncpy(will_msg, will, buffer_size);
	will_msg[buffer_size - 1] = '\0';
    }
    else
	will_msg[0] = '\0';
}


const char *wvcrash_read_will()
{
    return will_msg;
}


const char *wvcrash_read_assert()
{
    return assert_msg;
}


void wvcrash_ring_buffer_put(const char *str)
{
    wvcrash_ring_buffer_put(str, strlen(str));
}


void wvcrash_ring_buffer_put(const char *str, size_t len)
{
    while (len > 0)
    {
        int pos = (ring_buffer_start + ring_buffer_used) & ring_buffer_mask;
        ring_buffer[pos] = *str++;
        --len;
        if (ring_buffer_used == ring_buffer_size)
            ring_buffer_start = (ring_buffer_start + 1) & ring_buffer_mask;
        else
            ++ring_buffer_used;
    }
}


const char *wvcrash_ring_buffer_get()
{
    if (ring_buffer_used == 0)
        return NULL;
    const char *result;
    if (ring_buffer_start + ring_buffer_used >= ring_buffer_size)
    {
        ring_buffer[ring_buffer_size] = '\0';
        result = &ring_buffer[ring_buffer_start];
        ring_buffer_used -= ring_buffer_size - ring_buffer_start;
        ring_buffer_start = 0;
    }
    else
    {
        ring_buffer[ring_buffer_start + ring_buffer_used] = '\0';
        result = &ring_buffer[ring_buffer_start];
        ring_buffer_start += ring_buffer_used;
        ring_buffer_used = 0;
    }
    return result;
}


void __wvcrash_init_buffers(const char *program_name)
{
    if (program_name)
	argv0 = program_name;
    will_msg[0] = '\0';
    assert_msg[0] = '\0';
}


#else // __linux

void wvcrash_leave_will(const char *will) {}
const char *wvcrash_read_will() { return NULL; }

#endif // __linux
