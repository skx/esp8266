//
// Basic types
//
#include <Arduino.h>

//
// Headers for SSL/MAC access.
//
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

//
// Our header.
//
#include "url_fetcher.h"


/*
 * Constructor.  Called with the URL to fetch.
 */
UrlFetcher::UrlFetcher(const char *url)
{
    m_url = strdup(url);
}

/*
 * Destructor.
 *
 * Free any strings we've copied.
 */
UrlFetcher::~UrlFetcher()
{
    if (m_url)
    {
        free(m_url);
        m_url = NULL;
    }

    if (m_status)
    {
        free(m_status);
        m_status = NULL;
    }

    if (m_user_agent)
    {
        free(m_user_agent);
        m_user_agent = NULL;
    }

    if (m_client)
    {
        delete(m_client);
        m_client = NULL;
    }
}

/*
 * Get the hostname from the URL.
 */
char *UrlFetcher::getHost()
{
    if (strlen(m_host) < 1)
        parse();

    return (m_host);
}

/*
 * Get the request-path from the URL.
 */
char *UrlFetcher::getPath()
{
    if (strlen(m_host) < 1)
        parse();

    return (m_path);
}

/*
 * Return the port we'll connect to, 80 for HTTP, 443 for HTTPS.
 */
int UrlFetcher::port()
{
    if (is_secure())
        return 443;
    else
        return 80;
}

/*
 * Get the user-agent, if one hasn't been set it will be created
 * based upon the MAC-address of the board.
 */
const char *UrlFetcher::getAgent()
{
    if (m_user_agent == NULL)
    {
        //
        // User-Agent will have the MAC address in it, for
        // identification-purposes
        //
        uint8_t mac_array[6];
        WiFi.macAddress(mac_array);

        char tmp[128];
        snprintf(tmp, sizeof(tmp) - 1, "arduino-%02X:%02X:%02X:%02X:%02X:%02X/1.0",
                 mac_array[0],
                 mac_array[1],
                 mac_array[2],
                 mac_array[3],
                 mac_array[4],
                 mac_array[5]
                );
        m_user_agent = strdup(tmp);
    }

    return (m_user_agent);
}

/*
 * Set the user-agent, if any.
 */
void UrlFetcher::setAgent(const char *userAgent)
{
    if (m_user_agent)
        free(m_user_agent);

    m_user_agent = strdup(userAgent);
}

/*
 * Return the body-contents of the remote URL.
 *
 * We only fetch it once regardless of how many times it is invoked.
 */
String UrlFetcher::body()
{
    if (! m_fetched)
    {
        fetch();
        m_fetched = true;
    }

    return (m_body);
}

/*
 * Return the headers of the remote URL.
 *
 * We only fetch it once regardless of how many times it is invoked.
 */
String UrlFetcher::headers()
{
    if (! m_fetched)
    {
        fetch();
        m_fetched = true;
    }

    return (m_headers);
}


/*
 * Return the HTTP-status-code of our fetch.
 */
int UrlFetcher::code()
{
    //
    // Ensure that `m_headers` is populated.
    //
    if (! m_fetched)
    {
        fetch();
        m_fetched = true;
    }

    //
    // Make sure we have something useful.
    //
    const char *response = m_headers.c_str();

    if (response == NULL)
        return -1;

    //
    // Too short?
    //
    size_t len = strlen(response);

    if (len < 10)
        return -2;

    //
    // Response will start "HTTP/X.X CODE $MSG\n"
    //
    // Find the start & end of the CODE.
    //
    int start = 0;
    int end   = 0;

    //
    // Find the first space
    //
    int c = 0;

    while (c < len && (response[c] != ' '))
    {
        c += 1;
    }

    //
    // Outside our bounds?  That's an error.
    //
    if (c >= len)
        return -3;

    //
    // Bump past the space, and record it as the first digit.
    //
    c++;
    start = c;

    //
    // Look for the separating space between the code and
    // the explanation
    //
    while (c < len && (response[c] != ' '))
    {
        c += 1;
    }

    // Outside our length?
    if (c >= len)
        return -4;

    //
    // OK we have the start + end position of our status-code
    //
    end = c;

    //
    // Create that as a string.
    //
    char tmp[10];
    memset(tmp, '\0', sizeof(tmp));
    strncpy(tmp, response + start, end - start);

    //
    // Now convert to an integer.
    //
    return (atoi(tmp));
}

/*
 * Return the complete status-line of the remote server
 */
char *UrlFetcher::status()
{
    if (m_status == NULL)
    {
        //
        // Ensure that `m_headers` is populated.
        //
        if (! m_fetched)
        {
            fetch();
            m_fetched = true;
        }

        //
        // Get the response-headers, which will have the
        // status-line in the first .. line.
        //
        const char *response = m_headers.c_str();

        //
        // Find the first newline.
        //
        char *end = strchr(response, '\n');

        //
        // Allocate memory for a copy of that first line.
        //
        m_status = (char *)malloc(end - response + 1);
        memset(m_status, '\0', end - response + 1);

        //
        // Copy it into place.
        //
        strncpy(m_status, response, end - response);
    }

    // Return our copied memory.
    return (m_status);
}

/*
 * Fetch the contents of the remote URL.
 */
void UrlFetcher::fetch()
{
    /*
     * If we've not already parsed into Host + Path, do so.
     */
    if (strlen(m_host) < 1)
        parse();

    /*
     * Create the appropriate client-object.
     */
    if (is_secure())
        m_client = new WiFiClientSecure();
    else
        m_client = new WiFiClient;


    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    bool gotResponse = false;
    long now;

    /*
     * Remove any old state, if present.
     */
    m_headers = "";
    m_body = "";

    if (m_client->connect(m_host, port()))
    {
        m_client->print("GET ");
        m_client->print(m_path);
        m_client->println(" HTTP/1.0");
        m_client->print("Host: ");
        m_client->println(m_host);
        m_client->print("User-Agent: ");
        m_client->println(getAgent());
        m_client->println("");

        now = millis();

        // checking the timeout
        while (millis() - now < 5000)
        {
            // Read a single character
            while (m_client->available())
            {
                char c = m_client->read();

                if (finishedHeaders)
                {
                    m_body = m_body + c;
                }
                else
                {
                    if (currentLineIsBlank && c == '\n')
                        finishedHeaders = true;
                    else
                        m_headers = m_headers + c;
                }

                if (c == '\n')
                    currentLineIsBlank = true;
                else if (c != '\r')
                    currentLineIsBlank = false;

                //marking we got a response
                gotResponse = true;

            }

            if (gotResponse)
                break;
        }
    }

}


/*
 * Parse the URL into "host" + "path".
 */
void UrlFetcher::parse()
{
    /*
     * Look for the host.
     */
    char *host_start = strstr(m_url, "://");

    if (host_start != NULL)
    {
        /*
         * Look for the end.
         */
        host_start += 3;
        char *host_end = host_start;

        while (host_end[0] != '/')
            host_end += 1;

        strncpy(m_host, host_start, host_end - host_start);
        strcpy(m_path, host_end);
    }

}


/*
 * Is the URL secure?
 */
bool UrlFetcher::is_secure()
{
    return (strncmp(m_url, "https://", 7) == 0);
}
