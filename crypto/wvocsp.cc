#include "wvocsp.h"


static const int OCSP_MAX_VALIDITY_PERIOD = (5 * 60); // 5 min: openssl default


WvOCSPReq::WvOCSPReq(const WvX509 &cert, const WvX509 &issuer)
{
    wvssl_init();

    req = OCSP_REQUEST_new();

    // FIXME: follow return values of all these things

    id = OCSP_cert_to_id(NULL, cert.cert, issuer.cert);
    OCSP_request_add0_id(req, id);
}


WvOCSPReq::~WvOCSPReq()
{
    if (req)
        OCSP_REQUEST_free(req);

    wvssl_free();
}


void WvOCSPReq::encode(WvBuf &buf)
{
    BIO *bufbio = BIO_new(BIO_s_mem());
    BUF_MEM *bm;

    if (i2d_OCSP_REQUEST_bio(bufbio, req) <= 0)
    {
        // .. FIXME: handle failure via an error code
    }

    BIO_get_mem_ptr(bufbio, &bm);
    buf.put(bm->data, bm->length);
    BIO_free(bufbio);
}


WvOCSPResp::WvOCSPResp(const WvX509 &_issuer, const WvX509 &_responder) :
    issuer(_issuer),
    responder(_responder),
    resp(NULL),
    log("OCSP Response", WvLog::Debug5)
{
    wvssl_init();
}


WvOCSPResp::~WvOCSPResp()
{
    if (resp)
        OCSP_RESPONSE_free(resp);

    wvssl_free();
}


void WvOCSPResp::decode(WvBuf &encoded)
{
    BIO *membuf = BIO_new(BIO_s_mem());
    BIO_write(membuf, encoded.get(encoded.used()), encoded.used());

    resp = d2i_OCSP_RESPONSE_bio(membuf, NULL);

    BIO_free_all(membuf);
}


WvOCSPResp::Status WvOCSPResp::get_status(const WvOCSPReq &req) const
{
    if (!resp)
        return ERROR;

    int i = OCSP_response_status(resp);
    if (i != OCSP_RESPONSE_STATUS_SUCCESSFUL)
        return ERROR;

    OCSP_BASICRESP * bs = OCSP_response_get1_basic(resp);
    if (!bs)
        return ERROR;

    if ((i = OCSP_check_nonce(req.req, bs)) <= 0)
    {
        if (i == -1)
            log("No nonce in response\n");
        else
            log("Nonce Verify error\n");
        
        OCSP_BASICRESP_free(bs);
        return ERROR;
    }

    STACK_OF(X509) *verify_other = sk_X509_new_null(); assert(verify_other);
    sk_X509_push(verify_other, responder.cert);
    X509_STORE *store = X509_STORE_new(); assert(store);
    X509_STORE_add_cert(store, issuer.cert);    
    i = OCSP_basic_verify(bs, verify_other, store, OCSP_TRUSTOTHER);
    sk_free(verify_other);
    X509_STORE_free(store);

    if (i <= 0)
    {
        log("OCSP Verify Error: %s\n", wvssl_errstr());
        OCSP_BASICRESP_free(bs);
        return ERROR;
    }

    int status, reason;
    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

    if(!OCSP_resp_find_status(bs, req.id, &status, &reason,
                              &rev, &thisupd, &nextupd))
    {
        log("OCSP Find Status Error: %s\n", wvssl_errstr());
        OCSP_BASICRESP_free(bs);
        return ERROR;
    }
    if (!OCSP_check_validity(thisupd, nextupd, OCSP_MAX_VALIDITY_PERIOD, -1))
    {
        log("Error checking for OCSP validity: %s\n", wvssl_errstr());
        OCSP_BASICRESP_free(bs);
        return ERROR;
    }

    OCSP_BASICRESP_free(bs);

    if (status == V_OCSP_CERTSTATUS_GOOD)
        return GOOD;
    else if (status == V_OCSP_CERTSTATUS_REVOKED)
        return REVOKED;

    return UNKNOWN;
}
