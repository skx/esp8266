#ifndef URL_FETCHER_H
#define URL_FETCHER_H

/*
 * This is a simple class which can be used to make HTTP or HTTPS fetches.
 *
 * Usage is as simple as:
 *
 *   UrlFetcher foo( "http://steve.fi/robots.txt" );
 *
 *   String body    = foo.body();
 *   String headers = foo.headers();
 *
 * If you wish you can set a custom user-agent, via:
 *
 *    foo.setAgent( "moi.kissa/3.14" );
 *
 */
class UrlFetcher
{
public:

    /*
     * Constructor.
     */
    UrlFetcher(const char *url);

    /*
     * Destructor
     */
    ~UrlFetcher();


    /*
     * Return the headers of the remote URL.
     *
     * We only fetch it once regardless of how many times it is invoked.
     */
    String headers();


    /*
     * Return the body-contents of the remote URL.
     *
     * We only fetch it once regardless of how many times it is invoked.
     */
    String body();

    /*
     * Return the HTTP status-code of our fetch.
     */
    int code();

    /*
     * Return the complete status-line of the remote server
     */
    char *status();


    /*
     * Get the user-agent, if one hasn't been set it will be created
     * based upon the MAC-address of the board.
     */
    const char *getAgent();


    /*
     * Set the user-agent to the specified string.
     */
    void setAgent(const char *userAgent);


private:

    /*
     * Get the host-part of the URL.
     */
    char *getHost();


    /*
     * Get the path of the URL.
     */
    char *getPath();


    /*
     * Is the URL SSL-based?
     */
    bool is_secure();


    /*
     * Return the port we'll connect to, 80 for HTTP, 443 for HTTPS.
     */
    int port();


    /*
     * Parse URL into "host" + "path"
     */
    void parse();


    /*
     * Perform the fetch of the remote URL, recording
     * the response-headers and the body.
     */
    void fetch();


    /*
     * A copy of the URL we were constructed with.
     */
    char *m_url;

    /*
     * The hostname extracted from the URL.
     */
    char m_host[128] = { '\0' };

    /*
     * The path extracted from the URL.
     */
    char m_path[128] = { '\0' };

    /*
     * The user-agent to use for fetches.
     */
    char *m_user_agent = NULL;

    /*
     * The client we use for fetching.
     *
     * NOTE: This might be the derived class `WiFiClientSecure`
     *
     */
    WiFiClient *m_client;

    /*
     * Have we fetched the URL already?
     */
    bool m_fetched = false;

    /*
     * The headers returned from the remote HTTP-fetch.
     */
    String m_headers;

    /*
     * The status-line.
     */
    char *m_status = NULL;

    /*
     * The body returned from the remote HTTP-fetch.
     */
    String m_body;

};

#endif /* URL_FETCHER_H */
