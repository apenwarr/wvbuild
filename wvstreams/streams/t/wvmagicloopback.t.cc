#include <sys/wait.h>
#include "wvtest.h"
#include "wvmagicloopback.h"
#include "wvfork.h"

#include <stdio.h>

WVTEST_MAIN("WvMagicLoopback Sanity") 
{
    signal(SIGPIPE, SIG_IGN);

    WvMagicLoopback ml(1024);
    
    pid_t pid = fork();
    WVFAIL(pid < 0 && "fork() failed");
    if (pid == 0)
    {
    	int i;
    	
    	for (i=0; i<1024; ++i)
    	{
    	    while (!ml.iswritable());
    	    
    	    ml.write(&i, sizeof(i));
    	}
    	
    	_exit(0);
    }
    else
    {
    	int i, maybe_i;
    	
    	for (i=0; i<1024; ++i)
    	{
    	    while (!ml.isreadable());
    	    
    	    ml.read(&maybe_i, sizeof(maybe_i));
    	    
    	    if (i != maybe_i)
    	    {
    	    	WVPASS(i == maybe_i);
    	    	break;
    	    }
    	}
    }
    
    pid_t rv;
    while ((rv = waitpid(pid, NULL, 0)) != pid)
    {
        // In case a signal is in the process of being delivered...
        if (rv == -1 && errno != EINTR)
            break;
    }
    WVPASS(rv == pid);
}

WVTEST_MAIN("WvMagicLoopback Non-Blocking Writes") 
{
    WvMagicLoopback ml(1024);

    WVPASS(ml.isok());

    for (int i=0; i<(1<<10); ++i)
    {
        char buf[1024] = "WvMagicLoopback Non-Blocking Writes";
        ml.write(buf, sizeof(buf));
    }

    WVPASS(ml.isok());
}
