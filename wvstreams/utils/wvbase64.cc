/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * Functions for encoding and decoding strings in MIME's Base64 notation.
 *
 * Base 64 is pretty easy.  The input is processed in groups of three bytes.
 * These 24 bits are split into 4 groups of 6 bits.  Each group of six bits
 * is represented by one character in the base64 alphabet, in the encoded
 * output.  The alphabet is as follows:
 * 	ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=
 * Where 'A' through '/' represent 000000 through 011111, the 64 different
 * combinations.  The '=' (100000) is padding and has no value when decoded.
 */
#include "wvbase64.h"

// maps codes to the Base64 alphabet
static char alphabet[67] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n";

// finds codes in the Base64 alphabet
static int lookup(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9')
        return ch - '0' + 52;
    if (ch == '+')
        return 62;
    if (ch == '/')
        return 63;
    if (ch == '=')
        return 64; // padding
    if (ch == '\n' || ch == ' ' || ch == '\r' || ch == '\t' ||
        ch == '\f' || ch == '\v')
        return 65; // whitespace
    return -1;
}


/***** WvBase64Encoder *****/

WvBase64Encoder::WvBase64Encoder()
{
    _reset();
}


bool WvBase64Encoder::_reset()
{
    state = ATBIT0;
    bits = 0;
    return true;
}


bool WvBase64Encoder::_encode(WvBuf &in, WvBuf &out, bool flush)
{
    // base 64 encode the entire buffer
    while (in.used() != 0)
    {
        unsigned char next = in.getch();
        bits = (bits << 8) | next;
        switch (state)
        {
            case ATBIT0:
                out.putch(alphabet[bits >> 2]);
                bits &= 0x03;
                state = ATBIT2;
                break;
            case ATBIT2:
                out.putch(alphabet[bits >> 4]);
                bits &= 0x0f;
                state = ATBIT4;
                break;
            case ATBIT4:
                out.putch(alphabet[bits >> 6]);
                out.putch(alphabet[bits & 0x3f]);
                bits = 0;
                state = ATBIT0;
                break;
        }
    }
    // do not consider the data flushed if we need padding
    if (flush && state != ATBIT0)
        return false;
    return true;
}


bool WvBase64Encoder::_finish(WvBuf &out)
{
    // pad text if needed
    switch (state)
    {
        case ATBIT2:
            out.putch(alphabet[bits << 4]);
            out.putch('=');
            out.putch('=');
            break;
        case ATBIT4:
            out.putch(alphabet[bits << 2]);
            out.putch('=');
            break;
        case ATBIT0:
            break;
    }
    return true;
}



/***** WvBase64Decoder *****/

WvBase64Decoder::WvBase64Decoder()
{
    _reset();
}


bool WvBase64Decoder::_reset()
{
    state = ATBIT0;
    bits = 0;
    return true;
}


bool WvBase64Decoder::_encode(WvBuf &in, WvBuf &out, bool flush)
{
    // base 64 decode the entire buffer
    while (in.used() != 0)
    {
        unsigned char next = in.getch();
        int symbol = lookup(next);
        switch (symbol)
        {
            case -1: // invalid character
                seterror("invalid character #%s in base64 input", next);
                return false;
                
            case 64: // padding
                // strip out any remaining padding
                // we are lenient in that we do not track how much padding we skip
                setfinished();
                state = PAD;
                break;

            case 65: // whitespace
                break;

            default: // other symbol
                bits = (bits << 6) | symbol;
                switch (state)
                {
                    case ATBIT0:
                        state = ATBIT2;
                        break;
                    case ATBIT2:
                        out.putch(bits >> 4);
                        bits &= 0x0f;
                        state = ATBIT4;
                        break;
                    case ATBIT4:
                        out.putch(bits >> 2);
                        bits &= 0x03;
                        state = ATBIT6;
                        break;
                    case ATBIT6:
                        out.putch(bits);
                        bits = 0;
                        state = ATBIT0;
                        break;
                        
                    case PAD:
                        seterror("invalid character #%s "
                            "after base64 padding", next);
                        return false;
                }
                break;
        }
    }
    // if flushing and we did not get sufficient padding, then fail
    if (flush && (state == ATBIT2 || state == ATBIT4 || state == ATBIT6))
        return false; // insufficient padding to flush!
    return true;
}
