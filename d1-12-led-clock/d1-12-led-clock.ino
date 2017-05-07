

//
// The name of this project.
//
// Used for the Access-Point name, and for OTA-identification.
//
#define PROJECT_NAME "12-LED-CLOCK"


#include "FastLED.h"
#define NUM_LEDS 12

// Define the array of leds
CRGB leds[NUM_LEDS];


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

#include <ESP8266WiFi.h>


//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);

//
// We read/write data to flash.
//
#include <FS.h>


//
// Damn this is a nice library!
//
//   https://github.com/tzapu/WiFiManager
//
#include "WiFiManager.h"


//
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

//
// NTP uses UDP.
//
#include <WiFiUdp.h>



//
// For dealing with NTP & the clock.
//
#include "NTPClient.h"


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);



//
// We'll always be in one of a small number of modes/states.
//
// This is the list of such states.
//
typedef enum {CLOCK, BLINK, SWEEP} state;

//
// Our current state
//
state g_state = CLOCK;


//
// Timezone offset from GMT
//
int time_zone_offset = 0;

//
// UDP-socket & local-port for replies.
//
WiFiUDP Udp;
unsigned int localPort = 2390;

//
// The NTP-server we use.
//
static const char ntpServerName[] = "pool.ntp.org";



void setup()
{

    //
    // Enable our serial port.
    //
    Serial.begin(115200);

    //
    // Enable access to the filesystem.
    //
    SPIFFS.begin();

    //
    // Load the time-zone offset, if we can
    //
    String tz_str = read_file("/time.zone");

    if (tz_str.length() > 0)
        time_zone_offset = tz_str.toInt();

    //
    // Show a message.
    //
    DEBUG_LOG("Starting up ..\n");

    //
    // Handle Connection.
    //
    WiFiManager wifiManager;
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("WiFi Connected: ");
    DEBUG_LOG(WiFi.localIP().toString().c_str());
    DEBUG_LOG("\n");


    //
    // Ensure our NTP-client is ready.
    //
    timeClient.begin();
    DEBUG_LOG("Timezone offset is ");
    DEBUG_LOG(time_zone_offset);
    DEBUG_LOG("\n");
    timeClient.setTimeOffset(time_zone_offset * (60 * 60));
    timeClient.setUpdateInterval(300);

    //
    // Launch the HTTP-server
    //
    server.begin();
    DEBUG_LOG("HTTP-Server started on ");
    DEBUG_LOG("http://");
    DEBUG_LOG(WiFi.localIP().toString().c_str());
    DEBUG_LOG("\n");

    //
    // Configure our LEDs
    //
    FastLED.addLeds<WS2812,  D6, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(15);

    //
    // The final step is to allow over the air updates
    //
    // This is documented here:
    //     https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/
    //
    // Hostname defaults to esp8266-[ChipID]
    //
    ArduinoOTA.setHostname(PROJECT_NAME);

    ArduinoOTA.onStart([]()
    {
        DEBUG_LOG("OTA Start\n");
    });
    ArduinoOTA.onEnd([]()
    {
        DEBUG_LOG("OTA Ended\n");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[21];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, 20, "Upgrade - %02u%%\n", (progress / (total / 100)));
        DEBUG_LOG(buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        if (error == OTA_AUTH_ERROR)
            DEBUG_LOG("Auth Failed\n");
        else if (error == OTA_BEGIN_ERROR)
            DEBUG_LOG("Begin Failed\n");
        else if (error == OTA_CONNECT_ERROR)
            DEBUG_LOG("Connect Failed\n");
        else if (error == OTA_RECEIVE_ERROR)
            DEBUG_LOG("Receive Failed\n");
        else if (error == OTA_END_ERROR)
            DEBUG_LOG("End Failed\n");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();



}

void loop()
{

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

    //
    // Resync the clock?
    //
    timeClient.update();

    //
    // Check if a client has connected to our HTTP-server.
    //
    // If so handle it.
    //
    // (This allows changing some settings.)
    //
    WiFiClient client = server.available();

    if (client)
        processHTTPRequest(client);


    //
    // Draw our current state.
    //
    switch (g_state)
    {
    case CLOCK:
        draw_clock();
        break;

    case BLINK:
        draw_blink();
        break;

    case SWEEP:
        draw_sweep();
        break;
    };

}

//
// Draw the clock-face
//
// This involves setting a pixel for the hour & minute.
//
// This function is called non-stop, but only updates every
// second.
//
void draw_clock()
{
    //
    // The last time we drew our display.
    //
    static long long draw = 0;

    //
    // If we were last called <1s ago, return
    //
    if ((millis() - draw) < 1000)
        return;

    //
    // Record the current time.
    //
    draw = millis();


    //
    // Set the brightness.
    //
    FastLED.setBrightness(15);

    //
    // Get the current time.
    //
    int hur = timeClient.getHours();
    int min = timeClient.getMinutes();

    DEBUG_LOG("Time is ");
    DEBUG_LOG(hur);
    DEBUG_LOG(":");
    DEBUG_LOG(min);
    DEBUG_LOG("\n");


    //
    // Transform hours & minutes into suitable
    // values for our 12-LED display
    //
    hur = hur % 12;
    min = min / 5;
    min = min % 12;

    DEBUG_LOG("Adjusted Time ");
    DEBUG_LOG(hur);
    DEBUG_LOG(":");
    DEBUG_LOG(min);
    DEBUG_LOG("\n");


    // All LEDs should be off
    for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::Black;


    //
    // Turn the LED for the hour + minute on.
    //
    leds[hur] = CRGB::Red;
    leds[min] = CRGB::Green;

    //
    // If both values are the same then use a different
    // colour.
    //
    if (hur == min)
        leds[min] = CRGB::Blue;

    //
    // Update the state of the LEDs
    //
    FastLED.show();

}


//
// Sweep the LEDs, every 500ms, in a non-blocking fashion.
//
void draw_sweep()
{
    static int cur = 0;
    static long long draw = 0;

    //
    // If we were last called <500ms ago, return
    //
    if ((millis() - draw) < 500)
        return;

    //
    // Record the current time.
    //
    draw = millis();

    //
    // Setup the state of the LEDS
    //
    FastLED.setBrightness(60);

    for (int i = 0; i < NUM_LEDS; i++)
    {
        if (i < cur)
            leds[i] = CRGB::Red;
        else
            leds[i] = CRGB::Black;
    }

    // Draw them
    FastLED.show();

    cur += 1;

    if (cur > NUM_LEDS)
        cur = 0;
}



//
// Blink the LEDs, every 500ms, in a non-blocking fashion.
//
void draw_blink()
{
    static bool on = false;
    static long long draw = 0;

    //
    // If we were last called <500ms ago, return
    //
    if ((millis() - draw) < 500)
        return;

    //
    // Record the current time.
    //
    draw = millis();

    //
    // Setup the state of the LEDS
    //
    FastLED.setBrightness(60);

    //
    // We're blinking - so we're either
    // all-on or all-off
    //
    for (int i = 0; i < NUM_LEDS; i++)
    {
        if (on)
            leds[i] = CRGB::Red;
        else
            leds[i] = CRGB::Black;
    }

    // Draw them
    FastLED.show();

    // Invert our state for next time.
    on = ! on;
}


//
// Open the given file for writing, and write
// out the specified data.
//
void write_file(const char *path, const char *data)
{
    File f = SPIFFS.open(path, "w");

    if (f)
    {
        DEBUG_LOG("Writing file:");
        DEBUG_LOG(path);
        DEBUG_LOG(" data:");
        DEBUG_LOG(data);
        DEBUG_LOG("\n");

        f.println(data);
        f.close();
    }
}

//
// Read a single line from a given file, and return it.
//
// This function explicitly filters out `\r\n`, and only
// reads the first line of the text.
//
String read_file(const char *path)
{
    String line;

    File f = SPIFFS.open(path, "r");

    if (f)
    {
        line = f.readStringUntil('\n');
        f.close();
    }


    String result;

    for (int i = 0; i < line.length() ; i++)
    {
        if ((line[i] != '\n') &&
                (line[i] != '\r'))
            result += line[i];
    }

    return (result);
}


//
// Process an incoming HTTP-request
//
void processHTTPRequest(WiFiClient client)
{
    // Wait until the client sends some data
    while (client.connected() && !client.available())
        delay(1);

    // Read the first line of the request
    String request = client.readStringUntil('\r');
    client.flush();

    // Change the state to blink?
    if (request.indexOf("/state/blink") != -1)
    {
        g_state = BLINK;
        redirectIndex(client);
        return;
    }

    // Change the state to clock?
    if (request.indexOf("/state/clock") != -1)
    {
        g_state = CLOCK;
        redirectIndex(client);
        return;
    }

    // Change the state to sweep?
    if (request.indexOf("/state/sweep") != -1)
    {
        g_state = SWEEP;
        redirectIndex(client);
        return;
    }

    // Change the time-zone?
    if (request.indexOf("/?tz=") != -1)
    {
        char *pattern = "/?tz=";
        char *s = strstr(request.c_str(), pattern);

        if (s != NULL)
        {
            // Temporary holder for the timezone - as a string.
            char tmp[10] = { '\0' };

            // Skip past the pattern.
            s += strlen(pattern);

            // Add characters until we come to a terminating character.
            for (int i = 0; i < strlen(s); i++)
            {
                if ((s[i] != ' ') && (s[i] != '&'))
                    tmp[strlen(tmp)] = s[i];
                else
                    break;
            }

            // Record the data into a file.
            write_file("/time.zone", tmp);

            // Change the timezone now
            time_zone_offset = atoi(tmp);

            // Update the offset.
            timeClient.setTimeOffset(time_zone_offset * (60 * 60));
            timeClient.forceUpdate();
        }

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }


    // Return a simple response
    serveHTML(client);

}


//
// This is a bit horrid.
//
// Serve a HTML-page to any clients who connect via a browser.
//
void serveHTML(WiFiClient client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");

    client.println("<!DOCTYPE html>");
    client.println("<html lang=\"en\">");
    client.println("<head>");
    client.println("<title>12-LED Clock</title>");
    client.println("<meta charset=\"utf-8\">");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    client.println("<link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u\" crossorigin=\"anonymous\">");
    client.println("<script src=\"//code.jquery.com/jquery-1.12.4.min.js\"></script>");
    client.println("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\" integrity=\"sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa\" crossorigin=\"anonymous\"></script>");
    client.println("</head>");
    client.println("<body>");
    client.println("<nav id=\"nav\" class = \"navbar navbar-default\" style=\"padding-left:50px; padding-right:50px;\">");
    client.println("<div class = \"navbar-header\">");
    client.println("<h1 class=\"banner\"><a href=\"/\">12-LED Clock</a> - <small>by Steve</small></h1>");
    client.println("</div>");
    client.println("<ul class=\"nav navbar-nav navbar-right\">");
    client.println("<li><a href=\"https://steve.fi/Hardware/\">Steve's Projects</a></li>");
    client.println("</ul>");
    client.println("</nav>");
    client.println("<div class=\"container-fluid\">");

    // Start of body
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"><h1>12-LED Clock</h1><p>&nbsp;</p></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\"><p>This project draws a simplified clock, via 12 LEDs.</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");


    // Row
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>State</h2></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");

    // Showing the state.
    if (g_state == BLINK)
        client.println("<p>The current state is &quot;blinking&quot;.</p>");

    if (g_state == SWEEP)
        client.println("<p>The current state is &quot;sweeping&quot;.</p>");

    if (g_state == CLOCK)
        client.println("<p>The current state is &quot;clock&quot;.</p>");

    client.println("<p>Change to <a href=\"/state/blink\">blinking</a>, <a href=\"/state/sweep\">sweeping</a>, or simply draw the <a href=\"/state/clock\">clock</a></p>");

    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");

    // Row
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>Change Time Zone</h2></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");
    client.print("<p>The time zone you're configured is GMT ");

    if (time_zone_offset > 0)
        client.print("+");

    if (time_zone_offset < 0)
        client.print("-");

    client.print(time_zone_offset);
    client.print(" but you can change that:</p>");
    client.print("<form action=\"/\" method=\"GET\"><input type=\"text\" name=\"tz\" value=\"");

    if (time_zone_offset > 0)
        client.print("+");

    if (time_zone_offset < 0)
        client.print("-");

    client.print(time_zone_offset);
    client.println("\"><input type=\"submit\" value=\"Update\"></form>");
    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");

    // End of body
    client.println("</div>");
    client.println("</body>");
    client.println("</html>");

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
