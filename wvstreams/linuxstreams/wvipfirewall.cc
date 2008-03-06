/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * WvIPFirewall is an extremely simple hackish class that handles the Linux
 * 2.4 "iptables" firewall.  See wvipfirewall.h.
 */
#include "wvipfirewall.h"
#include "wvinterface.h"
#include <unistd.h>


bool WvIPFirewall::enable = false, WvIPFirewall::ignore_errors = true;


WvIPFirewall::WvIPFirewall() : log("Firewall", WvLog::Debug2)
{
    // don't change any firewall rules here!  Remember that there may be
    // more than one instance of the firewall object.
}


WvIPFirewall::~WvIPFirewall()
{
    zap();
}


WvString WvIPFirewall::port_command(const char *cmd, const char *proto,
				    const WvIPPortAddr &addr)
{
    WvIPAddr ad(addr), none;
    
    return WvString("iptables %s Services -j ACCEPT -p %s "
		    "%s --dport %s "
		    "%s",
		    cmd, proto,
		    ad == none ? WvString("") : WvString("-d %s", ad),
		    addr.port,
		    shutup());
}


WvString WvIPFirewall::redir_command(const char *cmd, const WvIPPortAddr &src,
				     int dstport)
{
    WvIPAddr ad(src), none;
    
    return WvString("iptables -t nat %s TProxy "
		    "-p tcp %s --dport %s "
		    "-j REDIRECT --to-ports %s "
		    "%s",
		    cmd,
		    ad == none ? WvString("") : WvString("-d %s", ad),
		    src.port, dstport,
		    shutup());
}

WvString WvIPFirewall::forward_command(const char *cmd, 
				       const char *proto,
				       const WvIPPortAddr &src,
				       const WvIPPortAddr &dst, bool snat)
{
    WvIPAddr srcaddr(src), dstaddr(dst), zero;
    WvString haveiface(""), haveoface("");
    if (!(srcaddr == zero))
    {
	haveiface.append("-d ");
	haveiface.append((WvString)srcaddr);
    }
    
    WvString retval;

    if ((dst == WvIPAddr("127.0.0.1")) || (dst == zero))
    {
        retval.append("iptables -t nat %s FASTFORWARD -p %s --dport %s %s "
                  "-j REDIRECT --to-port %s %s \n",
                   cmd, proto, src.port, haveiface, dst.port, shutup());
    }
    else
    {
	haveoface.append("-d ");
	haveoface.append((WvString)dstaddr);
    
        retval.append("iptables -t nat %s FASTFORWARD -p %s --dport %s %s "
                  "-j DNAT --to-destination %s "
                  "%s \n", cmd, proto, src.port, haveiface,  dst, shutup());
    }

    // FA57 is leet-speak for FAST, which is short for FASTFORWARD --adewhurst
    // FA58 is FA57+1. Nothing creative sprang to mind. --adewhurst
    // 
    // We need this to mark the packet as it comes in so that we allow the
    // FastForward-ed packets to bypass the firewall (FA57).
    // 
    // If we mark the packet with FA58, that means it gets masqueraded before
    // leaving, which may be useful to work around some network configuratios.
    retval.append("iptables -t mangle %s FASTFORWARD -p %s --dport %s "
	          "-j MARK --set-mark %s %s %s\n", cmd, proto, src.port,
		  snat ? "0xFA58" : "0xFA57", haveiface, shutup());

    // Don't open the port completely; just open it for the forwarded packets
    retval.append("iptables %s FFASTFORWARD -j ACCEPT -p %s "
		  "--dport %s -m mark --mark %s %s %s\n", cmd, proto, dst.port,
		  snat ? "0xFA58" : "0xFA57", haveoface, shutup());
    
    return retval;
}

WvString WvIPFirewall::redir_port_range_command(const char *cmd,
    	const WvIPPortAddr &src_min, const WvIPPortAddr &src_max, int dstport)
{
    WvIPAddr ad(src_min), none;
    
    return WvString("iptables -t nat %s TProxy "
		    "-p tcp %s --dport %s:%s "
		    "-j REDIRECT --to-ports %s "
		    "%s",
		    cmd,
		    ad == none ? WvString("") : WvString("-d %s", ad),
		    src_min.port == 0? WvString(""): WvString(src_min.port),
		    src_max.port == 0? WvString(""): WvString(src_max.port),
		    dstport,
		    shutup());
}

WvString WvIPFirewall::redir_all_command(const char *cmd, int dstport)
{
    return WvString("iptables -t nat %s TProxy "
		    "-p tcp "
		    "-j REDIRECT --to-ports %s "
		    "%s",
		    cmd,
		    dstport,
		    shutup());
}

WvString WvIPFirewall::proto_command(const char *cmd, const char *proto)
{
    return WvString("iptables %s Services -p %s -j ACCEPT "
		    "%s",
		    cmd, proto, shutup());
}


void WvIPFirewall::add_port(const WvIPPortAddr &addr)
{
    addrs.append(new WvIPPortAddr(addr), true);
    WvString s(port_command("-A", "tcp", addr)),
    	    s2(port_command("-A", "udp", addr));
    if (enable)
    {
	system(s);
	system(s2);
    }
}


// note!  This does not remove the address from the list, only the kernel!
void WvIPFirewall::del_port(const WvIPPortAddr &addr)
{
    WvIPPortAddrList::Iter i(addrs);
    for (i.rewind(); i.next(); )
    {
	if (*i == addr)
	{
	    WvString s(port_command("-D", "tcp", addr)),
		    s2(port_command("-D", "udp", addr));
	    if (enable)
	    {
		system(s);
		system(s2);
	    }
	    return;
	}
    }
}

void WvIPFirewall::add_forward(const WvIPPortAddr &src,
			       const WvIPPortAddr &dst, bool snat)
{
    ffwds.append(new FFwd(src, dst, snat), true);
    WvString s(forward_command("-A", "tcp", src, dst, snat)),
    	    s2(forward_command("-A", "udp", src, dst, snat));

    log("Add Forwards (%s):\n%s\n%s\n", enable, s, s2);
    
    if (enable)
    {
	system(s);
	system(s2);
    }
}

void WvIPFirewall::del_forward(const WvIPPortAddr &src,
			       const WvIPPortAddr &dst, bool snat)
{
    FFwdList::Iter i(ffwds);
    for (i.rewind(); i.next();) 
    {
        if (i->src == src && i->dst == dst && i->snat == snat) 
        {
            WvString s(forward_command("-D", "tcp", src, dst, snat)),
	            s2(forward_command("-D", "udp", src, dst, snat));

            log("Delete Forward (%s):\n%s\n%s\n", enable, s, s2);
    
            if (enable) 
            {
	        system(s);
                system(s2);
            }
        }
    }
}

void WvIPFirewall::add_redir(const WvIPPortAddr &src, int dstport)
{
    redirs.append(new Redir(src, dstport), true);
    WvString s(redir_command("-A", src, dstport));
    if (enable) system(s);
}


void WvIPFirewall::del_redir(const WvIPPortAddr &src, int dstport)
{
    RedirList::Iter i(redirs);
    for (i.rewind(); i.next(); )
    {
	if (i->src == src && i->dstport == dstport)
	{
	    WvString s(redir_command("-D", src, dstport));
	    if (enable) system(s);
	    return;
	}
    }
}

void WvIPFirewall::add_redir_all(int dstport)
{
    redir_alls.append(new RedirAll(dstport), true);
    WvString s(redir_all_command("-A", dstport));
    if (enable) system(s);
}


void WvIPFirewall::del_redir_all(int dstport)
{
    RedirAllList::Iter i(redir_alls);
    for (i.rewind(); i.next(); )
    {
	if (i->dstport == dstport)
	{
	    WvString s(redir_all_command("-D", dstport));
	    if (enable) system(s);
	    return;
	}
    }
}

void WvIPFirewall::add_redir_port_range(const WvIPPortAddr &src_min,
    	const WvIPPortAddr &src_max, int dstport)
{
    redir_port_ranges.append(new RedirPortRange(src_min, src_max, dstport), true);
    WvString s(redir_port_range_command("-A", src_min, src_max, dstport));
    if (enable) system(s);
}


void WvIPFirewall::del_redir_port_range(const WvIPPortAddr &src_min,
    	const WvIPPortAddr &src_max, int dstport)
{
    RedirPortRangeList::Iter i(redir_port_ranges);
    for (i.rewind(); i.next(); )
    {
	if (i->src_min == src_min && i->src_max == src_max
	    	&& i->dstport == dstport)
	{
	    WvString s(redir_port_range_command("-D", src_min, src_max, dstport));
	    if (enable) system(s);
	    return;
	}
    }
}


void WvIPFirewall::add_proto(WvStringParm proto)
{
    protos.append(new WvString(proto), true);
    WvString s(proto_command("-A", proto));
    if (enable) system(s);
}


void WvIPFirewall::del_proto(WvStringParm proto)
{
    WvStringList::Iter i(protos);
    for (i.rewind(); i.next(); )
    {
	if (*i == proto)
	{
	    WvString s(proto_command("-D", proto));
	    if (enable) system(s);
	    return;
	}
    }
}


// clear out our portion of the firewall
void WvIPFirewall::zap()
{
    WvIPPortAddrList::Iter i(addrs);
    for (i.rewind(); i.next(); )
    {
	del_port(*i);
	i.xunlink();
    }

    FFwdList::Iter ifwd(ffwds);
    for (ifwd.rewind(); ifwd.next();)
    {
        del_forward(ifwd->src, ifwd->dst, ifwd->snat);
        ifwd.xunlink();
    }
    
    RedirList::Iter i2(redirs);
    for (i2.rewind(); i2.next(); )
    {
	del_redir(i2->src, i2->dstport);
	i2.xunlink();
    }
    
    RedirAllList::Iter i2_5(redir_alls);
    for (i2_5.rewind(); i2_5.next(); )
    {
	del_redir_all(i2_5->dstport);
	i2_5.xunlink();
    }
    
    RedirPortRangeList::Iter port_range(redir_port_ranges);
    for (port_range.rewind(); port_range.next(); )
    {
	del_redir_port_range(port_range->src_min, port_range->src_max,
	    	port_range->dstport);
	port_range.xunlink();
    }
    
    WvStringList::Iter i3(protos);
    for (i3.rewind(); i3.next(); )
    {
        del_proto(*i3);
        i3.xunlink();
    }
}
