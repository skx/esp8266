//
// Internet-Button - https://steve.fi/Hardware/
//
// This project is a simple hardware button which will connect to your
// WiFi-network and allow an action to be triggered via the button being
// pressed.  We support two distinct button-press events:
//
//  * A short press (+release).
//  * A long press (+release).
//
// To allow flexible handling the button-presses trigger sending a message
// to an MQ bus, with a payload like so:
//
//   {"click":"short","mac":"AA:BB:CC:DD:EE:FF"}
//   {"click":"long","mac":"AA:BB:CC:DD:EE:FF"}
//
// ("mac" is obviously the MAC-address of the board.)
//
// Physically we just have a button wired between D0 & D8.  In the future
// it is possible to imagine multiple buttons :)
//
//
// Steve
// --
//



//
// The name of this project.
//
// Used for the Access-Point name, and for over the air updates.
//
#define PROJECT_NAME "D1-MQ-ALARM"



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
// WiFi & over the air updates
//
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>


//
// The button handler
//
#include "OneButton.h"


//
// Include the MQQ library, and define our server.
//
#include "PubSubClient.h"
#include "info.h"

//
// End-point for sending messages to.
//
const char* mqtt_server = "10.0.0.10";

//
// Create the MQ-client.
//
WiFiClient espClient;
PubSubClient client(espClient);

//
// Utility class for dumping board-information.
//
info board_info;

//
// Setup a new OneButton on pin D8.
//
OneButton button(D8, false);

//
// Are there pending clicks to process?
//
volatile bool short_click = false;
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
    // Handle WiFi setup.
    //
    WiFiManager wifiManager;
    wifiManager.setAPCallback(access_point_callback);
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("WiFi Connected : ");
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
        DEBUG_LOG("OTA Start");
    });
    ArduinoOTA.onEnd([]()
    {
        DEBUG_LOG("OTA Ended");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[32];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%", (progress / (total / 100)));
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
    button.attachLongPressStop(on_long_click);

    //
    // Setup our pub-sub connection.
    //
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}



//
// Record that a short-click happened.
//
void on_short_click()
{
    short_click = true;
}

//
// Record that a long-click happened.
//
void on_long_click()
{
    long_click = true;
}


//
// If we're not configured with WiFi login details we run an access-point.
//
// Show that, explicitly.
//
void access_point_callback(WiFiManager* myWiFiManager)
{
    DEBUG_LOG("AccessPoint Mode");
    DEBUG_LOG(PROJECT_NAME);
    DEBUG_LOG("\n");
}



//
// This function is called continuously.
//
void loop()
{
    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

    //
    // Process the input-button
    //
    button.tick();

    //
    // Ensure we're connected to our queue.
    //
    if (!client.connected())
        reconnect();

    //
    // Handle queue messages.
    //
    client.loop();

    //
    // Handle any pending clicks here.
    //
    handlePendingButtons();

    //
    // Now have a little rest.
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

        // Send it away
        String payload = "{\"click\":\"short\",\"mac\":\"" + board_info.mac() + "\"}";
        client.publish("alarm", payload.c_str());

    }

    //
    // If we have a pending long-click then handle it.
    //
    if (long_click)
    {
        long_click = false;
        DEBUG_LOG("Long Click\n");

        // Send it away
        String payload = "{\"click\":\"long\",\"mac\":\"" + board_info.mac() + "\"}";
        client.publish("alarm", payload.c_str());
    }
}



//
// This is called when messages are received.
//
// We only subscribe to the `meta`-topic, and we don't do
// anything with the messages.  It's just a nice example
// showing how we could if we wanted to.
//
//
void callback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message arrived [Topic:");
    Serial.print(topic);
    Serial.print("] ");

    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }

    Serial.println();
}




//
// Reconnect to the pub-sub-server, if we're dropped.
//
void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        DEBUG_LOG("Attempting MQTT connection...");

        String id = PROJECT_NAME;
        id += board_info.mac();

        // Attempt to connect
        if (client.connect(id.c_str()))
        {
            //
            // We've connected
            //
            DEBUG_LOG("connected\n");

            //
            // Dump all our local details to the meta-topic
            //
            client.publish("meta", board_info.to_JSON().c_str());

            //
            // Subscribe to the `meta`-topic.
            //
            client.subscribe("meta");
        }
        else
        {
            DEBUG_LOG("failed, rc=");
            DEBUG_LOG(client.state());
            DEBUG_LOG(" try again in 5 seconds\n");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
