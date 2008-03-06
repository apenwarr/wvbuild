#include "wvtest.h"
#include "wvdigest.h"
#include "wvhex.h"

WVTEST_MAIN("MD5 Test")
{
    WvMD5Digest md5;
    WvDynBuf inbuf, md5buf;
    inbuf.put("floogle", 7);
    md5.encode(inbuf, md5buf);
    md5.finish(md5buf);

    WvString md5str = WvHexEncoder().strflushbuf(md5buf, true);

    WVPASS(md5str == "85c42eea90a6586f2b75be98e15e9a1f");
}

WVTEST_MAIN("SHA-1 Test")
{
    WvSHA1Digest sha1;
    WvDynBuf inbuf, sha1buf;
    inbuf.put("floogle", 7);
    sha1.encode(inbuf, sha1buf);
    sha1.finish(sha1buf);

    WvString sha1str = WvHexEncoder().strflushbuf(sha1buf, true);

    WVPASS(sha1str == "9ec117f204872bdd467b6c337ba1cf78be0ad5d9");
}

/*
 * HMACs leak memory
 *
WVTEST_MAIN("HMAC Test")
{
    WvSHA1Digest *sha1 = new WvSHA1Digest();
    WvDynBuf inbuf, hmacbuf;
    WvHMACDigest hmac(sha1, "imakey", 6);
    inbuf.put("floogle", 7);
    hmac.encode(inbuf, hmacbuf);
    hmac.finish(hmacbuf);

    WvString hmacstr = WvHexEncoder().strflushbuf(hmacbuf, true);

    fprintf(stderr, "%s\n", hmacstr.cstr());
    WVPASS(hmacstr == "63d79e4bdaedf6c39564f2c9251c5edacbeabd30");
    delete sha1;
}
*/

WVTEST_MAIN("CRC32 Test")
{
    WvCrc32Digest crc32;
    WvDynBuf inbuf, crc32buf;
    inbuf.put("floogle", 7);
    crc32.encode(inbuf, crc32buf);
    crc32.finish(crc32buf);

    WvString crc32str = WvHexEncoder().strflushbuf(crc32buf, true);

    WVPASSEQ(crc32str, "cb130290");

    inbuf.put("hoopy frood", 11);
    crc32.reset();
    crc32.encode(inbuf, crc32buf);
    crc32.finish(crc32buf);

    crc32str = WvHexEncoder().strflushbuf(crc32buf, true);

    WVPASSEQ(crc32str, "ccdf6289");
}


WVTEST_MAIN("Adler32 Test")
{
    // Example taken from Adler32 wikipedia entry.
    WvAdler32Digest adler32;
    WvDynBuf inbuf, adler32buf;
    inbuf.put("Wikipedia", 9);
    adler32.encode(inbuf, adler32buf);
    adler32.finish(adler32buf);

    WvString adler32str = WvHexEncoder().strflushbuf(adler32buf, true);

    WVPASSEQ(adler32str, "11e60398");
}
