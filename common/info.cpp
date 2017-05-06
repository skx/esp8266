
#include <ESP8266WiFi.h>

#include "info.h"

/////////////////////////////////////////////////////
//
// PUBLIC
//

String info::mac()
{
    uint8_t mac_array[6];
    static char tmp[20];

    WiFi.macAddress(mac_array);
    snprintf(tmp, sizeof(tmp) - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_array[0],
             mac_array[1],
             mac_array[2],
             mac_array[3],
             mac_array[4],
             mac_array[5]);

    return (String(tmp));
}

String info::ip()
{
    IPAddress ip = WiFi.localIP();
    return (ip.toString());
}

String info::id()
{
    return (String(ESP.getChipId(), HEX));
}

String info::hostname()
{
    return (String(WiFi.hostname()));
}

int info::flash()
{
    return (ESP.getFlashChipSize());
}


String info::to_JSON()
{

    String payload = "{";
    payload += "\"hostname\":\"" + hostname()  + "\"";
    payload += String(",");
    payload += "\"ip\":\"" + ip() + "\"";
    payload += ",";
    payload += "\"id\":\"" + id() + "\"";
    payload += ",";
    payload += "\"mac\":\"" + mac() + "\"";
    payload += ",";
    payload += "\"flash\":" + String(flash());
    payload += "}";

    Serial.println(payload);
    return (payload);
}
