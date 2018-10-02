/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * Process iterator.  Iterates through the running processes.
 *
 */

#ifndef __WVPROCITER_H
#define __WVPROCITER_H

#include "wvstringlist.h"

class WvPipe;


struct WvProcEnt
{
    pid_t pid;
    WvString exe;
    WvStringList cmdline;
};

class WvProcIter
{
private:
    WvPipe *p;
    WvProcEnt proc_ent;

public:
    WvProcIter();
    ~WvProcIter();

    bool isok() const;
    void rewind();
    bool next();

    const WvProcEnt *ptr() const { return &proc_ent; }
    WvIterStuff(const WvProcEnt);
};

bool wvkillall(WvStringParm basename, int sig); 

#endif
