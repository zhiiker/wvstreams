/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * A fast, easy-to-use, parallelizing, pipelining HTTP/1.1 file retriever.
 * 
 * See wvhttppool.h.
 */
#include "wvhttppool.h"
#include "wvtcp.h"
#include "wvsslstream.h"
#include "base64.h"
#include "strutils.h"

WvHttpStream::WvHttpStream(const WvIPPortAddr &_remaddr, WvStringParm _username,
                bool _ssl, WvIPPortAddrTable &_pipeline_incompatible)
    : WvUrlStream(_remaddr, _username, WvString("HTTP %s", _remaddr)),
      pipeline_incompatible(_pipeline_incompatible)
{
    log("Opening server connection.\n");
    http_response = "";
    encoding = Unknown;
    remaining = 0;
    in_chunk_trailer = false;
    pipeline_test_count = 0;
    last_was_pipeline_test = false;

    enable_pipelining = global_enable_pipelining 
        && !pipeline_incompatible[target.remaddr];
    ssl = _ssl;

    if (ssl)
        clone = new WvSSLStream(clone);

    sent_url_request = false;

    alarm(60000); // timeout if no connection, or something goes wrong
}


WvHttpStream::~WvHttpStream()
{
    log(WvLog::Debug2, "Deleting.\n");
    if (geterr())
        log("Error was: %s\n", errstr());
    close();
}


void WvHttpStream::close()
{
    // assume pipelining is broken if we're closing without doing at least
    // one successful pipelining test and a following non-test request.
    if (enable_pipelining && max_requests > 1
            && (pipeline_test_count < 1
            || (pipeline_test_count == 1 && last_was_pipeline_test)))
        pipelining_is_broken(2);

    if (isok())
        log("Closing.\n");
    WvStreamClone::close();

    if (geterr())
    {
        // if there was an error, count the first URL as done.  This prevents
        // retrying indefinitely.
        WvUrlRequest *msgurl = curl;
        if (!msgurl && !urls.isempty())
            msgurl = urls.first();
        if (!msgurl && !waiting_urls.isempty())
            msgurl = waiting_urls.first();
        if (msgurl)
            log("URL '%s' is FAILED\n", msgurl->url);
    }
    waiting_urls.zap();
    if (curl)
        doneurl();
}


void WvHttpStream::doneurl()
{
    assert(curl != NULL);
    log("Done URL: %s\n", curl->url);

    http_response = "";
    encoding = Unknown;
    in_chunk_trailer = false;
    remaining = 0;

    last_was_pipeline_test = curl->pipeline_test;
    bool broken = false;
    if (last_was_pipeline_test)
    {
        pipeline_test_count++;
        if (pipeline_test_count == 1)
            start_pipeline_test(&curl->url);
        else if (pipeline_test_response != http_response)
        {
            // getting a bit late in the game to be detecting brokenness :(
            // However, if the response code isn't the same for both tests,
            // something's definitely screwy.
            pipelining_is_broken(4);
            broken = true;
        }
        pipeline_test_response = http_response;
    }

    assert(curl == urls.first());
    curl->done();
    curl = NULL;
    sent_url_request = false;
    urls.unlink_first();

    if (broken)
        close();

    request_next();
}


static WvString encode64(WvStringParm user, WvStringParm password)
{
    WvString s("%s:%s", user, password);
    return base64_encode(s, s.len());
}


static WvString fixnl(WvStringParm nonl)
{
    WvDynBuf b;
    const char *cptr;

    for (cptr = nonl; cptr && *cptr; cptr++)
    {
        if (*cptr == '\r')
            continue;
        else if (*cptr == '\n')
            b.put("\r", 1); // put BOTH \r and \n
        b.put(cptr, 1);
    }

    return b.getstr();
}


WvString WvHttpStream::request_str(WvUrlRequest *url, bool keepalive)
{
    WvString request;
    WvString auth(""), content = putstream_data.getstr();
    if(!!url->url.getuser() && !!url->url.getpassword())
        auth = WvString("Authorization: Basic %s\n",
                    encode64(url->url.getuser(), url->url.getpassword()));

    request = fixnl(WvString("%s %s HTTP/1.1\n"
                "Host: %s:%s\n"
                "Connection: %s\n"
                "%s"
                "%s"
                "%s%s"
                "\n"
                "%s",
                url->method,
                url->url.getfile(),
                url->url.gethost(), url->url.getport(),
                keepalive ? "keep-alive" : "close",
                auth,
                (content.len() > 0 ? WvString("Content-Length: %s\n", content.len()).cstr() : ""),
                trim_string(url->headers.edit()),
                !!url->headers ? "\n" : "",
                (content.len() > 0 ? content.cstr() : ""))
            );
    return request;
}


void WvHttpStream::send_request(WvUrlRequest *url)
{
    request_count++;
    log("Request #%s: %s\n", request_count, url->url);
    write(request_str(url, url->pipeline_test
                || request_count < max_requests));
    sent_url_request = true;
    alarm(60000);
}


void WvHttpStream::start_pipeline_test(WvUrl *url)
{
    WvUrl location(WvString(
                "%s://%s:%s/wvhttp-pipeline-check-should-not-exist/",
                url->getproto(), url->gethost(), url->getport()));
    WvUrlRequest *testurl = new WvUrlRequest(location, "HEAD", "", NULL,
                                             false, true);
    testurl->instream = this;
    send_request(testurl);
    urls.append(testurl, true, "sent_running_url");
}


void WvHttpStream::request_next()
{
    // Clear the putstream buffer before we start any new requests
    putstream_data.zap();

    // don't do a request if we've done too many already or we have none
    // waiting.
    if (request_count >= max_requests || waiting_urls.isempty())
        return;

    // don't do more than one request at a time if we're not pipelining.
    if (!enable_pipelining && !urls.isempty())
        return;

    // okay then, we really do want to send a new request.
    WvUrlRequest *url = waiting_urls.first();

    waiting_urls.unlink_first();
    if (!url->putstream)
    {
        if (enable_pipelining && !request_count && max_requests > 1)
            start_pipeline_test(&url->url);
        send_request(url);
    }
    urls.append(url, false, "sent_running_url");
}


void WvHttpStream::pipelining_is_broken(int why)
{
    if (!pipeline_incompatible[target.remaddr])
    {
        pipeline_incompatible.add(new WvIPPortAddr(target.remaddr), true);
        log("Pipelining is broken on this server (%s)!  Disabling.\n", why);
    }
}


bool WvHttpStream::pre_select(SelectInfo &si)
{
    SelectRequest oldwant = si.wants;
    WvUrlRequest *url;

    if (WvUrlStream::pre_select(si))
        return true;

    if (!urls.isempty())
    {
        url = urls.first();
        if(url && url->putstream && url->putstream->pre_select(si))
            return true;
    }
   
    si.wants = oldwant;
    return false;
}


bool WvHttpStream::post_select(SelectInfo &si)
{
    SelectRequest oldwant = si.wants;
    WvUrlRequest *url;

    if (WvUrlStream::post_select(si))
        return true;

    if (!urls.isempty())
    {
        url = urls.first();
        if(url && url->putstream && url->putstream->post_select(si))
            return true;
    }

    si.wants = oldwant;
    return false;
}


void WvHttpStream::execute()
{
    char buf[1024], *line;
    size_t len;

    WvStreamClone::execute();

    // make connections timeout after some idleness
    if (alarm_was_ticking)
    {
        log(WvLog::Debug4, "urls count: %s\n", urls.count());
        if (!urls.isempty())
        {
            seterr(ETIMEDOUT);

            WvUrlRequest *url = urls.first();
            if (url->outstream)
                url->outstream->seterr(ETIMEDOUT);
        }
        else
            close(); // timed out, but not really an error
        return;
    }

    // Die if somebody closed our outstream.  This is so that if we were
    // downloading a really big file, they can stop it in the middle and
    // our next url request can start downloading immediately.
    if (curl && !curl->outstream)
    {
        // don't complain about pipelining failures
        pipeline_test_count++;
        last_was_pipeline_test = false;

        close();
        if (curl)
            doneurl();
        return;
    }
    else if (curl)
        curl->inuse = true;

    if(!sent_url_request && !urls.isempty())
    {
        WvUrlRequest *url = urls.first();
        if(url)
        {
            if(url->putstream)
            {
                int len = 0;
                if(url->putstream->isok())
		{
		    unsigned char *buf = putstream_data.alloc(1024);
                    len = url->putstream->read(buf, 1024);
		    putstream_data.unalloc(1024-len);
		}

                if(!url->putstream->isok() || len == 0)
                {
                    url->putstream = NULL;
                    send_request(url);
                }
            }
        }
    }

    if (!curl)
    {
        // in the header section
        line = getline(0);
        if (line)
        {
            line = trim_string(line);
            log(WvLog::Debug4, "Header: '%s'\n", line);
            if (!http_response)
            {
                http_response = line;

                // there are never two pipeline test requests in a row, so
                // a second response string exactly like the pipeline test
                // response implies that everything between the first and
                // second test requests was lost: bad!
                if (last_was_pipeline_test
                        && http_response == pipeline_test_response)
                {
                    pipelining_is_broken(1);
                    close();
                    return;
                }

                // http response #400 is "invalid request", which we
                // shouldn't be sending. If we get one of these right after
                // a test, it probably means the stuff that came after it
                // was mangled in some way during transmission ...and we
                // should throw it away.
                if (last_was_pipeline_test && !!http_response)
                {
                    const char *cptr = strchr(http_response, ' ');
                    if (cptr && atoi(cptr+1) == 400)
                    {
                        pipelining_is_broken(3);
                        close();
                        return;
                    }
                }
            }

            if (urls.isempty())
            {
                log("got unsolicited data.\n");
                seterr("unsolicited data from server!");
                return;
            }

            if (!strncasecmp(line, "Content-length: ", 16))
            {
                remaining = atoi(line+16);
                encoding = ContentLength;
            }
            else if (!strncasecmp(line, "Transfer-Encoding: ", 19)
                    && strstr(line+19, "chunked"))
            {
                encoding = Chunked;
            }

            if (line[0])
            {
                char *p;
                WvBufUrlStream *outstream = urls.first()->outstream;

                if ((p = strchr(line, ':')) != NULL)
                {
                    *p = 0;
                    p = trim_string(p+1);
                    struct WvHTTPHeader *h = new struct WvHTTPHeader(line, p);
                    if (outstream)
                        outstream->headers.add(h, true);
                }
                else if (strncasecmp(line, "HTTP/", 5) == 0)
                {
                    char *p = strchr(line, ' ');
                    if (p)
                    {
                        *p = 0;
                        if (outstream)
                        {
                            outstream->version = line+5;
                            outstream->status = atoi(p+1);
                        }
                    }
                }
            }
            else
            {
                // blank line is the beginning of data section
                curl = urls.first();
                in_chunk_trailer = false;
                log(WvLog::Debug4,
                        "Starting data: %s (enc=%s)\n", remaining, encoding);

                if (encoding == Unknown)
                    encoding = Infinity; // go until connection closes itself

                if (curl->method == "HEAD")
                {
                    log("Got all headers.\n");
                    //		    getline(0);
                    doneurl();
                }
            }
        }
    }
    else if (encoding == Chunked && !remaining)
    {
        line = getline(0);
        if (line)
        {
            line = trim_string(line);

            if (in_chunk_trailer)
            {
                // in the trailer section of a chunked encoding
                log(WvLog::Debug4, "Trailer: '%s'\n", line);

                // a blank line means we're finally done!
                if (!line[0])
                    doneurl();
            }
            else
            {
                // in the "length line" section of a chunked encoding
                if (line[0])
                {
                    remaining = (size_t)strtoul(line, NULL, 16);
                    if (!remaining)
                        in_chunk_trailer = true;
                    log(WvLog::Debug4, "Chunk length is %s ('%s').\n",
                            remaining, line);
                }
            }
        }
    }
    else if (encoding == Infinity)
    {
        // just read data until the connection closes, and assume all was
        // well.  It sucks, but there's no way to tell if all the data arrived
        // okay... that's why Chunked or ContentLength encoding is better.
        len = read(buf, sizeof(buf));
        if (len)
            log(WvLog::Debug5, "Infinity: read %s bytes.\n", len);
        if (curl->outstream)
            curl->outstream->write(buf, len);

        if (!isok())
            doneurl();
    }
    else // not chunked or currently in a chunk - read 'remaining' bytes.
    {
        // in the data section of a chunked or content-length encoding,
        // with 'remaining' bytes of data left.

        if (remaining > sizeof(buf))
            len = read(buf, sizeof(buf));
        else
            len = read(buf, remaining);
        remaining -= len;
        if (len)
            log(WvLog::Debug5, 
                    "Read %s bytes (%s bytes left).\n", len, remaining);
        if (curl->outstream)
            curl->outstream->write(buf, len);

        if (!remaining && encoding == ContentLength)
            doneurl();
    }

    if (urls.isempty())
        alarm(5000); // just wait a few seconds before closing connection
    else
        alarm(60000); // give the server a minute to respond, if we're waiting
}