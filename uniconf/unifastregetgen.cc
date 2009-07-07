/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 2002-2005 Net Integration Technologies, Inc.
 * 
 * A lightweight but slightly dangerous version of UniCacheGen.
 */
#include <wvassert.h>

#include "unifastregetgen.h"
#include "uniconftree.h"
#include "wvmoniker.h"

// if 'obj' is non-NULL and is a UniConfGen, wrap that; otherwise wrap the
// given moniker.
static IUniConfGen *creator(WvStringParm s, IObject *_obj)
{
    return new UniFastRegetGen(wvcreate<IUniConfGen>(s, _obj));
}

static WvMoniker<IUniConfGen> reg("fast-reget", creator);


UniFastRegetGen::UniFastRegetGen(IUniConfGen *_inner) :
    UniFilterGen(_inner),
    tree(NULL)
{
    tree = new UniConfValueTree(NULL, "/", UniFilterGen::get("/"));
}


UniFastRegetGen::~UniFastRegetGen()
{
    if (tree)
    {
	delete tree;
	tree = NULL;
    }
}


void UniFastRegetGen::gencallback(const UniConfKey &key, WvStringParm value)
{
    if (tree == NULL)
        return; // initialising

    UniConfValueTree *t = tree->find(key);
    if (t) // never previously retrieved; don't cache it
	t->setvalue(value);
    UniFilterGen::gencallback(key, value);
}


WvString UniFastRegetGen::get(const UniConfKey &key)
{
    if (!tree)
    {
	wvassert(tree, "key: '%s'", key);
	abort();
    }

    // Keys with trailing slashes can't have values set on them
    if (key.hastrailingslash())
        return WvString::null;

    UniConfValueTree *t = tree->find(key);
    if (!t)
    {
        UniConfKey parentkey(key.removelast());
	get(parentkey); // guaranteed to create parent node
	t = tree->find(parentkey);
	assert(t);
	
	WvString value;
	if (!t->value().isnull()) // if parent is null, child guaranteed null
	    value = UniFilterGen::get(key);
	new UniConfValueTree(t, key.last(), value);
	return value;
    }
    else
	return t->value();
}


bool UniFastRegetGen::exists(const UniConfKey &key)
{
    // even if inner generator has a more efficient version of exists(),
    // do it this way so we can cache the result.
    return !get(key).isnull();
}


bool UniFastRegetGen::haschildren(const UniConfKey &key)
{
    if (!tree)
    {
	wvassert(tree, "key: '%s'", key);
	abort();
    }

    // if we already know the node is null, we can short circuit this one
    UniConfValueTree *t = tree->find(key);
    if (t && t->value().isnull())
	return false; // definitely no children
    return UniFilterGen::haschildren(key);
}
