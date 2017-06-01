//
//   This is a simple template-project which does the minimal
//  kind of thing that any of my projects tend to do:
//
//   * Handle the setup of WiFi.
//
//   * Handle OTA - no password.
//
//   * Get the time/date via NTP.
//
//
//   Steve
//   --
//


//
// The name of this project.
//
// Used for the Access-Point name, and for OTA-identification.
//
#define PROJECT_NAME "TEMPLATE"

//
// Should we enable debugging (via serial-console output) ?
//
// Use either `#undef DEBUG`, or `#define DEBUG`.
//
#define DEBUG


//
// If we did then DEBUG_LOG will log a string, otherwise
// it will be ignored as a comment.
//
#ifdef DEBUG
#  define DEBUG_LOG(x) Serial.print(x)
#else
#  define DEBUG_LOG(x)
#endif


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
// For dealing with NTP & the clock.
//
#include "NTPClient.h"

//
// The NTP-client object.
//
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//
// Timezone offset from GMT
//
int time_zone_offset = 0;


//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);




//
// This function is called when the device is powered-on.
//
void setup()
{
    //
    // Enable our serial port.
    //
    Serial.begin(115200);

    //
    // Handle Connection.
    //
    WiFiManager wifiManager;
    wifiManager.setAPCallback(access_point_callback);
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("\nWiFi Connected :");
    DEBUG_LOG(WiFi.localIP().toString().c_str());
    DEBUG_LOG("\n");

    //
    // Ensure our NTP-client is ready.
    //
    timeClient.begin();

    //
    // Configure the NTP-callbacks
    //
    timeClient.on_before_update(on_before_ntp);
    timeClient.on_after_update(on_after_ntp);

    //
    // Setup the timezone & update-interval.
    //
    timeClient.setTimeOffset(time_zone_offset * (60 * 60));
    timeClient.setUpdateInterval(300 * 1000);


    //
    // Now we can start our HTTP server
    //
    server.begin();
    DEBUG_LOG("HTTP-Server started on ");
    DEBUG_LOG("http://");
    DEBUG_LOG(WiFi.localIP().toString().c_str());
    DEBUG_LOG("\n");

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
        DEBUG_LOG("OTA Start");
    });
    ArduinoOTA.onEnd([]()
    {
        DEBUG_LOG("OTA Ended");
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
}



//
// Called just before the date/time is updated via NTP
//
void on_before_ntp()
{
    DEBUG_LOG("Updating date & time\n");
}

//
// Called just after the date/time is updated via NTP
//
void on_after_ntp()
{
    DEBUG_LOG("\tUpdated date & time\n");
}

//
// If we're unconfigured we run an access-point.
//
// Show that, explicitly.
//
void access_point_callback(WiFiManager* myWiFiManager)
{
    DEBUG_LOG("AccessPoint Mode Enabled");
}



//
// This function is called continously.
//
void loop()
{
    //
    // Keep the previous time, to avoid needless re-draws
    //
    static char prev_time[40] = { '\0'};
    static char curr_time[40] = { '\0'};

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

    //
    // Resync the clock?
    //
    timeClient.update();

    //
    // Get the current time.
    //
    // We save this so that we can test if we need to update the
    // tram-time, which we do every two minutes.
    //
    //
    int hour = timeClient.getHours();
    int min  = timeClient.getMinutes();
    int sec  = timeClient.getSeconds();
    int year = timeClient.getYear();

    String d_name = timeClient.getWeekDay();
    String m_name = timeClient.getMonth();
    int day = timeClient.getDayOfMonth();

    //
    // Format the time & date.
    //
    snprintf(curr_time, sizeof(curr_time) - 1,
             "%02d:%02d:%02d %s %02d %s %04d\n",
             hour, min, sec, d_name.c_str(), day, m_name.c_str(), year);


    //
    // Output to the serial-console if the time
    // has changed - i.e. once per second.
    //
    if (strcmp(curr_time , prev_time) != 0)
    {
        DEBUG_LOG(curr_time);
        strcpy(prev_time, curr_time);
    }

    //
    // Check if a client has connected to our HTTP-server.
    //
    // If so handle it.
    //
    // (This allows changing the stop, timezone, backlight, etc.)
    //
    WiFiClient client = server.available();

    if (client)
        processHTTPRequest(client);

    //
    // Now sleep a little.
    //
    delay(20);
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
// Serve a HTML-page to any clients who connect, via
// a browser.
//
void serveHTML(WiFiClient client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");

    client.println("<!DOCTYPE html>");
    client.println("<html lang=\"en\">");
    client.println("<head>");
    client.println("<title>Template Project</title>");
    client.println("<meta charset=\"utf-8\">");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    client.println("<link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u\" crossorigin=\"anonymous\">");
    client.println("<script src=\"//code.jquery.com/jquery-1.12.4.min.js\"></script>");
    client.println("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\" integrity=\"sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa\" crossorigin=\"anonymous\"></script>");
    client.println("</head>");
    client.println("<body>");
    client.println("<nav id=\"nav\" class = \"navbar navbar-default\" style=\"padding-left:50px; padding-right:50px;\">");
    client.println("<div class = \"navbar-header\">");
    client.println("<h1 class=\"banner\"><a href=\"/\">Template Project</a> - <small>by Steve</small></h1>");
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
    client.println("<div class=\"col-md-9\"><h1>Template Project</h1><p>&nbsp;</p></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");
    client.println("<p>This is a template project.  Update as you wish.</p>");
    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");


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
    // Sample of how to handle a particular request
    //
    //
#if 0

    if (request.indexOf("/ON") != -1)
    {
        backlight = true;
        lcd.setBacklight(true);

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }

#endif

    // Return a simple response
    serveHTML(client);

}
