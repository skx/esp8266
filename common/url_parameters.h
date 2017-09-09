#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * The maximum number of URL parameters we'll handle.
 */
#define MAX_PARAMS 10

/*
 * This structure holds the name & value of a single URL
 * parameter.
 */
struct UrlParam
{
    /**
     * The name of the parameter.
     */
    char *name;

    /**
     * The value of the parameter.
     */
    char *value;
};


/**
 * A simple class to parse out the (GET) parameters from an URL.
 */
class URL
{
public:
    /*
     * Constructor.
     *
     * Allocate space to hold some parameters.
     */
    URL(const char *url)
    {
        //
        // Copy the URL into place.
        //
        m_url = strdup( url );

        //
        // We'll cap the URL at the first space, if present.
        //
        // This allows the caller to be a bit sloppy :)
        //
        char *x = strstr(m_url, " ");
        if (x != NULL)
            m_url[ x - m_url ] = '\0';

        //
        // We've not yet parsed the parameters.
        //
        m_parsed = 0;

        //
        // Allocate some storage for the array of parameter-objects.
        //
        m_params = (UrlParam **)malloc(sizeof(struct UrlParam) * (MAX_PARAMS));

        //
        // Initialise each one.
        //
        for (int i = 0; i < MAX_PARAMS ; i++)
        {
            m_params[i] = (UrlParam*)malloc(sizeof(struct UrlParam));
            m_params[i]->name = NULL;
            m_params[i]->value = NULL;
        }
    }

    /**
     * Destructor.  Free up our dynamic allocations.
     */
    ~URL()
    {
        if (m_url)
            free(m_url);

        if (m_params)
        {
            for (int i = 0; i < MAX_PARAMS ; i++)
            {
                if (m_params[i]->name)
                    free(m_params[i]->name);

                if (m_params[i]->value)
                    free(m_params[i]->value);

                free(m_params[i]);
            }

            free(m_params);
        }
    }

    /**
     * Parse any supplied URL-parameters, to a limit of 10, and update
     * our internal array.
     *
     * NOTE: This also URL-decodes the values.
     */
    void parse()
    {

        //
        // The count of parameters
        //
        int c = 0;


        //
        // If we've already parsed then return
        //
        if (m_parsed)
            return;

        char* pch = NULL;

        //
        // Do we have some params?
        //
        char *x = strstr(m_url, "?");

        if (x == NULL)
            return;

        //
        // Split by "&"
        //
        pch = strtok(x + 1 , "&");

        while (pch != NULL)
        {
            //
            // We now have "blah=blah"
            //
            // We need to split the key/value
            //
            char *equal = strstr(pch, "=");

            if (equal != NULL)
            {
                //
                // The name, truncated at the equals-sign
                //
                m_params[c]->name = strdup(pch);
                m_params[c]->name[equal - pch ] = '\0';

                //
                // The value, which is URL-decoded
                //
                m_params[c]->value = urldecode(equal + 1);

                //
                // Bump to the next parameter.
                //
                c += 1;

                //
                // We stop at the max-params.
                //
                if (c >= MAX_PARAMS)
                    return;
            }

            pch = strtok(NULL, "&");
        }

        m_parsed = 1;
    }

    /**
     * Iterate over all known-values and return what we found
     * for the given parameter.  Return NULL if the given parameter
     * wasn't found.
     */
    char *param(const char *name)
    {
        parse();

        for (int i = 0; i < MAX_PARAMS ; i++)
        {
            if (m_params[i]->name &&
                    strcmp(m_params[i]->name, name) == 0)
                return (m_params[i]->value);
        }

        return NULL;
    }

    /**
     * Return the count of parameters received.
     */
    int count()
    {
        parse();

        int count = 0;

        for (int i = 0; i < MAX_PARAMS ; i++)
        {
            if (m_params[i]->name)
                count += 1;
        }

        return count;
    }

    /**
     * Get the name of the Nth param.
     */
    char *param_name(int i)
    {
        parse();

        if (i > count())
            return NULL;

        return (m_params[i]->name);
    };

    /**
     * Get the (urldecoded) value of the Nth param.
     */
    char *param_value(int i)
    {
        parse();

        if (i > count())
            return NULL;

        return (m_params[i]->value);
    };

private:

    /**
     * Decodes an URL from its percent-encoded form back into normal
     * representation. This function returns the decoded URL in a string.
     * The URL to be decoded does not necessarily have to be encoded but
     * in that case the original string is duplicated.
     *
     * @param url a string to be decoded.
     * @return new string with the URL decoded or NULL if decoding failed.
     * Note that the returned string should be explicitly freed when not
     * used anymore.
     */
    char *urldecode(const char *url)
    {
        int s = 0, d = 0, url_len = 0;
        char c;
        char *dest = NULL;

        if (!url)
            return NULL;

        url_len = strlen(url) + 1;
        dest = (char *)malloc(url_len);

        if (!dest)
            return NULL;

        while (s < url_len)
        {
            c = url[s++];

            if (c == '%' && s + 2 < url_len)
            {
                char c2 = url[s++];
                char c3 = url[s++];

                if (isxdigit(c2) && isxdigit(c3))
                {
                    c2 = tolower(c2);
                    c3 = tolower(c3);

                    if (c2 <= '9')
                        c2 = c2 - '0';
                    else
                        c2 = c2 - 'a' + 10;

                    if (c3 <= '9')
                        c3 = c3 - '0';
                    else
                        c3 = c3 - 'a' + 10;

                    dest[d++] = 16 * c2 + c3;

                }
                else     /* %zz or something other invalid */
                {
                    dest[d++] = c;
                    dest[d++] = c2;
                    dest[d++] = c3;
                }
            }
            else if (c == '+')
            {
                dest[d++] = ' ';
            }
            else
            {
                dest[d++] = c;
            }

        }

        return dest;
    };


private:
    /*
     * The URL we were created with.
     */
    char *m_url;

    /*
     * The array of parameter-name + values.
     */
    UrlParam **m_params;

    /*
     * Have we parsed?
     */
    int m_parsed ;
};

#if 0
int main(int argc, char *argv[])
{
    URL x("http://example.com/?foo=bar&ex=%2f&x=34&b=px");
    printf("foo: %s\n", x.param("foo"));
    printf("bar: %s\n", x.param("bar"));
    printf("ex: %s\n", x.param("ex"));

    printf("There were %d params\n", x.count());

    for (int i = 0; i < x.count(); i++)
    {
        printf("Param %d : %s -> %s\n", i, x.param_name(i), x.param_value(i));
    }
}
#endif
