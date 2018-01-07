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
// There is a default MQ address configured, 10.0.0.10, but you can
// point your web-browser at the device to change this.
//
//
// Steve
// --
//


//
// We read/write to SPIFSS.
//
#include <FS.h>


//
// WiFi setup.
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
// Include the MQQ library.
//
#include "PubSubClient.h"
#include "info.h"


//
// Debug messages over the serial console.
//
#include "debug.h"


//
// For handling URL-parameters
//
#include "url_parameters.h"


//
// The name of this project.
//
// Used for the Access-Point name, and for over the air updates.
//
#define PROJECT_NAME "D1-MQ-ALARM"


//
// The HTTP-server we present runs on port 80.
//
WiFiServer server(80);


//
// Address of our MQ queue
//
char mqtt_server[64] = { '\0' };


//
// The default server-address if nothing is configured.
//
#define DEFAULT_MQ_SERVER  "10.0.0.10"


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
#ifdef DEBUG
    Serial.begin(115200);
#endif

    //
    // Enable access to the filesystem.
    //
    SPIFFS.begin();

    //
    // Handle WiFi setup.
    //
    WiFiManager wifiManager;
    wifiManager.setAPCallback(access_point_callback);
    wifiManager.autoConnect(PROJECT_NAME);

    //
    // Now we're connected show the local IP address.
    //
    DEBUG_LOG("WiFi Connected as %s\n", WiFi.localIP().toString().c_str());

    //
    // Allow over the air updates
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
        DEBUG_LOG("OTA Ended\n");
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
        if (error == OTA_AUTH_ERROR)
            DEBUG_LOG("\tAuth Failed\n");
        else if (error == OTA_BEGIN_ERROR)
            DEBUG_LOG("\tBegin Failed\n");
        else if (error == OTA_CONNECT_ERROR)
            DEBUG_LOG("\tConnect Failed\n");
        else if (error == OTA_RECEIVE_ERROR)
            DEBUG_LOG("\tReceive Failed\n");
        else if (error == OTA_END_ERROR)
            DEBUG_LOG("\tEnd Failed\n");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();

    //
    // Start our HTTP server
    //
    server.begin();
    DEBUG_LOG("HTTP-Server started on http://%s/\n",
              WiFi.localIP().toString().c_str());

    //
    // Our switch is wired between D8 & D0.
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
    // Load the MQ address, if we can.
    //
    String mq_str = read_file("/mq.addr");

    //
    // If we did make it live, otherwise use the default value.
    //
    if (mq_str.length() > 0)
        strcpy(mqtt_server, mq_str.c_str());
    else
        strncpy(mqtt_server, DEFAULT_MQ_SERVER, sizeof(mqtt_server) - 1);

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
    if (client.connected())
        client.loop();

    //
    // Handle any pending clicks here.
    //
    handlePendingButtons();

    //
    // Check if a client has connected to our HTTP-server.
    //
    // If so handle it.
    //
    // (This allows changing MQ address.)
    //
    WiFiClient wclient = server.available();

    if (wclient)
        processHTTPRequest(wclient);


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
    //
    // Keep track of connection-attempts.
    //
    // If there are too many failures we'll just
    // abort.
    //
    static int count = 0;

    if (count > 5)
        return;

    //
    // Bump our attempt-count.
    //
    count += 1;

    // Attempt to reconnect
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

        //
        // We can stop counting failures now.
        //
        count = 0;
    }
    else
    {
        DEBUG_LOG("failed, rc=%d, delaying for 5 seconds\n", client.state());
        delay(5000);
    }
}



//
// Process an incoming HTTP-request.
//
void processHTTPRequest(WiFiClient httpclient)
{
    // Wait until the client sends some data
    while (httpclient.connected() && !httpclient.available())
        delay(1);


    // Read the first line of the request
    String request = httpclient.readStringUntil('\r');
    httpclient.flush();

    //
    // Find the URL we were requested
    //
    // We'll have something like "GET XXXXX HTTP/XX"
    // so we split at the space and send the "XXX HTTP/XX" value
    //
    request = request.substring(request.indexOf(" ")  + 1);

    //
    // Now we'll want to peel off any HTTP-parameters that might
    // be present, via our utility-helper.
    //
    URL url(request.c_str());

    //
    // Change the MQ server?
    //
    char *mq = url.param("mq");

    if (mq != NULL)
    {
        // Write the data to a file.
        write_file("/mq.addr", mq);

        // Update the queue address
        memset(mqtt_server, '\0', sizeof(mqtt_server));
        strncpy(mqtt_server, mq, sizeof(mqtt_server) - 1);

        // Force a reconnection
        client.disconnect();

        // Redirect to the server-root
        redirectIndex(httpclient);
        return;
    }


    // Return a simple response
    serveHTML(httpclient);

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
// Serve a HTML-page to any clients who connect, via a browser.
//
void serveHTML(WiFiClient client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");

    client.println("<!DOCTYPE html>");
    client.println("<html lang=\"en\">");
    client.println("<head>");
    client.println("<title>Alarm Button</title>");
    client.println("<meta charset=\"utf-8\">");
    client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    client.println("<link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u\" crossorigin=\"anonymous\">");
    client.println("<script src=\"https://code.jquery.com/jquery-1.12.4.min.js\" integrity=\"sha256-Qw82+bXyGq6MydymqBxNPYTaUXXq7c8v3CwiYwLLNXU=\" crossorigin=\"anonymous\"></script>");
    client.println("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\" integrity=\"sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa\" crossorigin=\"anonymous\"></script>");
    client.println("</head>");
    client.println("<body>");
    client.println("<nav id=\"nav\" class = \"navbar navbar-default\" style=\"padding-left:50px; padding-right:50px;\">");
    client.println("<div class = \"navbar-header\">");
    client.println("<h1 class=\"banner\"><a href=\"/\">Alarm Button</a> - <small>by Steve</small></h1>");
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
    client.println("<div class=\"col-md-9\"> <h2>Network Details</h2></div>");
    client.println("</div>");
    client.println("<div class=\"row\">");
    client.println("<div class=\"col-md-4\"></div>");
    client.println("<div class=\"col-md-4\">");
    client.print("<p>This device has the IP address <code>");
    client.print(WiFi.localIP());
    client.println("</code>, and is configured to send data to the following MQ server:</p>");
    client.println("<form action=\"/\" method=\"GET\"><input type=\"text\" name=\"mq\" value=\"");
    client.print(mqtt_server);
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
        DEBUG_LOG("Writing data '%s' to file '%s'\n", data, path);

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
