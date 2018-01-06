//
// d1-web-radio.ino
//
// Operate a radio.  We serve a simple HTTP-page allowing the user
// to search up/down, or enter a frequency directly.
//
// Omissions in this project include:
//
// * Volume control.
// * RDS
//
// These are missing from the hardware, but we _could_ add muting..
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

//Variables:
int search_mode = 0;
int search_direction;
unsigned char buf[5];
int inByte;
int flag = 0;


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
#endif

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

    if (Serial.available() > 0)
    {
        inByte = Serial.read();

        if (inByte == '+' || inByte == '-')   //accept only + and - from keyboard
        {
            flag = 0;
        }
    }


    if (Radio.read_status(buf) == 1)
    {
        //When button pressed, search for new station
        if (search_mode == 1)
        {
            if (Radio.process_search(buf, search_direction) == 1)
            {
                search_mode = 0;
            }
        }

        //If forward button is pressed, go up to next station
        if (inByte == '+')
        {
            Serial.println("Searching up");
            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_UP;
            Radio.search_up(buf);
        }

        //If backward button is pressed, go down to next station
        if (inByte == '-')
        {
            Serial.println("Searching down");
            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_DOWN;
            Radio.search_down(buf);
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
    // Now sleep a little.
    //
    delay(5);
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
    client.println("<script src=\"//code.jquery.com/jquery-1.12.4.min.js\"></script>");
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
        client.println("<div class=\"col-md-9\"> <h2>Search</h2></div>");
        client.println("</div>");
        client.println("<div class=\"row\">");
        client.println("<div class=\"col-md-4\"></div>");
        client.println("<div class=\"col-md-4\">");
        client.print("<p>Search <a href=\"/?search=up\">up</a>, or <a href=\"/?search=down\">down</a>.</p>");
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

        }

        if (strcmp(search, "down") == 0)
        {
            search_mode = 1;
            search_direction = TEA5767_SEARCH_DIR_DOWN;
            Radio.search_up(buf);
        }

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }

    // Return a simple response
    serveHTML(client);

}
