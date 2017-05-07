//
//   Helsinki Tram-Departure Display - https://steve.fi/Hardware/
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
//   of the next tram from the chosen stop.  For example we'd
//   we would see something like this on a 4x20 LCD display:
//
//      #######################
//      # 11:12:13 Fri 21 Apr #
//      #   Line 4B @ 11:17   #
//      #   Line 7B @ 11:35   #
//      #   Line 10 @ 11:45   #
//      #######################
//
//
//   There is a simple HTTP-server which is running on port 80, which
//   will mirror the LCD-contents, and allow changes to be made to the
//   device.
//
//   For example the user can change the time-zone offset, the tram-stop,
//   toggle the backlight, or even change the remote URL being polled.
//
//   The user-selectable-changes will be persisted to flash memory, so
//   they survive reboots.
//
//   Because the Helsinki Tram API is a complex GraphQL-bease we're getting
//   data via a simple helper:
//
//       https://steve.fi/Helsinki/Tram-API/
//
//
//  Optional Button
//  --------------
//
//    If you wire a button between D0 & D8 you can control the device
//    using it:
//
//      Single press & release
//         Toggle the backlight.
//
//      Double-Click
//         Show IP and other data.
//
//      Long-press & release
//         Resync time & departure-data.
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
#define PROJECT_NAME "TRAM-TIMES"


//
// The number of rows/columns of our display.
//
#define NUM_ROWS 4
#define NUM_COLS 20

//
// The URL to fetch our tram-data from.
//
// The string `__ID__` in this URL will be replaced by the ID of the stop.
//
#define DEFAULT_API_ENDPOINT "https://steve.fi/Helsinki/Tram-API/api.cgi?id=__ID__"

//
// The default tram stop to monitor.
//
#define DEFAULT_TRAM_STOP "1160404"

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
// We read/write the tram-stop to flash, along with the timezone offset.
//
#include <FS.h>


//
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

//
// For driving the LCD
//
#include <Wire.h>
#include "LiquidCrystal_I2C.h"


//
// For dealing with NTP & the clock.
//
#include "NTPClient.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


//
// The button handler
//
#include "OneButton.h"


//
// For fetching URLS
//
#include "url_fetcher.h"


//
// Timezone offset from GMT
//
int time_zone_offset = 0;


//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);


//
// The tram stop we're going to display.
//
char tram_stop[12] = { '\0' };

//
// The API end-point we poll for display-purposes
//
char api_end_point[256] = { '\0' };


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
// Is the backlight lit?  Defaults to true.
//
bool backlight = true;


//
// Setup a new OneButton on pin D8.
//
OneButton button(D8, false);


//
// Are there pending clicks to process?
//
volatile bool short_click = false;
volatile bool double_click = false;
volatile bool long_click = false;


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
    // Enable access to the filesystem.
    //
    SPIFFS.begin();

    //
    // Load the tram-stop if we can
    //
    String tram_stop_str = read_file("/tram.stop");

    if (tram_stop_str.length() > 0)
        strcpy(tram_stop, tram_stop_str.c_str());
    else
        strcpy(tram_stop, DEFAULT_TRAM_STOP);

    //
    // Load the API end-point if we can
    //
    String api_end_str = read_file("/tram.api");

    if (api_end_str.length() > 0)
        strcpy(api_end_point, api_end_str.c_str());
    else
        strcpy(api_end_point, DEFAULT_API_ENDPOINT);

    //
    // Load the time-zone offset, if we can
    //
    String tz_str = read_file("/time.zone");

    if (tz_str.length() > 0)
        time_zone_offset = tz_str.toInt();


    //
    // initialize the LCD
    //
    lcd.begin();
    lcd.setBacklight(true);

    //
    // Show a message.
    //
    draw_line(0, "Starting up ..");

    //
    // Handle Connection.
    //
    WiFiManager wifiManager;
    wifiManager.setAPCallback(access_point_callback);
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    draw_line(0, "WiFi Connected");
    draw_line(1, WiFi.localIP().toString().c_str());


    //
    // Allow the IP to be visible.
    //
    delay(2500);

    //
    // Ensure our NTP-client is ready.
    //
    timeClient.begin();

    //
    // Configure the callbacks.
    //
    timeClient.on_before_update( on_before_ntp );
    timeClient.on_after_update( on_after_ntp );

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
        draw_line(0, "OTA Start");
    });
    ArduinoOTA.onEnd([]()
    {
        draw_line(0, "OTA Ended");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[NUM_COLS + 1];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, NUM_COLS, "Upgrade - %02u%%", (progress / (total / 100)));
        draw_line(0, buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        if (error == OTA_AUTH_ERROR)
            draw_line(0, "Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            draw_line(0, "Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            draw_line(0, "Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            draw_line(0, "Receive Failed");
        else if (error == OTA_END_ERROR)
            draw_line(0, "End Failed");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();

    //
    // We have a switch between D8 & D0.
    //
    // If this is missing no harm will be done, it will just
    // never receive any clicks :)
    //
    pinMode(D8, INPUT_PULLUP);
    pinMode(D0, OUTPUT);
    digitalWrite(D0, HIGH);

    //
    // Configure the button-action(s).
    //
    button.attachClick(on_short_click);
    button.attachDoubleClick(on_double_click);
    button.attachLongPressStop(on_long_click);

}


//
// Record that a short-click happened.
//
void on_short_click()
{
    short_click = true;
}


//
// Record that a double-click happened.
//
void on_double_click()
{
    double_click = true;
}

//
// Record that a long-click happened.
//
void on_long_click()
{
    long_click = true;
}




//
// Called just before the date/time is updated via NTP
//
void on_before_ntp()
{
    draw_line(NUM_ROWS - 1, "Refreshing date & time");
}

//
// Called just after the date/time is updated via NTP
//
void on_after_ntp()
{
    draw_line(NUM_ROWS - 1, "Date & time updated");
}

//
// If we're unconfigured we run an access-point.
//
// Show that, explicitly.
//
void access_point_callback(WiFiManager* myWiFiManager)
{
    draw_line(0, "AccessPoint Mode");
    draw_line(1, PROJECT_NAME);
}



//
// This function is called continously.
//
void loop()
{
    //
    // Keep the previous time, to avoid needless re-draws
    //
    static char prev_time[NUM_COLS] = { '\0'};


    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

    //
    // Resync the clock?
    //
    timeClient.update();


    //
    // Process the button - looking for clicks, double-clicks,
    // and long-clicks
    //
    button.tick();

    //
    // Handle any pending clicks here.
    //
    handlePendingButtons();

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
    // Format the time & date in the first row.
    //
    //                               HH:MM:SS     $DAY $NUM $MON $YEAR
    snprintf(screen[0], NUM_COLS, "%02d:%02d:%02d %s %02d %s %04d", hour, min, sec, d_name.c_str(), day, m_name.c_str(), year);



    //
    // Every two minutes we'll update the departure times.
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
            draw_line(i, screen[i]);

        //
        // Keep the current time in our local/static buffer.
        //
        strcpy(prev_time, screen[0]);
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
    // Now sleep, to avoid updating our LCD too often.
    //
    delay(20);
}


//
// We bind our button such that short-clicks, long-clicks,
// and double-clicks will invoke a call-back.
//
// The callbacks just record the pending state, and here we
// process any of them that were raised.
//
void handlePendingButtons()
{

    //
    // If we have a pending-short-click then handle it
    //
    if (short_click)
    {
        short_click = false;
        DEBUG_LOG("Short Click\n");


        // Toggle the state of the backlight
        backlight = !backlight;
        lcd.setBacklight(backlight);
    }

    //
    // If we have a pending long-click then handle it.
    //
    if (long_click)
    {
        long_click = false;
        DEBUG_LOG("Long Click\n");

        // Update the date/time & tram-data.
        timeClient.forceUpdate();
        fetch_tram_times();
    }

    //
    // If we have a pending double-click then handle it
    //
    if (double_click)
    {
        double_click = false;
        DEBUG_LOG("Double Click\n");

        char line[NUM_COLS + 1];

        // Line 0 - About
        draw_line(0, "Steve Kemp - Trams");

        // Line 1 - IP
        snprintf(line, NUM_COLS, "IP: %s", WiFi.localIP().toString().c_str());
        draw_line(1, line);

        // Line 2 - Timezone
        if (time_zone_offset > 0)
            snprintf(line, NUM_COLS, "Timezone: +%d", time_zone_offset);
        else if (time_zone_offset < 0)
            snprintf(line, NUM_COLS, "Timezone: -%d", time_zone_offset);
        else
            snprintf(line, NUM_COLS, "Timezone: %d", time_zone_offset);

        draw_line(2, line);

        // Line 3 - Tram ID
        snprintf(line, NUM_COLS, "Tram ID : %s", tram_stop);
        draw_line(3, line);

        delay(2500);
    }


}




//
// Given a bunch of CSV, containing departures, we parse this and
// update the screen-array.
//
// We handle CSV of the form:
//
//    NNNNN,HH:MM:SS,Random-Text
//
// The first field is the tram/bus ID, the second is the time in
// full hour-minute-second format, and the third is the name of the
// route.
//
// We'll cope with a bus/tram-ID of up to five characters, and we'll
// keep the display neat by truncating the time at HH:MM.
//
// The name of the route is not displayed.
//
void update_tram_times(const char *txt)
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
        //   NN,HH:MM:SS,NAME
        //
        // So if we assume a two-digit ID such as "10", "7A", "4B",
        // and the six digits of the time, then ten is a reasonable
        // bound on the minimum-length of a valid-line.
        //
        if ((strlen(pch) > 10) && (line < NUM_ROWS))
        {

            //
            // Skip newlines / carriage returns
            //
            while (pch[0] == '\n' || pch[0] == '\r')
                pch += 1;

            //
            // Show what we're operating upon
            //
            DEBUG_LOG("LINE: '");
            DEBUG_LOG(pch);
            DEBUG_LOG("'\n");

            //
            // Look for the first comma, which seperates
            // the tram/bus-number and the time.
            //
            // We don't know how long that bus/tram ID
            // will be.  But we'll assume <=6 characters
            // later on.
            //
            char *comma = strchr(pch, ',');

            //
            // If we found a comma then proceed.
            //
            if (comma != NULL)
            {
                // ID of line, and time of departure.
                char id[6] = {'\0'};
                char tm[6] = {'\0'};

                //
                // So our line-ID is contained between pch & comma.
                //
                // Copy it, capping it if we need to at five characters.
                //
                memset(id, '\0', sizeof(id));

                strncpy(id, pch, (comma - pch) >= sizeof(id) ? sizeof(id) - 1 : (comma - pch));


                //
                // Now we have comma pointing to ",HH:MM:SS,DESCRIPTION-HERE"
                //
                // We want to extract the time, and save that away.
                //
                // If our time is HH:MM:SS then c + 9 will be a comma
                //
                // We will copy just the HH:MM part of the time, so five
                // digits in total.
                //
                if (comma[9] == ',')
                {
                    strncpy(tm, comma + 1 , 5);
                }

                //
                // Now we format the line of the screen with the data.
                //
                snprintf(screen[line], NUM_COLS - 1,
                         "  Line %s @ %s", id, tm);

            }

            // Show what we generated
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


//
// Call our HTTP-service and retrieve the tram time(s).
//
// This function will update the global "screen" array
// with the departure time of the next tram(s).
//
void fetch_tram_times()
{
    //
    // Show what we're doing on the last row.
    //
    draw_line(NUM_ROWS - 1, "Refreshing Trams ..");

    //
    // The URL we're going to fetch, replacing `__ID__` with
    // the ID of the tram.
    //
    String url(api_end_point);
    url.replace("__ID__", tram_stop);

    //
    // Show what we're going to do.
    //
    DEBUG_LOG("Fetching ");
    DEBUG_LOG(url);
    DEBUG_LOG("\n");

    //
    // Fetch the contents of the remote URL.
    //
    UrlFetcher client(url.c_str());

    //
    // If that succeeded.
    //
    int code = client.code();

    if (code == 200)
    {

        //
        // Parse the returned data and process it.
        //
        String body = client.body();

        if (body.length() > 0)
        {
            update_tram_times(body.c_str());
        }
        else
        {
            DEBUG_LOG("Empty body from HTTP-fetch");
        }
    }
    else
    {
        //
        // Log the status-code
        //
        DEBUG_LOG("HTTP-Request failed, status-Code was ");
        DEBUG_LOG(code);
        DEBUG_LOG("\n");

        //
        // Log the status-line
        //
        DEBUG_LOG("STATUS: ");
        DEBUG_LOG(client.status());
        DEBUG_LOG("\n");
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

    // Start of body
    // Row
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
        client.print("<tr><td><code>");
        client.print(screen[i]);
        client.print("</code></td></tr>");
    }

    client.println("</table>");
    client.println("</div>");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("</div>");

    // Row
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>Backlight</h2></div>");
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

    // Row
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>Change Tram Stop</h2></div>");
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


    // Row
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-3\"></div>");
    client.println("<div class=\"col-md-9\"> <h2>Change API End-Point</h2></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");
    client.print("<p>This device works by polling a remote API, to fetch tram-data.  You can change the end-point in the form below:</p>");
    client.println("<form action=\"/\" method=\"GET\"><input type=\"text\" name=\"api\" size=\"75\" value=\"");
    client.print(api_end_point);
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
// Draw a single line of text on the given row of our
// LCD.
//
// This is used to ensure that each line drawn is always
// padded with spaces.  This is required to avoid
// display artifacts if you draw:
//
//    THIS IS A LONG LINE
// then
//    HELLO
//
// (Which would show "HELLOIS A LONG LINE".)
//
void draw_line(int row, const char *txt)
{
    lcd.setCursor(0, row);
    lcd.print(txt);

    int extra = NUM_COLS - strlen(txt);

    while (extra > 0)
    {
        lcd.print(" ");
        extra--;
    }

}

//
// This is a horrid function.
//
void urldecode(const char *src, char *dst)
{
    char a, b;

    while (*src)
    {
        if ((*src == '%') &&
                ((a = src[1]) && (b = src[2])) &&
                (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
                a -= 'a' - 'A';

            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';

            if (b >= 'a')
                b -= 'a' - 'A';

            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';

            *dst++ = 16 * a + b;
            src += 3;
        }
        else
        {
            *dst++ = *src++;
        }
    }

    *dst++ = '\0';
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

    // Turn on the backlight?
    if (request.indexOf("/ON") != -1)
    {
        backlight = true;
        lcd.setBacklight(true);

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }

    // Turn off the backlight?
    if (request.indexOf("/OFF") != -1)
    {
        backlight = false;
        lcd.setBacklight(false);

        // Redirect to the server-root
        redirectIndex(client);
        return;
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

            // Record the data in a file.
            write_file("/tram.stop", tram_stop);

            // So we've changed the tram ID we should refresh the date.
            fetch_tram_times();
        }

        // Redirect to the server-root
        redirectIndex(client);
        return;
    }

    // Change the API end-point
    if (request.indexOf("/?api=") != -1)
    {
        char *pattern = "/?api=";
        char *s = strstr(request.c_str(), pattern);

        if (s != NULL)
        {
            char tmp[256] = { '\0' };

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

            // URL decode..
            urldecode(tmp, api_end_point);

            // Record the data in a file.
            write_file("/tram.api", api_end_point);

            // So we've changed the tram ID we should refresh the date.
            fetch_tram_times();
        }

        // Redirect to the server-root
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
