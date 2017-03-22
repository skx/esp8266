//
// Pixel Editor - https://steve.fi/Hardware/
//
// This project allows a user to use their browser as a
// pixel-editor.
//
// The changes they make will be made in real-time
// on an 8x8 Matrix.
//


#include <FS.h>
#include <Wire.h>

#include "Adafruit_GFX.h"
#include "Adafruit_LEDBackpack.h"



//
// The name of this project.
//
// Used for:
//   Access-Point name, in config-mode
//   OTA name.
//
#define PROJECT_NAME "D1-PIXELS"



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
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>


//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);

//
// The matrix-display itself
//
Adafruit_8x8matrix matrix = Adafruit_8x8matrix();


//
// Given a request-string such as : 1,2,3,4,5
//
// Populate an eight-line array with the data, and
// use that data to draw on the LED Matrix.
//
void light_leds(char *txt)
{
    DEBUG_LOG("INPUT:");
    DEBUG_LOG(txt);
    DEBUG_LOG("\n");

    uint8_t  pattern[8];

    // Ensure the pattern is OK
    for (int i = 0; i < 8; i++)
        pattern[i] = 0;

    // Current line.
    int line = 0;

    // Copy of the data - strtok modifies it.
    char *data = strdup(txt);

    // Look for the ","
    char *pch = strtok(data, ",");

    while ((pch != NULL) && (line < 8))
    {
        DEBUG_LOG(" Line ");
        DEBUG_LOG(line);
        DEBUG_LOG(" is ");
        DEBUG_LOG(pch);
        DEBUG_LOG("\n");

        pattern[line] = atoi(pch);

        line += 1;
        pch = strtok(NULL, ",");
    }

    // Free the copy of the data
    free(data);

    // Show the data on the display
    matrix.clear();
    matrix.drawBitmap(0, 0, pattern, 8, 8, LED_ON);
    matrix.writeDisplay();

}



//
// This function is called when the device is powered-on.
//
void setup()
{
    Serial.begin(115200);

    //
    // Initialize the SPIFFS filesystem.
    //
    SPIFFS.begin();

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
    // Try to connect to WiFi, constantly.
    //
    while (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_LOG(".");
        delay(500);
    }

#endif

    DEBUG_LOG("WiFi connected  ");
    DEBUG_LOG(WiFi.localIP());
    DEBUG_LOG("\n");

    //
    // Now we can start our HTTP server
    //
    server.begin();
    DEBUG_LOG("Server started\n");

    //
    // Eanble over-the-air updates
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
        DEBUG_LOG("\n");
    });
    ArduinoOTA.onEnd([]()
    {
        DEBUG_LOG("OTA End");
        DEBUG_LOG("\n");
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
        if (error == OTA_AUTH_ERROR)
            DEBUG_LOG("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            DEBUG_LOG("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            DEBUG_LOG("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            DEBUG_LOG("Receive Failed");
        else if (error == OTA_END_ERROR)
            DEBUG_LOG("End Failed");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();

    //
    // Setup our LED Matrix.
    //
    matrix.begin(0x70);
    matrix.setBrightness(9);
    matrix.clear();

}


//
// This function is called continously, and handles a few
// different things:
//
//  * Over-The-Air updates
//
//  * Serving our application over HTTP.
//
//  * Updating the LED matrix
//
void loop()
{

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();


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

        // Change the LED pattern?
        if (request.indexOf("/?data=") != -1)
        {
            char *pattern = "/?data=";
            char *s = strstr(request.c_str(), pattern);

            if (s != NULL)
                light_leds(s + strlen("/?data="));

            // Return a simple response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("");
            client.println("OK");
        }
        else if (request.indexOf("/app.js") != -1)
        {
            //
            // Serve our Application.
            //
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/javascript");
            client.println("");

            File dataFile = SPIFFS.open("/app.js", "r");

            if (dataFile)
            {

                while (dataFile.available())
                {
                    //Lets read line by line from the file
                    String line = dataFile.readStringUntil('\n');
                    client.println(line);
                }

                dataFile.close();
            }
        }
        else
        {
            //  Serve /index.html
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("");

            File dataFile = SPIFFS.open("/index.html", "r");

            if (dataFile)
            {

                while (dataFile.available())
                {
                    //Lets read line by line from the file
                    String line = dataFile.readStringUntil('\n');
                    client.println(line);
                }

                dataFile.close();
            }
        }

        delay(10);
    }

}
