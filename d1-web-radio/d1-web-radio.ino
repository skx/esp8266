//
// d1-web-radio.ino
//
// An internet-connected radio!  We serve a simple HTTP-page allowing the user
// to search up/down, enter a frequency directly, and mute/unmute.
//
// For clients connected via serial we can also interact via that.
//
// Omissions in this project are caused by lack of hardware support:
//
// * Volume control.
// * RDS.
//
//   Steve
//   --
//



//
// WiFi manager library, for working as an access-point, etc.
//
#include "WiFiManager.h"


//
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>


//
// Debug messages over the serial console.
//
#include "debug.h"

//
// Decode URL parameters.
//
#include "url_parameters.h"

//
// Radio-library
//
#include "TEA5767.h"
#include <Wire.h>

//
// The name of this project.
//
// Used for the Access-Point name, and for OTA-identification.
//
#define PROJECT_NAME "WEB-RADIO"


//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);


//
//  The Radio object.
//
TEA5767 Radio;

//
// Are we searching?
//
int search_mode = 0;

//
// If we're searching then this holds the direction.
//
int search_direction;

//
// Buffer for reading the radio-status
//
unsigned char buf[5];



//
// This function is called when the device is powered-on.
//
void setup()
{
    //
    // Enable our serial port.
    //
#ifdef DEBUG
    Serial.begin(115200);
    Serial.setTimeout(25);
#endif

    //
    // So we get working mdns.
    //
    WiFi.hostname(PROJECT_NAME);

    //
    // Handle Connection.
    //
    WiFiManager wifiManager;
    wifiManager.setAPCallback(access_point_callback);
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("\nWiFi Connected with IP %s\n",
              WiFi.localIP().toString().c_str());

    //
    // Now we can start our HTTP server
    //
    server.begin();
    DEBUG_LOG("HTTP-Server started on http://%s\n",
              WiFi.localIP().toString().c_str());


    //
    // Allow over the air updates
    //
    // This is documented here:
    //     https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/
    //
    // Hostname defaults to esp8266-[ChipID]
    //
    ArduinoOTA.setHostname(PROJECT_NAME);

    ArduinoOTA.onStart([]()
    {
        DEBUG_LOG("OTA Started:");
    });
    ArduinoOTA.onEnd([]()
    {
        DEBUG_LOG("OTA Ended.");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[64];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%", (progress / (total / 100)));
        DEBUG_LOG(buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        if (error == OTA_AUTH_ERROR)
            DEBUG_LOG("\tAuth Failed\n");
        else if (error == OTA_BEGIN_ERROR)
            DEBUG_LOG("\tBegin Failed\n");
        else if (error == OTA_CONNECT_ERROR)
            DEBUG_LOG("\tConnect Failed\n");
        else if (error == OTA_RECEIVE_ERROR)
            DEBUG_LOG("\tReceive Failed\n");
        else if (error == OTA_END_ERROR)
            DEBUG_LOG("\tEnd Failed\n");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();

    //
    // Setup the radio
    //
    Wire.begin();
    Radio.init();

    //
    // Pick a default frequency.
    //
    Radio.set_frequency(94.90);
}


//
// If we're unconfigured we run an access-point.
//
// Show that, explicitly.
//
void access_point_callback(WiFiManager* myWiFiManager)
{
    DEBUG_LOG("AccessPoint Mode Enabled.\n");
}



//
// This function is called continously.
//
void loop()
{
    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();


    //
    // Are we searching?
    //
    if (search_mode == 1)
    {
        //
        // If so try to do the search, and if it succeeds then
        // cancel further searching.
        //
        if (Radio.read_status(buf) == 1)
        {
            if (Radio.process_search(buf, search_direction) == 1)
            {
                search_mode = 0;
            }
        }
    }

    //
    // Check if a client has connected to our HTTP-server.
    //
    // If so handle it.
    //
    WiFiClient client = server.available();

    if (client)
        processHTTPRequest(client);

    //
    // Is there serial-input available?
    //
    if (Serial.available())
    {

        //
        // Read a command from the serial-console.
        //
        char buffer[80] = { '\0' };
        int read = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);

        //
        // Remove non-print characters
        //
        for (int i = 0; i < strlen(buffer); i++)
        {
            if (! isprint(buffer[i]))
                buffer[i] = '\0';
        }

        //
        // Now look for commands we know of.
        //
        if (strcmp(buffer, "search up") == 0)
        {

            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_UP;
            Radio.search_up(buf);
            DEBUG_LOG("Searching up ..\n");
        }
        else if (strcmp(buffer, "search down") == 0)
        {

            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_DOWN;
            Radio.search_down(buf);
            DEBUG_LOG("Searching down ..\n");
        }
        else if (strcmp(buffer, "mute") == 0)
        {
            Radio.mute();
        }
        else if (strcmp(buffer, "info") == 0)
        {
            if (Radio.read_status(buf) == 1)
            {
                double current_freq = floor(Radio.frequency_available(buf) / 100000 + .5) / 10;
                int  stereo = Radio.stereo(buf);
                int signal_level = Radio.signal_level(buf);

                DEBUG_LOG("Tuned to %fFM\n", current_freq);
                DEBUG_LOG("Signal strength: %d/15\n", signal_level);

                if (stereo)
                    DEBUG_LOG("(stereo)\n");
                else
                    DEBUG_LOG(" (mono)\n");
            }
        }
        else if (strcmp(buffer, "unmute") == 0)
        {

            if (Radio.read_status(buf) == 1)
            {
                double current_freq = floor(Radio.frequency_available(buf) / 100000 + .5) / 10;
                Radio.set_frequency(current_freq);
            }
        }
        else if ((strncmp(buffer, "tune ", 5) == 0) && (strlen(buffer) > 5))
        {

            // Look for the number
            double f = atof(buffer + 4);
            DEBUG_LOG("Tuning to %f\n", f);

            Radio.set_frequency(f);
        }
        else
        {
            DEBUG_LOG("Ignoring input: '%s'.  Valid commands are:\n", buffer);
            DEBUG_LOG("\tinfo\n");
            DEBUG_LOG("\tmute\n");
            DEBUG_LOG("\tsearch up\n");
            DEBUG_LOG("\tsearch down\n");
            DEBUG_LOG("\ttune 12.3\n");
            DEBUG_LOG("\tunmute\n");

        }
    }
}


//
// Serve a redirect to the server-root
//
void redirectIndex(WiFiClient client)
{
    client.println("HTTP/1.1 302 Found");
    client.print("Location: http://");
    client.print(WiFi.localIP().toString().c_str());
    client.println("/");
}


//
// This is a bit horrid.
//
// Serve a HTML-page to any clients who connect, via a browser.
//
void serveHTML(WiFiClient client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");

    client.println("<!DOCTYPE html>");
    client.println("<html lang=\"en\">");
    client.println("<head>");
    client.println("<title>Web Radio</title>");
    client.println("<meta charset=\"utf-8\">");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    client.println("<link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u\" crossorigin=\"anonymous\">");
    client.println("<script src=\"https://code.jquery.com/jquery-1.12.4.min.js\" integrity=\"sha256-Qw82+bXyGq6MydymqBxNPYTaUXXq7c8v3CwiYwLLNXU=\" crossorigin=\"anonymous\"></script>");
    client.println("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\" integrity=\"sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa\" crossorigin=\"anonymous\"></script>");
    client.println("</head>");
    client.println("<body>");
    client.println("<nav id=\"nav\" class = \"navbar navbar-default\" style=\"padding-left:50px; padding-right:50px;\">");
    client.println("<div class = \"navbar-header\">");
    client.println("<h1 class=\"banner\"><a href=\"/\">Web Radio</a> - <small>by Steve</small></h1>");
    client.println("</div>");
    client.println("<ul class=\"nav navbar-nav navbar-right\">");
    client.println("<li><a href=\"https://steve.fi/Hardware/\">Steve's Projects</a></li>");
    client.println("</ul>");
    client.println("</nav>");
    client.println("<div class=\"container-fluid\">");

    // Start of body

    // Row
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"><h1>Web Radio</h1><p>&nbsp;</p></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");

    if (Radio.read_status(buf) == 1)
    {
        double current_freq = floor(Radio.frequency_available(buf) / 100000 + .5) / 10;
        int  stereo = Radio.stereo(buf);
        int signal_level = Radio.signal_level(buf);

        client.print("<p>Currently listening to ");
        client.println(current_freq);
        client.println("FM.</p>");

        client.print("<p>The signal strength is ");
        client.print(signal_level);
        client.print("/15 ");

        if (stereo)
            client.println(" (stereo).</p>");
        else
            client.println(" (mono).</p>");


        client.println("</div>");
        client.println("<div class=\"col-md-4\"></div>");
        client.println("</div>");


        // Row
        client.println("<div class=\"row\">");
        client.println("<div class=\"col-md-3\"></div>");
        client.println("<div class=\"col-md-9\"> <h2>Change Frequency</h2></div>");
        client.println("</div>");
        client.println("<div class=\"row\">");
        client.println("<div class=\"col-md-4\"></div>");
        client.println("<div class=\"col-md-4\">");
        client.print("<p>Here you can change the frequency directly:</p>");
        client.println("<form action=\"/\" method=\"GET\"><input type=\"text\" name=\"freq\" value=\"");
        client.println(current_freq);

        client.println("\"><input type=\"submit\" value=\"Update\"></form>");
        client.println("</div>");
        client.println("<div class=\"col-md-4\"></div>");
        client.println("</div>");


        // Row
        client.println("<div class=\"row\">");
        client.println("<div class=\"col-md-3\"></div>");
        client.println("<div class=\"col-md-9\"> <h2>Operations</h2></div>");
        client.println("</div>");
        client.println("<div class=\"row\">");
        client.println("<div class=\"col-md-4\"></div>");
        client.println("<div class=\"col-md-4\">");
        client.print("<p>Search <a href=\"/?search=up\">up</a>, or <a href=\"/?search=down\">down</a>.</p>");
        client.print("<p><a href=\"/?mute=1\">Mute</a>, or <a href=\"/?unmute=1\">unmute</a>.</p>");
        client.println("</div>");
        client.println("<div class=\"col-md-4\"></div>");
        client.println("</div>");

    }
    else
    {
        client.println("<p>Failed to find radio-details..</p>");
    }

    // End of body
    client.println("</div>");
    client.println("</body>");
    client.println("</html>");

}


//
// Process an incoming HTTP-request.
//
void processHTTPRequest(WiFiClient client)
{
    // Wait until the client sends some data
    while (client.connected() && !client.available())
        delay(1);

    // Read the first line of the request
    String request = client.readStringUntil('\r');
    client.flush();

    //
    // Find the URL we were requested
    //
    // We'll have something like "GET XXXXX HTTP/XX"
    // so we split at the space and send the "XXX HTTP/XX" value
    //
    request = request.substring(request.indexOf(" ")  + 1);

    //
    // Now we'll want to peel off any HTTP-parameters that might
    // be present, via our utility-helper.
    //
    URL url(request.c_str());

    //
    // Does the user want to tune directly?
    //
    char *number = url.param("freq");

    if (number != NULL)
    {
        // Change the timezone now
        double var  = atof(number);

        DEBUG_LOG("Converted '%s' -> '%f'\n", number, var);

        Radio.set_frequency(var);

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }

    //
    // Does the user want to scan?
    //
    char *search = url.param("search");

    if (search != NULL)
    {
        if (strcmp(search, "up") == 0)
        {
            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_UP;
            Radio.search_up(buf);
            DEBUG_LOG("Searching up ..\n");
        }

        if (strcmp(search, "down") == 0)
        {
            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_DOWN;
            Radio.search_down(buf);
            DEBUG_LOG("Searching down ..\n");
        }

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }

    char *mute = url.param("mute");

    if (mute != NULL)
    {
        DEBUG_LOG("Mute ..\n");
        Radio.mute();
        redirectIndex(client);
        return;
    }

    char *unmute = url.param("unmute");

    if (unmute != NULL)
    {
        DEBUG_LOG("Unmute ..\n");

        if (Radio.read_status(buf) == 1)
        {
            double current_freq = floor(Radio.frequency_available(buf) / 100000 + .5) / 10;
            Radio.set_frequency(current_freq);
        }

        redirectIndex(client);
        return;
    }


    // Return a simple response
    serveHTML(client);

}
