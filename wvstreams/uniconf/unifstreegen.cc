#include "uniconfgen.h"
#include "unimountgen.h"
#include "wvmoniker.h"
#include "wvlinkerhack.h"
#include "wvlog.h"

#include "unifiltergen.h"

class UniAutoMountGen : public UniFilterGen
{
    WvString dir;
    UniMountGen *mount;
    IUniConfGen *treegen;
    WvLog log;
    
public:
    UniAutoMountGen(WvStringParm _dir)
	: UniFilterGen(mount = new UniMountGen), dir(_dir),
          log(WvString("AutoMount '%s'", dir), WvLog::Info)
    {
	log("Starting.\n");
	mount->mount("/", WvString("readonly:fs:%s", dir), true);
	treegen = mount->whichmount("/", NULL);
    }
    
    virtual ~UniAutoMountGen()
    {
	log("Stopping.\n");
    }
    
    virtual bool keymap(const UniConfKey &key, UniConfKey &mapped_key)
    {
	automount(key);
	return UniFilterGen::keymap(key, mapped_key);
    }
    
    void automount(const UniConfKey &key)
    {
	IUniConfGen *parent = mount->whichmount(key, NULL);
	if (parent && parent != treegen && parent->haschildren("/"))
	    return; // don't bother; already mounted a parent
	
	log("Automount for '%s'\n", key);
	
	for (int count = key.numsegments(); count >= 0; count--)
	{
	    UniConfKey k(key.first(count));
	    if (mount->ismountpoint(k))
	    {
		log("Key '%s' already mounted.\n", k);
		return; // already mounted
	    }
	    
	    WvString filename("%s/%s", dir, k);
	    log("Filename is '%s'\n", filename);
	    mount->mount(k, WvString("ini:%s", filename), true);
	    log("Key '%s' newly mounted.\n", k);
	    return; // newly mounted
	}
	
	// just plain not found
	log("Key '%s' not found.\n", key);
    }
    
    virtual IUniConfGen::Iter *recursiveiterator(const UniConfKey &key)
    {
	// don't try to optimize this like UniMountGen does, because we're
	// going to mount things *as* we iterate through them, not sooner.
	// Use the default UniConfGen implementation, which just recursively
	// calls iterator().
	return UniConfGen::recursiveiterator(key);
    }
};


WV_LINK(UniFsTreeGen);


static IUniConfGen *creator(WvStringParm s, IObject *)
{
    return new UniAutoMountGen(s);
}

WvMoniker<IUniConfGen> UniFsTreeGenMoniker("fstree", creator);


