#include "wvgzip.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "wvxor.h"

extern char *optarg;

void usage(const char *prog)
{
    fprintf(stderr, 
	    "Usage: %s <-z|-Z|-x ##> [-f]\n"
	    "    Encode data from stdin to stdout.\n"
	    "        -z: use libz-style encoder (not gzip compatible)\n"
	    "        -Z: use libz-style decoder\n"
	    "        -x: amazing XOR encryption\n"
	    "        -x: amazing XOR decryption\n"
	    "        -f: flush output stream often\n",
	    prog);
}

int main(int argc, char **argv)
{
    WvEncoder *enc = NULL;
    char buf[2048];
    enum { NoMode, Gzip, Gunzip, XOR } mode = NoMode;
    bool flush_often = false;
    int c;
    const char *xor_key = NULL;
    
    while ((c = getopt(argc, argv, "zZx:f?")) >= 0)
    {
	switch (c)
	{
	case 'z':
	    mode = Gzip;
	    break;
	    
	case 'Z':
	    mode = Gunzip;
	    break;
	    
	case 'x':
	    mode = XOR;
	    xor_key = optarg;
	    break;
	    
	case 'f':
	    flush_often = true;
	    break;
	    
	case '?':
	default:
	    usage(argv[0]);
	    return 1;
	}
    }
    
    switch (mode)
    {
    case Gzip:
	enc = new WvGzipEncoder(WvGzipEncoder::Deflate);
	break;
    case Gunzip:
	enc = new WvGzipEncoder(WvGzipEncoder::Inflate);
	break;
    case XOR:
	enc = new WvXOREncoder(xor_key, strlen(xor_key));
	break;
	
    case NoMode:
    default:
	usage(argv[0]);
	return 2;
    }
    
    assert(enc);
    
    WvDynBuf inbuf;
    WvDynBuf outbuf;
    
    while (enc->isok())
    {
        size_t rlen = read(0, buf, sizeof(buf));
        //fprintf(stderr, "[read %d bytes]\n", rlen);
        if (rlen > 0)
        {
            inbuf.put(buf, rlen);
            enc->encode(inbuf, outbuf, flush_often);
        }
        else
        {
            enc->flush(inbuf, outbuf, true);
            enc->finish(outbuf);
        }
        
	size_t wlen = outbuf.used();
	write(1, outbuf.get(wlen), wlen);
	//fprintf(stderr, "[wrote %d bytes]\n", wlen);
	
	if (rlen <= 0)
	    break;
    }
    
    fprintf(stderr, "exiting...\n");
    
    if (!enc->isok())
	fprintf(stderr, "encoder is not okay! %s\n",
            enc->geterror().cstr());
    
    delete enc;
    
    return 0;
}
