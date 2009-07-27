/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * A class for managing error numbers and strings.
 */ 
#ifndef __WVERROR_H
#define __WVERROR_H

#include "wvstring.h"

/**
 * A class for managing error numbers and strings.
 *
 * It can have either a system error value, like those defined
 * in errno.h, or an arbitrary error string.  In either case, it
 * can return a string representation of the error message.
 * 
 * This object is most useful for using as a base class of your own class,
 * for historical/backwards compatibility reasons.  Consider using a WvError
 * instead, and making it a member of your class instead of a parent.
 */
class WvErrorBase
{
protected:
    int errnum;
    WvString errstring;

public:
    WvErrorBase()
        { noerr(); }
    virtual ~WvErrorBase();

    /**
     * By default, returns true if geterr() == 0.
     * Might be overridden so that isok() == false even though no
     * error code has been specified.
     */
    virtual bool isok() const
        { return errnum == 0; }

    /**
     * If isok() is false, return the system error number corresponding to
     * the error, -1 for a special error string (which you can obtain with
     * errstr()) or 0 on end of file.  If isok() is true, returns an
     * undefined number.
     */ 
    virtual int geterr() const
        { return errnum; }
    virtual WvString errstr() const;

    /**
     * A replacement for the operating system ::strerror() function that
     * can map more kinds of error strings (especially in win32).
     */
    static WvString strerror(int errnum);
    
    /**
     * Set the errnum variable -- we have an error.  If called more than
     * once, seterr() doesn't change the error code away from the previous
     * one.  That way, we remember the _original_ cause of our problems.
     * 
     * Subclasses may want to override seterr(int) to shut themselves down
     * (eg. WvStream::close()) when an error condition is set.
     * 
     * Note that seterr(WvString) will call seterr(-1).
     */
    virtual void seterr(int _errnum);
    void seterr(WvStringParm specialerr);
    void seterr(WVSTRING_FORMAT_DECL)
        { seterr(WvString(WVSTRING_FORMAT_CALL)); }
    void seterr_both(int _errnum, WvStringParm specialerr);
    void seterr_both(int _errnum, WVSTRING_FORMAT_DECL)
        { seterr_both(_errnum, WvString(WVSTRING_FORMAT_CALL)); }
    void seterr(const WvErrorBase &err);
    
    /** Reset our error state - there's no error condition anymore. */
    void noerr()
        { errnum = 0; errstring = WvString::null; }
};


/**
 * A variant of WvErrorBase suitable for embedding as a member of your own
 * object, preferably called 'err'.  It adds some extra convenience functions
 * to remove function name redundancy, so you can say "obj.err.get()" instead
 * of "obj.err.geterr()", for example.
 */
class WvError : public WvErrorBase
{
public:
    int get() const
        { return geterr(); }
    WvString str() const
        { return errstr(); }
    
    const WvError &set(int _errnum)
        { seterr(_errnum); return *this; }
    const WvError &set(WvStringParm specialerr)
        { seterr(specialerr); return *this; }
    const WvError &set(WVSTRING_FORMAT_DECL)
        { seterr(WvString(WVSTRING_FORMAT_CALL)); return *this; }
    const WvError &set_both(int _errnum, WvStringParm specialerr)
        { seterr_both(_errnum, specialerr); return *this; }
    const WvError &set_both(int _errnum, WVSTRING_FORMAT_DECL)
        { seterr_both(_errnum, WvString(WVSTRING_FORMAT_CALL)); return *this; }
    const WvError &set(const WvErrorBase &err)
        { seterr(err); return *this; }

    void reset()
        { noerr(); }
};


#endif // __WVERROR_H
