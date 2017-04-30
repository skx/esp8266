//
//   Temperature/Humidity Reporter - https://steve.fi/Hardware/
//
//   This program will connect to your WiFi network, serving as
//  an access-port if no valid credentials are found, then submit
//  temp/humidity values to a local CGI-script.
//
//   Steve
//   --
//

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>


//
// DHT11 / DHT22 sensor interface
//
#include "dht.h"

//
// The pin we're connecting the sensor to
//
#define DHTPIN D4

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
// Include the MQQ library, and define our server.
//
#include "PubSubClient.h"
#include "info.h"
const char* mqtt_server = "192.168.10.64";
WiFiClient espClient;
PubSubClient client(espClient);
info board_info;


//
// The name of this project.
//
// Used for:
//   Access-Point name, in config-mode
//   OTA name.
//
#define PROJECT_NAME "D1-TEMP"


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
// Measure the temperature + humidity.
//
// Post to remote CGI-script AND publish in MQ.
//
void measureDHT()
{

    //
    // Make a reading
    //
    dht DHT;

    int chk = DHT.read22(DHTPIN);

    //
    // If the reading succeeded
    //
    if (chk == DHTLIB_OK)
    {
        // DISPLAY DATA
        Serial.print("Humidity:");
        Serial.print(DHT.humidity, 1);
        Serial.print(" Temperature:");
        Serial.print(DHT.temperature, 1);
        Serial.println();

        // Send it away
        String payload = "{\"temperature\":" + String(DHT.temperature) +
                         ",\"humidity\":" + String(DHT.humidity) +
                         ",\"mac\":\"" + board_info.mac() + "\"}";

        // Publish
        client.publish("temperature", payload.c_str());
        return;

    }
    else
    {
        // SHow the error
        switch (chk)
        {
        case DHTLIB_OK:
            Serial.println("OK");
            break;

        case DHTLIB_ERROR_CHECKSUM:
            Serial.println("Checksum error.");
            break;

        case DHTLIB_ERROR_TIMEOUT:
            Serial.println("Time out error.");
            break;

        case DHTLIB_ERROR_CONNECT:
            Serial.println("Connect error");
            break;

        case DHTLIB_ERROR_ACK_L:
            Serial.println("Ack Low error.");
            break;

        case DHTLIB_ERROR_ACK_H:
            Serial.println("Ack High error.");
            break;

        default:
            Serial.println("Unknown error");
            break;
        }

        return;
    }

}


//
// Called once, when the device boots.
//
void setup()
{
    //
    // Setup the serial-console.
    //
    Serial.begin(115200);

    //
    // Handle WiFi setup
    //
#ifdef WIFI_MANAGER

    WiFi.hostname(PROJECT_NAME);
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


#endif
    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("\nWiFi connected ");
    DEBUG_LOG(WiFi.localIP());
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

    //
    // Setup our pub-sub connection.
    //
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}


//
// Called constantly.
//
// Read the value of the temperature-sensor, and submit it, once a minute.
//
void loop()
{
    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

    //
    // The time we last read the temperature
    //
    static long last_read = 0;

    //
    // Ensure we're connected to our queue.
    //
    if (!client.connected())
        reconnect();

    //
    // Handle queue messages.
    //
    client.loop();

    // Get the current time.
    long now = millis();

    // If we've not updated, or it was >60 seconds, then update
    if ((last_read == 0) || (abs(now - last_read) > 60 * 1000))
    {

        measureDHT();
        last_read = now;
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
        Serial.print("Attempting MQTT connection...");

        String id = PROJECT_NAME;
        id += board_info.mac();

        // Attempt to connect
        if (client.connect(id.c_str()))
        {
            // We've connected
            Serial.println("connected");

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
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }

}
