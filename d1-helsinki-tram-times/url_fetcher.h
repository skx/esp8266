#ifndef URL_FETCHER_H
#define URL_FETCHER_H

/*
 * This is a simple class which can be used to make HTTP or HTTPS fetches.
 *
 * Usage is as simple as:
 *
 *   UrlFetcher foo( "http://steve.fi/robots.txt" );
 *   String body = foo.fetch()
 *
 */
class UrlFetcher
{
public:
    /*
     * Constructor.
     */
    UrlFetcher(char *url);

    /*
     * Constructor.
     */
    UrlFetcher(String url);

    /*
     * Destructor
     */
    ~UrlFetcher();

    /*
     * Get the host of the URL.
     */
    char *getHost();

    /*
     * Get the path of the URL.
     */
    char *getPath();

    /*
     * Get the contents of the URL.
     */
    String fetch(long timeout = 5000);

    /*
     * Is the URL SSL-based?
     */
    bool is_secure();

    /*
     * The port to use - 80 for HTTP, 443 for SSL
     */
    int port();

    /*
     * Set the user-agent, if any.
     */
    void setAgent(const char *userAgent);

private:

    /*
     * Parse URL into "host" + "path"
     */
    void parse();

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
     * The user-agent the user has set, if any.
     */
    char *m_user_agent = NULL;

    /*
     * The client we use for fetching.
     *
     * NOTE: This might be the derived class `WiFiClientSecure`
     *
     */
    WiFiClient *m_client;
};

#endif
