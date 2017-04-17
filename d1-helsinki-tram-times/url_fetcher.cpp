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
UrlFetcher::UrlFetcher(char *url)
{
    m_url = strdup(url);
}

/*
 * Constructor.  Called with the URL to fetch.
 */
UrlFetcher::UrlFetcher(String url)
{
    m_url = strdup(url.c_str());
}

UrlFetcher::~UrlFetcher()
{
    if (m_url)
    {
        free(m_url);
        m_url = NULL;
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

int UrlFetcher::port()
{
    if (is_secure())
        return 443;
    else
        return 80;
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


    /*
     * If the user has set a user-agent then use it
     * if not build one based on the MAC address of the
     * board.
     */
    char ua[128];
    memset(ua, '\0', sizeof(ua));

    if (m_user_agent)
    {
        snprintf(ua, sizeof(ua) - 1, "User-Agent: %s", m_user_agent);
    }
    else
    {
        //
        // User-Agent will have the MAC address in it, for
        // identification-purposes
        //
        uint8_t mac_array[6];
        WiFi.macAddress(mac_array);

        snprintf(ua, sizeof(ua) - 1, "User-Agent: arduino-%02X:%02X:%02X:%02X:%02X:%02X/1.0",
                 mac_array[0],
                 mac_array[1],
                 mac_array[2],
                 mac_array[3],
                 mac_array[4],
                 mac_array[5]
                );
    }


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
        m_client->println(ua);
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
