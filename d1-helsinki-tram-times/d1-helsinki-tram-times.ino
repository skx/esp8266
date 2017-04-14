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
//   Each additional line of the display will show the departure
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
//   Because the Helsinki Tram API has recently changed we've
// started polling from a site that I control:
//
//   https://steve.fi/Helsinki/Tram-API/
//
//   Steve
//   --
//


//
// The name of this project.
//
// Used for:
//   Access-Point name, in config-mode
//   OTA name.
//
#define PROJECT_NAME "TRAM-TIMES"



//
// The number of rows/columns of our display.
//
#define NUM_ROWS 4
#define NUM_COLS 20

//
// The URL to poll
//
#define TRAM_HOST "steve.fi"
#define TRAM_PATH "/Helsinki/Tram-API/api.cgi?id="

//
// The default tram stop to monitor.
//
#define DEFAULT_TRAM_STOP "1160404"

//
// The timezone - comment out to stay at GMT.
//
#define TIME_ZONE (+3)

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
#include "WiFiManager.h"

//
// We read/write the tram-stop to flash, along
// with the timezone offset.
//
#include <FS.h>


//
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

//
// HTTP server & SSL-fetching.
//
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>


//
// NTP uses UDP.
//
#include <WiFiUdp.h>

//
// For driving the LCD
//
#include <Wire.h>
#include "LiquidCrystal_I2C.h"

//
// For dealing with NTP & the clock.
//
#include "TimeLib.h"









//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);


//
// The tram stop we're going to display.
//
char tram_stop[12] = { '\0' };

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
static const char ntpServerName[] = "pool.ntp.org";


//
// Is the backlight lit?  Defaults to true.
//
bool backlight = true;



//
// This function is called when the device is powered-on.
//
void setup()
{
    // Enable our serial port.
    Serial.begin(115200);

    //
    // Enable access to the filesystem.
    //
    SPIFFS.begin();

    //
    // Load the tram-stop if we can
    //
    File f = SPIFFS.open("/tram.stop", "r");

    while (f.available())
    {

        //
        // We'll process the data to make sure we only
        // use 0-9 in our tram-stop.  This is mostly a
        // paranoia check to make sure we don't try
        // to cope with "\r", "\n", etc.
        //
        String line = f.readStringUntil('\n');
        const char *data = line.c_str();
        int len = strlen(data);

        //
        // Empty the value.
        //
        memset(tram_stop, '\0', sizeof(tram_stop));

        // Append suitable characters.
        for (int i = 0; i < len; i++)
        {
            if (data[i] >= '0' && data[i] <= '9')
                tram_stop[strlen(tram_stop)] = data[i];
        }
    }

    f.close();


    //
    // Did we fail to load the tram-stop?  If so use
    // the default.
    //
    if (strlen(tram_stop) == 0)
        strcpy(tram_stop, DEFAULT_TRAM_STOP);

    // initialize the LCD
    lcd.begin();

    // Turn on the blacklight
    lcd.setBacklight(true);

    lcd.setCursor(0, 0);
    lcd.print("Booting up ..");

    //
    // Handle Connection.
    //
    WiFiManager wifiManager;
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    lcd.setCursor(0, 0);
    lcd.print("WiFi connected  ");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    // Allow this to be visible.
    delay(2500);

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
    // Now we can start our HTTP server
    //
    server.begin();
    DEBUG_LOG("HTTP-Server started\n");

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
        lcd.setCursor(0, 0);
        lcd.print("OTA Start");
    });
    ArduinoOTA.onEnd([]()
    {
        lcd.setCursor(0, 0);
        lcd.print("OTA Start          ");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[16];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%          ", (progress / (total / 100)));
        lcd.setCursor(0, 0);
        lcd.print(buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        lcd.setCursor(0, 0);
        Serial.printf("Error[%u]: ", error);

        if (error == OTA_AUTH_ERROR)
            lcd.print("Auth Failed          ");
        else if (error == OTA_BEGIN_ERROR)
            lcd.print("Begin Failed          ");
        else if (error == OTA_CONNECT_ERROR)
            lcd.print("Connect Failed          ");
        else if (error == OTA_RECEIVE_ERROR)
            lcd.print("Receive Failed          ");
        else if (error == OTA_END_ERROR)
            lcd.print("End Failed          ");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();
}





//
// This function is called continously, and is responsible
// for performing updates to the screen, polling the remote
// HTTP-server for our tram-data, and also running a HTTP-server
// of its own.
//
void loop()
{

    // Keep the previous time, to avoid needless re-draws
    static char prev_time[NUM_COLS] = { '\0'};

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

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
        fetch_tram_times();


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

            // Padding?
            int extra = NUM_COLS - strlen(screen[i]);

            while (extra > 0)
            {
                lcd.print(" ");
                extra --;
            }
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
        if (request.indexOf("/?stop=") != -1)
        {
            char *pattern = "/?stop=";
            char *s = strstr(request.c_str(), pattern);

            if (s != NULL)
            {

                // Empty the tram ID
                memset(tram_stop, '\0', sizeof(tram_stop));

                // Skip past the pattern.
                s += strlen(pattern);

                // Add characters until we come to a terminating character.
                for (int i = 0; i < strlen(s); i++)
                {
                    if ((s[i] != ' ') && (s[i] != '&'))
                        tram_stop[strlen(tram_stop)] = s[i];
                    else
                        break;
                }

                // Write the updated value to flash
                File f = SPIFFS.open("/tram.stop", "w");

                if (f)
                {
                    DEBUG_LOG("Writing to /tram.stop: ");
                    DEBUG_LOG(tram_stop);
                    DEBUG_LOG("\n");

                    f.println(tram_stop);
                    f.close();
                }

                // So we've changed the tram ID we should refresh the date.
                fetch_tram_times();
            }
        }

        // Return a simple response
        serveHTML(client);
        delay(10);
    }

    //
    // Now sleep, to avoid updating our LCD too often.
    //
    delay(250);
}


void  update_tram_times(const char *txt)
{
    char* pch = NULL;

    int line = 1;
    pch = strtok((char *)txt, "\r\n");

    while (pch != NULL)
    {
        //
        // If we got a line, and it is at least ten characters
        // long then it is probably valid.
        //
        // The line will be:
        //
        //  STOP,TIME,NAME
        //
        if ((strlen(pch) > 10) && (line < NUM_ROWS))
        {

            //
            // Skip newlines / carriage returns
            //
            while (pch[0] == '\n' || pch[0] == '\r')
                pch += 1;

            DEBUG_LOG("LINE: '");
            DEBUG_LOG(pch);
            DEBUG_LOG("'\n");

            //
            // The time & line-number.
            //
            char tm[6] = { '\0' };
            char ln[3] = { '\0' };

            //
            // This is sleazy, but efficient.
            //
            ln[0] = pch[0];
            ln[1] = pch[1];
            ln[2] = '\0';

            //
            // This is sleazy, but efficient.
            //
            tm[0] = pch[3];  // 1
            tm[1] = pch[4];  // 2
            tm[2] = pch[5];  // :
            tm[3] = pch[6];  // 3
            tm[4] = pch[7];  // 4
            tm[5] = '\0';

            //
            // Format into our screen-buffer
            //
            snprintf(screen[line], NUM_COLS - 1, "  Tram %s @ %s", ln, tm);

            DEBUG_LOG("Generated line ");
            DEBUG_LOG(line);
            DEBUG_LOG(" '");
            DEBUG_LOG(screen[line]);
            DEBUG_LOG("'\n");

            //
            // Bump to the next display-line
            //
            line += 1;
        }

        pch = strtok(NULL, "\r\n");
    }
}



String fetchURL(const char *host, const char *path)
{
    WiFiClientSecure client;

    String headers = "";
    String body = "";
    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    bool gotResponse = false;
    long now;


    if (client.connect(host, 443))
    {
        DEBUG_LOG("connected to host ");
        DEBUG_LOG(host);
        DEBUG_LOG("\n");

        client.print("GET ");
        client.print(path);
        client.println(" HTTP/1.1");

        client.print("Host: ");
        client.println(host);
        client.println("User-Agent: arduino/1.0");
        client.println("");

        now = millis();

        // checking the timeout
        while (millis() - now < 3000)
        {
            while (client.available())
            {
                char c = client.read();

                if (finishedHeaders)
                {
                    body = body + c;
                }
                else
                {
                    if (currentLineIsBlank && c == '\n')
                    {
                        finishedHeaders = true;
                    }
                    else
                    {
                        headers = headers + c;
                    }
                }

                if (c == '\n')
                {
                    currentLineIsBlank = true;
                }
                else if (c != '\r')
                {
                    currentLineIsBlank = false;
                }

                //marking we got a response
                gotResponse = true;

            }

            if (gotResponse)
                break;
        }
    }

    return (body);
}




//
// Call our HTTP-service and retrieve the tram time(s).
//
// This function will update the global "screen" array
// with the departure time of the next tram(s).
//
void fetch_tram_times()
{
    DEBUG_LOG("Making request for tram details \n");
    DEBUG_LOG("Tram stop is ");
    DEBUG_LOG(tram_stop);
    DEBUG_LOG("\n");

    char url[128];
    snprintf(url, sizeof(url) - 1, "%s%s", TRAM_PATH, tram_stop);

    String body = fetchURL(TRAM_HOST, url);
    update_tram_times(body.c_str());
}


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
    IPAddress ntpServerIP;

    // Show what we're doing on the last row.
    lcd.setCursor(0, NUM_ROWS - 1);
    lcd.print("Syncing date & time");

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

    // delay(50);
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


void serveHTML(WiFiClient client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");

    client.println("<!DOCTYPE html>");
    client.println("<html lang=\"en\">");
    client.println("<head>");
    client.println("<title>Tram Times</title>");
    client.println("<meta charset=\"utf-8\">");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    client.println("<link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u\" crossorigin=\"anonymous\">");
    client.println("<script src=\"//code.jquery.com/jquery-1.12.4.min.js\"></script>");
    client.println("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\" integrity=\"sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa\" crossorigin=\"anonymous\"></script>");
    client.println("</head>");
    client.println("<body>");
    client.println("<nav id=\"nav\" class = \"navbar navbar-default\" style=\"padding-left:50px; padding-right:50px;\">");
    client.println("<div class = \"navbar-header\">");
    client.println("<h1 class=\"banner\"><a href=\"/\">Tram Times</a> - <small>by Steve</small></h1>");
    client.println("</div>");
    client.println("<ul class=\"nav navbar-nav navbar-right\">");
    client.println("<li><a href=\"https://steve.fi/Hardware/\">Steve's Projects</a></li>");
    client.println("</ul>");
    client.println("</nav>");
    client.println("<div class=\"container-fluid\">");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"><h1>Tram Times</h1><p>&nbsp;</p></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");
    client.println("<table class=\"table table-striped table-hover table-condensed table-bordered\">");

    // Now show the calculated tram-times
    for (int i = 0; i < NUM_ROWS; i++)
    {
        client.print("<tr><td>");
        client.print(screen[i]);
        client.print("</td></tr>");
    }

    client.println("</table>");
    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>Backlight</h2><p>&nbsp;</p></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");

    // Showing the state.
    if (backlight)
        client.println("<p>Backlight is ON, <a href=\"/OFF\">turn off</a>.</p>");
    else
        client.println("<p>Backlight OFF, <a href=\"/ON\">turn on</a>.</p>");

    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>Change Tram Stop</h2><p>&nbsp;</p></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");
    client.print("<p>You are currently monitoring the tram-stop with ID <a href=\"https://beta.reittiopas.fi/pysakit/HSL:");
    client.print(tram_stop);
    client.print("\">");
    client.print(tram_stop);
    client.print("</a>, but you can change that:</p>");
    client.println("<form action=\"/\" method=\"GET\"><input type=\"text\" name=\"stop\" value=\"");
    client.print(tram_stop);
    client.println("\"><input type=\"submit\" value=\"Update\"></form>");
    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");
    client.println("</div>");
    client.println("<p>&nbsp;</p>");
    client.println("</body>");
    client.println("</html>");

}
