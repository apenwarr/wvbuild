/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 2002 Net Integration Technologies, Inc.
 * 
 * UniDefGen is a UniConfGen for retrieving data with defaults
 */
#include "unidefgen.h"
#include "wvmoniker.h"
//#include "wvstream.h"
#include <ctype.h>
#include <stdlib.h>

#include "wvlinkerhack.h"

WV_LINK(UniDefGen);


// if 'obj' is non-NULL and is a UniConfGen, wrap that; otherwise wrap the
// given moniker.
static IUniConfGen *creator(WvStringParm s, IObject *_obj)
{
    return new UniDefGen(wvcreate<IUniConfGen>(s, _obj));
}

// this name is too confusing.  We should deprecate it.
static WvMoniker<IUniConfGen> reg("default", creator);

// a better name for the same thing.
static WvMoniker<IUniConfGen> reg2("wildcard", creator);


UniConfKey UniDefGen::finddefault(const UniConfKey &key, char *p, char *q)
{
    UniConfKey result;
    
    if (!p)
    {
	q++;
	if (inner() && inner()->exists(q))
	    return q;
	else
	    return UniConfKey();
    }

    // pop the first segment of p to r
    char *r = strchr(p, '/');
    if (r)
        *r++ = '\0';

    // append p to q
    char *s = strchr(q, '\0');
    *s++ = '/';
    *s = 0;
    q = strcat(q, p);

    // try this literal path
    result = finddefault(key, r, q);
    if (result.numsegments())
        return result;

    // replace what used to be p with a *
    *s++ = '*';
    *s = '\0';
    result = finddefault(key, r, q);

    if (r)
        *--r = '/';
    
    return result;
}


WvString UniDefGen::replacewildcard(const UniConfKey &key,
			    const UniConfKey &defkey, WvStringParm in)
{
    // check if the result wants a wildcard ('*n')
    if (in.len() < 2 || in[0] != '*')
        return in;

    const char *s = in.cstr();
    int idx = atoi(s+1);
    if (idx == 0)
        return in;

    // search backwards for segment num of the n'th wildcard
    UniConfKey k(defkey);
    int loc = key.numsegments();
    for (int i = 0; i < idx; i++)
    {
        if (i != 0)
        {
            k = k.removelast();
            loc--;
        }
        while (!k.last().iswild())
        {
            k = k.removelast();
            loc--;
            if (k.isempty())
            {
                // oops, ran out of segments!
                return WvString();
            }
        }
    }


    // pull the literal from that segment num of the key
    return key.segment(loc-1);
}


bool UniDefGen::keymap(const UniConfKey &unmapped_key, UniConfKey &mapped_key)
{
    WvString tmp_key(unmapped_key), tmp("");
    char *p = tmp_key.edit();

    tmp.setsize(strlen(tmp_key) * 2);
    char *q = tmp.edit();
    *q = '\0';

    mapped_key = finddefault(unmapped_key, p, q);
    if (!mapped_key.numsegments())
	mapped_key = unmapped_key;
    // fprintf(stderr, "mapping '%s' -> '%s'\n", key.cstr(), result.cstr());
    
    return true;
}


WvString UniDefGen::get(const UniConfKey &key)
{
    UniConfKey mapped_key;
    if (keymap(key, mapped_key))
        return replacewildcard(key, mapped_key,
 			    inner() ? inner()->get(mapped_key) : WvString());
    else
        return WvString::null;
}


void UniDefGen::set(const UniConfKey &key, WvStringParm value)
{
    // no keymap() on set()
    if (inner())
	inner()->set(key, value);
}
