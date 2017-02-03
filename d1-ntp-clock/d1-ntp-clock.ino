//
//   NTP-Based Clock - https://steve.fi/Hardware/
//
//   This is a simple program which uses WiFi & an 4x7-segment display
//   to show the current time, complete with blinking ":".
//
//   Steve
//   --
//


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
// The display-interface
//
#include "TM1637.h"

//
// Time & NTP
//
#include "TimeLib.h"


//
// The name of this project.
//
// Used for:
//   Access-Point name, in config-mode
//   OTA name.
//
#define PROJECT_NAME "NTP-CLOCK"



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
// Use the user-friendly WiFI library?
//
// If you don't want to use this then comment out the following line:
//
#define WIFI_MANAGER

//
//  Otherwise define a SSID / Password
//
#ifdef WIFI_MANAGER
# include "WiFiManager.h"
#else
# define WIFI_SSID "SCOTLAND"
# define WIFI_PASS "highlander1"
#endif


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
// Pin definitions for TM1637 and can be changed to other ports
//
#define CLK D3
#define DIO D2
TM1637 tm1637(CLK, DIO);



//
// This function is called when the device is powered-on.
//
void setup()
{
    // Enable our serial port.
    Serial.begin(115200);

    // initialize the display
    tm1637.init();

    // We want to see ":" between the digits.
    tm1637.point(true);

    //
    // Set the intensity - valid choices include:
    //
    //   BRIGHT_DARKEST   = 0
    //   BRIGHT_TYPICAL   = 2
    //   BRIGHT_BRIGHTEST = 7
    //
    tm1637.set(BRIGHT_DARKEST);

    //
    // Handle WiFi setup
    //
#ifdef WIFI_MANAGER

    WiFiManager wifiManager;
    wifiManager.autoConnect(PROJECT_NAME);

#else
    //
    // Connect to the WiFi network, and set a sane
    // hostname so we can ping it by name.
    //
    WiFi.mode(WIFI_STA);
    WiFi.hostname(PROJECT_NAME);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    //
    // Show that we're connecting to the WiFi.
    //
    DEBUG_LOG("WiFi connecting: ");

    //
    // Try to connect to WiFi, constantly.
    //
    while (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_LOG(".");
        delay(500);
    }

    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("\nWiFi connected ");
    DEBUG_LOG(WiFi.localIP());
    DEBUG_LOG("\n");

#endif

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
        DEBUG_LOG("OTA End\n");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[16];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%          ", (progress / (total / 100)));
        DEBUG_LOG(buf);
        DEBUG_LOG("\n");
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        DEBUG_LOG("Error - ");
        DEBUG_LOG(error);
        DEBUG_LOG(" ");

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



//
// This function is called continously, and is responsible
// for flashing the ":", and otherwise updating the display.
//
// We rely on the background NTP-updates to actually make sure
// that that works.
//
void loop()
{
    static char buf[10] = { '\0' };
    static char prev[10] = { '\0' };
    static long last_read = 0;
    static bool flash = true;

    //
    // Get the current hour/min
    //
    time_t nw   = now();
    int cur_hour = hour(nw);
    int cur_min  = minute(nw);

    //
    // Format them in a useful way.
    //
    sprintf(buf, "%02d%02d", cur_hour, cur_min);

    //
    // If the current "hourmin" is different to
    // that we displayed last loop ..
    //
    if (strcmp(buf, prev) != 0)
    {
        // Update the display
        tm1637.display(0, buf[0] - '0');
        tm1637.display(1, buf[1] - '0');
        tm1637.display(2, buf[2] - '0');
        tm1637.display(3, buf[3] - '0');

        // And cache it
        strcpy(prev , buf);

    }


    //
    // The preceeding piece of code would
    // have ensured the display only updated
    // when the hour/min changed.
    //
    // However note that we nuke the cached
    // value every second - solely so we can
    // blink the ":".
    //
    //  Sigh

    long now = millis();

    if ((last_read == 0) ||
            (abs(now - last_read) > 500))
    {
        // Invert the "show :" flag
        flash = !flash;

        // Apply it.
        tm1637.point(flash);

        // However note that the ":" won't redraw
        // unless/until you update.  So we'll
        // force that to happen.
        memset(prev, '\0', sizeof(prev));
        last_read = now;
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
