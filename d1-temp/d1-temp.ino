#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "dht.h"

#define DHTPIN D2

#define SERVER_ADDRESS "192.168.10.64"
#define SERVER_PORT 80

dht DHT;

float humidity = 0;
float temperature = 0;

const char* ssid = "SCOTLAND";
const char* password = "highlander1";

int sendData(String url, String payload)
{
    HTTPClient http;
    http.begin(SERVER_ADDRESS, SERVER_PORT, url);
    http.addHeader("Content-Type", "application/json");
    http.POST(payload);
    http.end();
}

void measureDHT()
{

    //
    // Make a reading
    //
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
        String payload = "{\"temperature\":" + String(DHT.temperature) + ",\"humidity\":" + String(DHT.humidity) + "}";
        sendData("/cgi-bin/temp.cgi", payload);

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

void setup()
{
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

}

void loop()
{
    measureDHT();
    delay(1000 * 60);
}
