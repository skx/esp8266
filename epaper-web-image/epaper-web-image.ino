//
// Alternate between displaying two images, fetched from a remote URL.
//
// This script is designed to display a pre-processed image upon a 4.2"
// epaper, and is based upon the writeup located here:
//
//  https://steve.fi/Hardware/d1-epaper/
//
// To use this you'll need to:
//
//   1.  Find a monochrome image which is 400x300 pixels in size.
//
//   2.  Process it into a series of lines, via the "export.pl" script.
//
//   3.  Upload the result to a remote HTTP-server.
//
//   4.  Point this script at the URL to that file.
//
//   5.  Profit.
//
// Steve
// --
//



//
// ePaper library.
//
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.cpp>      // 4.2" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>


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
// Headers for SSL/MAC access.
//
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>


//
// Debug messages over the serial console.
//
#include "debug.h"


//
// The name of this project.
//
// Used for the Access-Point name, and for OTA-identification.
//
#define PROJECT_NAME "EPAPER-IMAGE"


//
// The helper & object for the epaper display.
//
GxIO_Class io(SPI, SS, 0, 2);
GxEPD_Class display(io);


//
// Setup, which is called once.
//
void setup()
{
    //
    // Setup serial-console & the display.
    //
    Serial.begin(115200);
    display.init();

    //
    // Handle the WiFi connection.
    //
    WiFiManager wifiManager;
    wifiManager.setAPCallback(access_point_callback);
    wifiManager.autoConnect(PROJECT_NAME);
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
// Fetch and display the image specified at the given URL
//
void display_url(const char * m_path)
{
    //
    // Clear the display.
    //
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    //
    // The host we're going to fetch from.
    //
    char m_host[] = "plain.steve.fi";

    char m_tmp[50] = { '\0' };
    memset(m_tmp, '\0', sizeof(m_tmp));

    /*
     * Create a HTTP client-object.
     */
    WiFiClient m_client;
    int port = 80;

    //
    // Keep track of where we are.
    //
    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    String m_body = "";
    long now;

    DEBUG_LOG("About to make the connection to '%s:%d' for %s\n", m_host, port, m_path);

    //
    // Connect to the remote host, and send the HTTP request.
    //
    if (!m_client.connect(m_host, port))
    {
        DEBUG_LOG("Failed to connect");
        return;
    }

    DEBUG_LOG("Making HTTP-request\n");

    m_client.print("GET ");
    m_client.print(m_path);
    m_client.println(" HTTP/1.0");
    m_client.print("Host: ");
    m_client.println(m_host);
    m_client.println("User-Agent: epaper-web-image/1.0");
    m_client.println("Connection: close");
    m_client.println("");

    DEBUG_LOG("Made HTTP-request\n");

    now = millis();

    //
    // Test for timeout waiting for the first byte.
    //
    while (m_client.available() == 0)
    {
        if (millis() - now > 15000)
        {
            Serial.println(">>> Client Timeout !");
            m_client.stop();
            return;
        }
    }

    int l = 0;

    //
    // Now we hope we'll have smooth-sailing, and we'll
    // read a single character until we've got it all
    //
    while (m_client.connected())
    {
      if(m_client.available())
      {
        char c = m_client.read();

        if (finishedHeaders)
        {
            //
            // Here we're reading the body of the response.
            //
            // We keep appending characters, but we don't want to
            // store the whole damn response in a string, because
            // that would eat all our RAM.
            //
            // Instead we read each character until we find a newline
            //
            // Once we find a newline we process that input, then
            // we keep reading more.
            //
            if (c == '\n' && strlen(m_tmp) > 5)
            {
                l += 1;

                //
                // Here we've read a complete line.
                //
                // The line will be of the form "x,y,X,Y", so we
                // need to parse that into four integers, and then
                // draw the appropriate line on our display.
                //
                int line[5] = { 0 };
                int i = 0;


                //
                // Parse into values.
                //
                char* ptr = strtok(m_tmp, ",");

                while (ptr != NULL && i < 4)
                {
                    // create next part
                    line[i] = atoi(ptr);
                    i++;
                    ptr = strtok(NULL, ",");
                }


                //
                // Draw the line.
                //
                display.drawLine(line[1], line[0], line[3], line[2], GxEPD_BLACK);

                //
                // Now we nuke the memory so that we can read
                // another line.
                //
                memset(m_tmp, '\0', sizeof(m_tmp));
            }
            else
            {
                // TODO - buffer-overflow.
                m_tmp[strlen(m_tmp)] = c;
            }
        }
        else
        {
            if (currentLineIsBlank && c == '\n')
                finishedHeaders = true;
        }

        if (c == '\n')
            currentLineIsBlank = true;
        else if (c != '\r')
            currentLineIsBlank = false;

      }
    }

    DEBUG_LOG("Nothing more is available - terminating");
    m_client.stop();
    DEBUG_LOG("Processed %d lines", l);


    //
    // Trigger the update of the display.
    //
    display.update();

}


void loop()
{
    char *one = "/Hardware/d1-epaper/knot.dat";
    char *two = "/Hardware/d1-epaper/skull.dat";
    static int i = 0;

    //
    // Have we displayed the image?
    //
    static bool show = false;

    //
    // The time we last displayed the image.
    static long displayed = 0;

    //
    // If we've not shown the image, then do so.
    //
    if (show == false)
    {
        //
        // Fetch / display
        //
        if (i == 0)
        {
            display_url(one);
            i = 1;
        }
        else
        {
            display_url(two);
            i = 0;
        }

        //
        // And never again.
        //
        show = true;
        displayed = millis();
    }


    //
    // Update the image 60 seconds.
    //
    // Do this by pretending we've never updated, even though we have.
    //
    if (millis() - displayed > (60 * 1000))
        show = false;


}
