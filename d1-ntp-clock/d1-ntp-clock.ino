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
// For dealing with NTP & the clock.
//
#include "NTPClient.h"

//
// The display-interface
//
#include "TM1637.h"


//
// WiFi setup.
//
#include "WiFiManager.h"


//
// Debug messages over the serial console.
//
#include "debug.h"


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
// NTP client, and UDP socket it uses.
//
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


//
// Pin definitions for TM1637 and can be changed to other ports
//
#define CLK D3
#define DIO D2
TM1637 tm1637(CLK, DIO);


//
// Called just before the date/time is updated via NTP
//
void on_before_ntp()
{
    DEBUG_LOG("Updating date & time");
}

//
// Called just after the date/time is updated via NTP
//
void on_after_ntp()
{
    DEBUG_LOG("Updated NTP client\n");
}

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
    WiFiManager wifiManager;
    wifiManager.autoConnect(PROJECT_NAME);


    //
    // Ensure our NTP-client is ready.
    //
    timeClient.begin();

    //
    // Configure the callbacks.
    //
    timeClient.on_before_update(on_before_ntp);
    timeClient.on_after_update(on_after_ntp);

    //
    // Setup the timezone & update-interval.
    //
    timeClient.setTimeOffset(TIME_ZONE * (60 * 60));
    timeClient.setUpdateInterval(300 * 1000);


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
        char buf[32];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%\n", (progress / (total / 100)));
        DEBUG_LOG(buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        DEBUG_LOG("Error - ");

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
    // Resync the clock?
    //
    timeClient.update();

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

    //
    // Get the current hour/min
    //
    int cur_hour = timeClient.getHours();
    int cur_min  = timeClient.getMinutes();

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
    // value every half-second - solely so we can
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

        //
        // Note that the ":" won't redraw unless/until you update.
        // So we'll force that to happen by removing the cached
        // value here.
        //
        memset(prev, '\0', sizeof(prev));
        last_read = now;
    }
}
