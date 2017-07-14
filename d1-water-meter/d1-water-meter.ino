#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>

//
// The access-point functionality
//
#include "WiFiManager.h"

//
// Debug messages over the serial console.
//
#include "debug.h"


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
#define PROJECT_NAME "D1-WATER-METER"





//
// Count of rotations the sensor has seen
//
volatile int NbTopsFan;

//
// This is called by the interrupt-handler, and bumps the count
// of rotations we've seen.
//
void rpm()
{
    NbTopsFan++;
}



//
// The setup() method runs once, when the sketch starts
//
void setup() //
{
    Serial.begin(115200);


    //
    // Handle WiFi setup
    //
    WiFi.hostname(PROJECT_NAME);
    WiFiManager wifiManager;
    wifiManager.autoConnect(PROJECT_NAME);

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
    DEBUG_LOG("HTTP-Server started on http://%s/\n",
              WiFi.localIP().toString().c_str());

    // Count rotations via D2
    pinMode(D2, INPUT);
    attachInterrupt(D2, rpm, RISING);

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


void loop()
{
    //
    // Record the last time we read the flow-rate
    //
    static long long last_read = 0;

    //
    // Handle any pending over the air updates.
    //
    ArduinoOTA.handle();

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
    // Get the current time.
    //
    long now = millis();

    //
    // If we've not updated, or it was >5 seconds, then update
    //
    if ((last_read == 0) || (abs(now - last_read) > 5 * 1000))
    {
        measure_water();
        last_read = now;
    }
}


//
// Measure the flow of water we can see for a one
// second period.
//
// This is called every 5 seconds, or so.
//
void measure_water()
{

    //
    // Reset ourselves and wait for a second to see
    // how many times the sensor records a rotation
    //
    NbTopsFan = 0;
    sei();
    delay(1000);
    cli();

    //
    // Now calculate the flow-rate
    //
    int Calc = (NbTopsFan * 60 / 7.5);

    //
    // The JSON we publish.
    //
    String payload = "{\"flow\":" + String(Calc) + ",\"mac\":\"" + board_info.mac() + "\"}";

    //
    // Log it
    //
    DEBUG_LOG("Sending data to MQ: %s\n", payload.c_str());

    //
    // Publish it to the bus
    //
    client.publish("water", payload.c_str());
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
            // We've connected
            DEBUG_LOG(" connected\n");

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
            DEBUG_LOG(" failed, rc=");
            DEBUG_LOG(client.state());

            DEBUG_LOG("\ntrying again in 5 seconds\n");

            // Wait 5 seconds before retrying
            delay(5000);
        }
    }

}
