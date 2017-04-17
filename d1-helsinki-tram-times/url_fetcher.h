#ifndef URL_FETCHER_H
#define URL_FETCHER_H

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
    String fetch();

    /*
     * Is the URL SSL-based?
     */
    bool is_secure();

private:

    /*
     * Parse URL into "host" + "path"
     */
    void parse();

    String secure_fetch();
    String http_fetch();

    char *m_url;

    char m_host[128] = { '\0' };
    char m_path[128] = { '\0' };
};

#endif
