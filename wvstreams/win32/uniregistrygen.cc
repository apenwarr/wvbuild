/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2004 Net Integration Technologies, Inc.
 * 
 * A generator that exposes the windows registry.
 */
#include "uniregistrygen.h"
#include "wvmoniker.h"
#include "wvlinkerhack.h"

WV_LINK(UniRegistryGen);


static LONG openregkey(HKEY hLastKey, WvStringParm subkey,
		       REGSAM samDesired, HKEY *phNextKey, bool create)
{
    if (create)
	return RegCreateKeyEx(hLastKey, subkey, 0, NULL, 0, samDesired, 
	    NULL, phNextKey, NULL);
    return RegOpenKeyEx(hLastKey, subkey, 0, samDesired, phNextKey);
}

// returns a handle to the key specified by key, or, if key specifies a value,
// a handle to the key containing that value (and setting isValue = true)
static HKEY follow_path(HKEY from, const UniConfKey &key,
			bool create, bool *isValue)
{
    const REGSAM samDesired = KEY_READ | KEY_WRITE;
    LONG result;
    HKEY hLastKey = from; // DuplicateHandle() does not work with regkeys
    int n = key.numsegments();

    if (isValue) *isValue = false;

    for (int i=0;i<n;i++)
    {
	WvString subkey = key.segment(i).printable();
	HKEY hNextKey;

	result = openregkey(hLastKey, subkey, samDesired, &hNextKey, create);
	// In Windows Vista, you can't seemingly get KEY_WRITE permissions on
	// HKEY_LOCAL_MACHINE/Software (and likely others) if running from
	// within certain processes (presumably ones that set some magic
	// incantation).  We don't care, of course, since we don't intend to
	// write this key, and this wonderful fix fixes all.
	if (result == ERROR_ACCESS_DENIED)
	    result = openregkey(hLastKey, subkey, KEY_READ, &hNextKey, create);

	if ((result == ERROR_FILE_NOT_FOUND) && (i == n-1))
	{
	    WvString xsub(subkey=="." ? WvString::null : subkey);
	    
	    // maybe the last segment is a value name
	    result = RegQueryValueEx(hLastKey, xsub, 0, NULL, NULL, NULL);
	    if (result == ERROR_SUCCESS)
	    {
		// ... it is a value
		if (isValue) *isValue = true;
		break;
	    }
	}
	if (result != ERROR_SUCCESS)
	{
	    return 0;
	}
	
	
	if (i > 0)
	{
	    RegCloseKey(hLastKey);
	}
	hLastKey = hNextKey;
    }

    return hLastKey;
}


UniRegistryGen::UniRegistryGen(WvString _moniker) :
    m_log(_moniker), m_hRoot(0)
{
    UniConfKey key = _moniker;
    WvString hive = key.first().printable();
    if (strcmp("HKEY_CLASSES_ROOT", hive) == 0)
	m_hRoot = HKEY_CLASSES_ROOT;
    else if (strcmp("HKEY_CURRENT_USER", hive) == 0)
	m_hRoot = HKEY_CURRENT_USER;
    else if (strcmp("HKEY_LOCAL_MACHINE", hive) == 0)
	m_hRoot = HKEY_LOCAL_MACHINE;
    else if (strcmp("HKEY_USERS", hive) == 0)
	m_hRoot = HKEY_USERS;
    
    m_hRoot = follow_path(m_hRoot, key.range(1, key.numsegments()), true, NULL);

#if 0
    // FIXME: Notifications don't work for external registry changes.
    //
    hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    RegNotifyChangeKeyValue(
	m_hRoot,
	TRUE,
	REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES |
	REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY,
	hEvent,
	TRUE
    );
#endif
}

UniRegistryGen::~UniRegistryGen()
{
    if (m_hRoot)
    {
	RegCloseKey(m_hRoot);
	m_hRoot = 0;
    }
}

bool UniRegistryGen::isok()
{
    return m_hRoot != 0;
}

WvString UniRegistryGen::get(const UniConfKey &key)
{
    WvString retval = WvString::null;
    bool isvalue;
    HKEY hKey = follow_path(m_hRoot, key, false, &isvalue);

    WvString value;
    if (isvalue)
    {
	// the path ends up at a value so fetch that
	value = key.last();
	if (value == ".") value = WvString::null;
    }
    else
    {
	// the key isn't a value, fetch its default value instead
	value = WvString::null;
    }
    
    DWORD type;
    TCHAR data[1024];
    DWORD size = sizeof(data) / sizeof(data[0]) - 1;
    LONG result = RegQueryValueEx(
	hKey, 
	value.cstr(), 
	0, 
	&type, 
	(BYTE *) data, 
	&size
    );

    if (result == ERROR_SUCCESS)
    {
	switch (type)
	{
	case REG_DWORD:
	    retval.setsize(11);
	    itoa(*((int *) data), retval.edit(), 10);
	    break;
	case REG_SZ:
	case REG_EXPAND_SZ:
	case REG_MULTI_SZ:
	case REG_BINARY:
	    data[size] = 0;
	    retval = data;
	    break;
	default:
	    fprintf(stderr, "uniregistrygen: unknown data type 0x%08X\n",
		    (int)type);
	    break;
	};
    }

    if (hKey != m_hRoot) RegCloseKey(hKey);
    return retval;
}

void UniRegistryGen::set(const UniConfKey &key, WvStringParm value)
{
    LONG result;
    HKEY hKey = follow_path(m_hRoot, key.first( key.numsegments()-1 ), true, NULL);
    if (hKey)
    {
	if (value.isnull())
	{
	    result = RegDeleteValue(hKey, key.last().printable());
	}
	else
	{
	    WvString last = key.last();
	    if (last == ".") last = WvString::null;
	    result = RegSetValueEx(
		hKey,
		last,
		0,
		REG_SZ,
		(BYTE *) value.cstr(),
		strlen(value)+1
	    );
	}
	if (result == ERROR_SUCCESS)
	{
	    delta(key, value);
	}
    }
    if (hKey != m_hRoot) RegCloseKey(hKey);
}

void UniRegistryGen::setv(const UniConfPairList &pairs)
{
    setv_naive(pairs);
}

bool UniRegistryGen::exists(const UniConfKey &key)
{
    return !get(key).isnull();
}

bool UniRegistryGen::haschildren(const UniConfKey &key)
{
    UniRegistryGenIter iter(*this, key, m_hRoot);
    iter.rewind();
    return iter.next();
}


UniConfGen::Iter *UniRegistryGen::iterator(const UniConfKey &key)
{
    return new UniRegistryGenIter(*this, key, m_hRoot);
}


UniRegistryGenIter::UniRegistryGenIter(UniRegistryGen &gen,
				       const UniConfKey &key, HKEY base)
    : m_hKey(0), m_enumerating(KEYS), m_index(0), gen(gen), parent(key),
      m_dontClose(base)
{
    bool isValue;
    HKEY hKey = follow_path(base, key, false, &isValue);
    
    // fprintf(stderr, "(iter:%s:%d:%p)\n",
    //	    key.printable().cstr(), isValue, hKey); fflush(stderr);
	    
    if (isValue)
    {
	// a value doesn't have subkeys
	if (hKey != m_dontClose) RegCloseKey(hKey);
	m_enumerating = VALUES;
    }
    else
	m_hKey = hKey;
}


UniRegistryGenIter::~UniRegistryGenIter()
{
    if (m_hKey && m_hKey != m_dontClose)
	RegCloseKey(m_hKey);
}


void UniRegistryGenIter::rewind()
{
    current_key = "YOU HAVE TO REWIND, DUMMY!";
    m_enumerating = KEYS;
    m_index = 0;
}


bool UniRegistryGenIter::next()
{
    if (m_enumerating == KEYS)
    {
	LONG result = next_key();
	if (result == ERROR_SUCCESS)
	    return true;
	else if (result == ERROR_NO_MORE_ITEMS)
	{
	    // done enumerating keys, now enumerate the values
	    m_enumerating = VALUES;
	    m_index = 0;
	}
	else
	{
	    fprintf(stderr, "KEY_ENUM result: %ld\n", result);
	    fflush(stderr);
	    return false; // give up
	}
    }
    assert(m_enumerating == VALUES);
    LONG result = next_value();
    if (result == ERROR_SUCCESS)
	return true;
    return false;
}

UniConfKey UniRegistryGenIter::key() const
{
    return current_key;
}


WvString UniRegistryGenIter::value() const
{
    UniConfKey val(parent, current_key);
    return gen.get(val);
}


LONG UniRegistryGenIter::next_key()
{
    if (!m_hKey)
	return ERROR_NO_MORE_ITEMS;
    
    FILETIME dontcare;
    TCHAR data[1024];
    DWORD size = sizeof(data) / sizeof(data[0]);
    LONG result = RegEnumKeyEx(m_hKey, m_index++, data, &size, 0, 0, 0, &dontcare);
    if (result == ERROR_SUCCESS)
	current_key = data;
    return result;
}


LONG UniRegistryGenIter::next_value()
{
    if (!m_hKey)
	return ERROR_NO_MORE_ITEMS;
    
    TCHAR data[1024] = "";
    DWORD size = sizeof(data) / sizeof(data[0]);
    while (!*data)
    {
	LONG result = RegEnumValue(m_hKey, m_index++, data, &size, 0, 0, 0, 0);
	if (result != ERROR_SUCCESS)
	    return result;
    }
    current_key = data;
    return ERROR_SUCCESS;
}


static IUniConfGen *creator(WvStringParm s, IObject*)
{
    return new UniRegistryGen(s);
}

WvMoniker<IUniConfGen> UniRegistryGenMoniker("registry", creator);
