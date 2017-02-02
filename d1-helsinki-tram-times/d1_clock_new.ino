//
//   Helsinki Tram Time Clock - https://steve.fi/Hardware/
//
//   This is a simple program which uses WiFi & an LCD display
//   to show useful information.
//
//   The top line of the display will show the current date & time,
//   which will be retrieved via NTP every five minutes - we don't
//   need an RTC to store the time, because even if it drifts it
//   can't drift too much in that time.
//
//   Each additional line of hte display will show the departure
//   of the next tram from the chosen stop.
//
//   One a two-line display we thus see output like this:
//
//      ####################
//      # 11:12:13 1 Feb   #
//      # Tram 10 @ 11:17  #
//      ####################
//
//   On a four-line display we'd see something else:
//
//      ####################
//      # 11:12:13 1 Feb   #
//      # Tram 4B @ 11:17  #
//      # Tram 7B @ 11:35  #
//      # Tram 10 @ 11:45  #
//      ####################
//
//
//   Finally there is a simple HTTP-server which is running on
//   port 80, which shows the same information, and allows the
//   control of the backlight and the stop-ID to be changed.
//
//   This can be accessed via:
//
//    http://192.168.10.51/
//
//   Steve
//   --
//


//
// Use the user-friendly WiFI library?
//
// If you don't want to use this then comment out the following line:
//
#define WIFI_MANAGER

//
//  Otherwise define a SSID / Password
//
#ifndef WIFI_MANAGER
# define WIFI_SSID "SCOTLAND"
# define WIFI_PASS "highlander1"
#endif

//
// The number of rows/columns of our display.
//
#define NUM_ROWS 2
#define NUM_COLS 16

//
// The URL to poll
//
#define TRAM_BASE "http://hsl.trapeze.fi/omatpysakit/web?command=fullscreen2&action=departures&stop="

//
// The tram ID
//
#define TRAM_STOP "1160404"

//
// Lasipalatsi - Useful for testing since multiple routes stop there.
//
// #define TRAM_STOP "1020444"
//

//
// The timezone - comment out to stay at GMT.
//
#define TIME_ZONE (+2)

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
// Damn this is a nice library!
//
//   https://github.com/tzapu/WiFiManager
//
#ifdef WIFI_MANAGER
# include "WiFiManager.h"  
#endif



//
// WiFi
//
#include <ESP8266WiFi.h>

//
// HTTP
//
#include <ESP8266HTTPClient.h>

//
// NTP uses UDP.
//
#include <WiFiUdp.h>

//
// For driving the LCD
//
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//
// For dealing with NTP & the clock.
//
#include "TimeLib.h"









//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);


//
// This two-dimensional array holds the text that we're
// going to display on the LCD.
//
// The first line will ALWAYS be the time/date.
//
// Any additional lines will contain the departure
// time of the next tram(s).
//
char screen[NUM_ROWS][NUM_COLS];


//
// Set the LCD address to 0x27, and define the number
// of rows & columns.
//
LiquidCrystal_I2C lcd(0x27, NUM_COLS, NUM_ROWS);

//
// UDP-socket & local-port for replies.
//
WiFiUDP Udp;
unsigned int localPort = 2390;

//
// The NTP-server we use.
//
static const char ntpServerName[] = "time.nist.gov";


//
// The tram stop to monitor.
//
static char tramID[10] = {'\0' };



//
// This function is called when the device is powered-on.
//
void setup()
{
    // Enable our serial port.
    Serial.begin(115200);

    // initialize the LCD
    lcd.begin();

    // Turn on the blacklight
    lcd.setBacklight(true);

#ifdef WIFI_MANAGER

    WiFiManager wifiManager;
    wifiManager.autoConnect("TRAM-TIMES" );
    
#else
    //
    // Connect to the WiFi network, and set a sane
    // hostname so we can ping it by name.
    //
    WiFi.mode(WIFI_STA);
    WiFi.hostname("tram-clock");
    WiFi.begin(WIFI_SSID, WIFI_PASS);


    //
    // Show that we're connecting to the WiFi.
    //
    lcd.setCursor(0, 0);
    lcd.print("WiFi connecting");
    lcd.setCursor(0, 1);

    //
    // Try to connect to WiFi, constantly.
    //
    while (WiFi.status() != WL_CONNECTED)
    {
        lcd.print(".");
        DEBUG_LOG(".");
        delay(500);
    }

    //
    // Now we're connected show the local IP address.
    //
    lcd.setCursor(0, 0);
    lcd.print("WiFi connected  ");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    // Allow this to be visible.
    delay(5000);
#endif


    //
    // Configure our tram stop
    //
    strcpy(tramID, TRAM_STOP );

    //
    // Ensure our UDP port is listening for receiving NTP-replies
    //
    Udp.begin(localPort);


    //
    // But now we're done, so we'll setup the clock.
    //
    // We provide the time via NTP, and resync every five minutes
    // which is excessive, but harmless.
    //
    setSyncProvider(getNtpTime);
    setSyncInterval(300);


    //
    // Finally we start our HTTP server
    //
    server.begin();
    DEBUG_LOG("Server started\n");
}





//
// This function is called continously, and is responsible
// for performing updates to the screen, polling the remote
// HTTP-server for our tram-data, and also running a HTTP-server
// of its own.
//
void loop()
{
    static bool backlight = true;

    // Keep the previous time, to avoid needless re-draws
    static char prev_time[NUM_COLS] = { '\0'};

    //
    // Get the current time.
    //
    // We save this so that we can test if we need to update the
    // tram-time, which we do every two minutes.
    //
    time_t t = now();
    int hur  = hour(t);
    int min  = minute(t);
    int sec  = second(t);

    //
    // Format the time & date in the first row.
    //
    snprintf(screen[0], NUM_COLS, "%02d:%02d:%02d %2d %s", hur, min, sec, day(t), monthShortStr(month(t)));

    //
    // Every two minutes we'll update the departure time
    // of the next tram.
    //
    // We also do it immediately the first time we're run,
    // when there is no pending time available.
    //
    if (((min % 2 == 0) && (sec == 0)) ||
            (strlen(screen[1]) == 0))
        update_tram_time();


    //
    // Now draw all the rows - correctly doing this
    // after the previous step might have updated
    // the display.
    //
    // That avoids showing outdated information, albeit
    // information that is only outdated for <1 second!
    //
    // NOTE: We delay() for less than a second in this function
    // so we cheat here, only updating the LCD if the currently
    // formatted time is different to that we set in the past.
    //
    // i.e. We don't re-draw the screen unless the time has changed.
    //
    // If the real-time hasn't changed then the tram-arrival times
    // haven't changed either, since that only updates every two minutes..
    //
    if (strcmp(prev_time, screen[0]) != 0)
    {
        for (int i = 0; i < NUM_ROWS; i++)
        {
            lcd.setCursor(0, i);
            lcd.print(screen[i]);
        }

        strcpy(prev_time, screen[0]);
    }
    else
    {
        DEBUG_LOG("Skipped update of LCD\n");
    }

    //
    // Check if a client has connected to our HTTP-server.
    //
    WiFiClient client = server.available();

    // If so.
    if (client)
    {
        // Wait until the client sends some data
        while (client.connected() && !client.available())
            delay(1);

        // Read the first line of the request
        String request = client.readStringUntil('\r');
        client.flush();

        // Turn on the backlight?
        if (request.indexOf("/ON") != -1)
        {
            backlight = true;
            lcd.setBacklight(true);
        }

        // Turn off the backlight?
        if (request.indexOf("/OFF") != -1)
        {
            backlight = false;
            lcd.setBacklight(false);
        }

        // Change the tram-stop?
        if (request.indexOf("/?stop=" ) != -1)
        {
           char *pattern = "/?stop=";
           char *s = strstr( request.c_str(), pattern );
           if ( s != NULL )
           {
           
              // Empty the tram ID
              memset( tramID, '\0', sizeof(tramID));
           
             // Skip past the pattern.
             s += strlen( pattern );
             
             // Add characters until we come to a terminating character.
             for (int i = 0; i < strlen(s); i++)
             {
                if ( (s[i] != ' ') && ( s[i] != '&' )  )
                  tramID[strlen(tramID)] = s[i];
                else
                   break;
             }

             // So we've changed the tram ID we should refresh the date.
             update_tram_time();
           }
        }

        // Return a simple response
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("");
        client.println("<!DOCTYPE HTML>");
        client.println("<html><head><title>Tram Times</title></head><body>");

        client.println("<h1>Tram Times</h1>");
        client.print("<table><tr><th>Screen Output</th></tr>" );
        
        // Now show the calculated tram-times
        for (int i = 0; i < NUM_ROWS; i++)
        {
            client.print("<tr><td>");
            client.print(screen[i]);
            client.print("</td></tr>");
        }

        client.println("</table>");
        

        // Showing the state.
        if (backlight)
            client.println("<p>Backlight is ON, <a href=\"/OFF\">turn off</a>.</p>");
        else
            client.println("<p>Backlight OFF, <a href=\"/ON\">turn on</a>.</p>");

        // Show a form
        client.println("<h4>Change Tram Stop</h4><blockquote>" );
        client.print( "<form action=\"/\" method=\"GET\"><input type=\"text\" name=\"stop\" value=\"");
        client.print( tramID );
        client.print( "\"><input type=\"submit\" value=\"Update\"></form>" );
        client.println("</blockquote>");
        client.println("</body></html>");
        delay(10);
    }

    //
    // Now sleep, to avoid updating our LCD too often.
    //
    delay(250);
}


//
// Update the given line of the display, from the
// specified JSON object.
//
void update_line_from_json(int line, char *txt)
{
    //
    // The time will either be "XX min", or "12:34" so we're
    // going to say ten bytes is enough.
    //
    char tm[10] = { '\0' };

    //
    //  The "line" will be "10", "4", "7", or similar.
    //
    //  Again ten bytes should be enough for everybody.
    //
    char ln[10] = { '\0' };

    //
    // We're looking for whatever is between:
    //
    //      "line": "
    // and
    //
    //      "
    //
    // (i.e. The tram-line.)
    //
    char *line_patt  = "\"line\": \"";
    char *line_start = strstr(txt, line_patt);

    if (line_start != NULL)
    {
        //  Move past the pattern itself.
        line_start += strlen(line_patt);

        // Add characters until we come to that terminating quote.
        for (int i = 0; i < strlen(line_start); i++)
        {
            if (line_start[i] != '\"')
                ln[strlen(ln)] = line_start[i];
            else
                break;
        }

    }

    //
    // Same again, except we're looking for "time" this time.
    //
    char *time_patt  = "\"time\": \"";
    char *time_start = strstr(txt, time_patt);

    if (time_start != NULL)
    {
        // Move past the pattern itself.
        time_start += strlen(time_patt);

        // Add characters until we come to that terminating quote.
        for (int i = 0; i < strlen(time_start); i++)
        {
            if (time_start[i] != '\"')
                tm[strlen(tm)] = time_start[i];
            else
                break;
        }

    }

    // Format the entry neatly.
    if (strlen(ln) && strlen(tm))
        snprintf(screen[line], NUM_COLS, "Tram %s @ %s", ln, tm);
    else
        snprintf(screen[line], NUM_COLS, "Failed to parse");

}



//
// Call our HTTP-service and retrieve the tram time(s).
//
// This function will update the global "screen" array
// with the departure time of the next tram(s).
//
void update_tram_time()
{

    DEBUG_LOG("Making request for tram details \n");
    DEBUG_LOG( "Tram stop is " );
    DEBUG_LOG( tramID );
    DEBUG_LOG( "\n" );

    
    char url[128];
    snprintf( url, sizeof(url)-1, "%s%s", TRAM_BASE, tramID );

    HTTPClient http;
    http.begin(url);

    // httpCode will be negative on error
    int httpCode = http.GET();

    if (httpCode > 0)
    {

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {

            // Get the body of the response.
            // This uses some hellish String object.
            String payload = http.getString();

            // Convert to C-string, because we're adults
            const char *input = payload.c_str();

            //
            // The line we're writing in our scren-array
            //
            // (Line 0 contains the time/date, so we start at line 1)
            //
            int line = 1;

            //
            // Start of JSON object-entry.
            //
            int start = -1;

            //
            // Process each part of the string looking for
            // regions deliminated by "{" + "}".
            //
            // Each such entry is an object, and from that
            // we look for the "time" field.
            //
            for (int i = 0; i < strlen(input); i++)
            {
                // Start of object.
                if (input[i] == '{')
                    start = i;

                // End of object.
                if ((input[i] == '}') && (start >= 0))
                {
                    //
                    // We've found a complete object entry, save
                    // that as a string.
                    //
                    char tmp[255];
                    memset(tmp, '\0', sizeof(tmp));
                    strncpy(tmp, input + start, i - start + 1);

                    // Are we still in the bounds of our LCD?
                    if (line < NUM_ROWS)
                    {
                        // If so we can populate the line of the LCD-data
                        // with the parsed details from this line.
                        update_line_from_json(line, tmp);

                        // Log to the serial console
                        DEBUG_LOG("Entry ");
                        DEBUG_LOG(line);
                        DEBUG_LOG(" is ");
                        DEBUG_LOG(screen[line]);
                        DEBUG_LOG("\n");
                    }

                    //
                    // And we move on to the next line.
                    //
                    line++;


                    // Reset for finding the start of the next JSON-object.
                    start = -1;
                }
            }
        }
        else
        {
            DEBUG_LOG("HTTP-fetch failed: ");
            DEBUG_LOG(http.errorToString(httpCode).c_str());
            DEBUG_LOG("\n");
        }

        http.end();
    }

}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
    IPAddress ntpServerIP;

    // discard any previously received packets
    while (Udp.parsePacket() > 0) ;

    DEBUG_LOG("Initiating NTP sync\n");

    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);

    DEBUG_LOG(ntpServerName);
    DEBUG_LOG(" -> ");
    DEBUG_LOG(ntpServerIP);
    DEBUG_LOG("\n");

    sendNTPpacket(ntpServerIP);

    delay(50);
    uint32_t beginWait = millis();

    while ((millis() - beginWait) < 5000)
    {
        DEBUG_LOG("#");
        int size = Udp.parsePacket();

        if (size >= NTP_PACKET_SIZE)
        {

            DEBUG_LOG("Received NTP Response\n");
            Udp.read(packetBuffer, NTP_PACKET_SIZE);

            unsigned long secsSince1900;

            // convert four bytes starting at location 40 to a long integer
            secsSince1900 = (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];

            // Now convert to the real time.
            unsigned long now = secsSince1900 - 2208988800UL;

#ifdef TIME_ZONE
            DEBUG_LOG("Adjusting time : ");
            DEBUG_LOG(TIME_ZONE);
            DEBUG_LOG("\n");

            now += (TIME_ZONE * SECS_PER_HOUR);
#endif

            return (now);
        }

        delay(50);
    }

    DEBUG_LOG("NTP-sync failed\n");
    return 0;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision

    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}
