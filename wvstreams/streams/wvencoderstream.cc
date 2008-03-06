/*
 * Worldvisions Tunnel Vision Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * WvEncoderStream chains a series of encoders on the input and
 * output ports of the underlying stream to effect on-the-fly data
 * transformations.
 */
#include "wvencoderstream.h"

WvEncoderStream::WvEncoderStream(WvStream *_cloned) : WvStreamClone(_cloned)
{
    is_closing = false;
    min_readsize = 0;
}


WvEncoderStream::~WvEncoderStream()
{
    close();
}


void WvEncoderStream::close()
{
    // fprintf(stderr, "Encoderstream close!\n");
    
    // we want to finish the encoders even if !isok() since we
    // might just have encountered an EOF condition, and we want
    // to ensure that the remaining data is processed, but this
    // might cause recursion if the encoders set a new error condition
    if (is_closing) return;
    is_closing = true;
    
    // finish encoders
    finish_read();
    finish_write();
    
    // flush write chain and close the stream
    WvStreamClone::close();
}


bool WvEncoderStream::isok() const
{
    //fprintf(stderr, "encoderstream isok: %d %p %d %d\n",
    //	    WvStream::isok(), cloned, cloned->isok(), cloned->geterr());
    
    // handle encoder error conditions
    if (!WvStream::isok())
        return false;

    // handle substream error conditions
    // we don't check substream isok() because that is handled
    // during read operations to distinguish EOF from errors
    if (!cloned || cloned->geterr() != 0)
        return false;
        
    return true;
}


bool WvEncoderStream::flush_internal(time_t msec_timeout)
{
    flush_write();
    return WvStreamClone::flush_internal(msec_timeout);
}


bool WvEncoderStream::flush_read()
{
    bool success = readchain.flush(readinbuf, readoutbuf);
    checkreadisok();
    inbuf.merge(readoutbuf);
    return success;
}


bool WvEncoderStream::flush_write()
{
    bool success = push(true /*flush*/, false /*finish*/);
    return success;
}


bool WvEncoderStream::finish_read()
{
    bool success = readchain.flush(readinbuf, readoutbuf);
    if (!readchain.finish(readoutbuf))
        success = false;
    checkreadisok();
    inbuf.merge(readoutbuf);
    // noread();
    return success;
}


bool WvEncoderStream::finish_write()
{
    return push(true /*flush*/, true /*finish*/);
}


void WvEncoderStream::pull(size_t size)
{
    // fprintf(stderr, "encoder pull %d\n", size);
    
    // pull a chunk of unencoded input
    bool finish = false;
    if (cloned)
    {
        if (size != 0)
            cloned->read(readinbuf, size);
        if (!cloned->isok())
            finish = true; // underlying stream hit EOF or error
    }
    
    // deal with any encoders that have been added recently
    WvDynBuf tmpbuf;
    tmpbuf.merge(readoutbuf);
    readchain.continue_encode(tmpbuf, readoutbuf);
    
    // apenwarr 2004/11/06: always flush on read, because otherwise there's
    // no clear way to decide when we need to flush.  Anyway, most "decoders"
    // (the kind of thing you'd put in the readchain) don't care whether you
    // flush or not.
    readchain.encode(readinbuf, readoutbuf, true);
    //readchain.encode(readinbuf, readoutbuf, finish /*flush*/);
    if (finish)
    {
        readchain.finish(readoutbuf);
        // if (readoutbuf.used() == 0 && inbuf.used() == 0)
	//    noread();
	close();
        // otherwise defer EOF until the buffered data has been read
    }
    else if (!readoutbuf.used() && !inbuf.used() && readchain.isfinished())
    {
        // only get EOF when the chain is finished and we have no
        // more data
	//noread();
	close();
    }
    checkreadisok();
}


bool WvEncoderStream::push(bool flush, bool finish)
{
    WvDynBuf writeoutbuf;
    
    // encode the output
    if (flush)
        writeinbuf.merge(outbuf);
    bool success = writechain.encode(writeinbuf, writeoutbuf, flush);
    if (finish)
        if (!writechain.finish(writeoutbuf))
            success = false;
    checkwriteisok();

#if 0
    // push encoded output to cloned stream
    size_t size = writeoutbuf.used();
    if (size != 0)
    {
        const unsigned char *writeout = writeoutbuf.get(size);
        size_t len = WvStreamClone::uwrite(writeout, size);
        writeoutbuf.unget(size - len);
    }
#endif
    if (cloned)
	cloned->write(writeoutbuf, writeoutbuf.used());
    
    return success;
}


size_t WvEncoderStream::uread(void *buf, size_t size)
{
    // fprintf(stderr, "encstream::uread(%d)\n", size);
    if (size && readoutbuf.used() == 0)
	pull(min_readsize > size ? min_readsize : size);
    size_t avail = readoutbuf.used();
    if (size > avail)
        size = avail;
    readoutbuf.move(buf, size);
    return size;
}


size_t WvEncoderStream::uwrite(const void *buf, size_t size)
{
    writeinbuf.put(buf, size);
    push(false /*flush*/, false /*finish*/);
    return size;
}


void WvEncoderStream::pre_select(SelectInfo &si)
{
    WvStreamClone::pre_select(si);

    if (si.wants.readable && readoutbuf.used() != 0)
        si.msec_timeout = 0;     
}


bool WvEncoderStream::post_select(SelectInfo &si)
{
    bool sure = false;
    
    // if we have buffered input data and we want to check for
    // readability, then cause a callback to occur that will
    // hopefully ask us for more data via uread()
    if (si.wants.readable && readoutbuf.used() != 0)
    {
        pull(0); // try an encode
        if (readoutbuf.used() != 0)
            sure = true;
    }
    
    // try to push pending encoded output to cloned stream
    // outbuf_delayed_flush condition already handled by uwrite()
    push(false /*flush*/, false /*finish*/);
    
    // consult the underlying stream
    sure |= WvStreamClone::post_select(si);

    return sure;
}


void WvEncoderStream::checkreadisok()
{
    if (!readchain.isok())
    {
        seterr(WvString("read chain: %s", readchain.geterror()));
	noread();
    }
}


void WvEncoderStream::checkwriteisok()
{
    if (!writechain.isok())
        seterr(WvString("write chain: %s", writechain.geterror()));
}
