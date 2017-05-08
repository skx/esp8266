#pragma once

#include "Arduino.h"

#include <Udp.h>

#define SEVENZYYEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337


extern "C" {
    /*
     * Signature for our callback functions(s)
     */
    typedef void (*callbackFunction)(void);

    /*
     * The time-data as a structure.
     */
    typedef struct  {
        int Second;
        int Minute;
        int Hour;
        int Wday;
        int Day;
        int Month;
        int Year;
    } time_data;
}


class NTPClient {
  private:
    UDP*          _udp;
    bool          _udpSetup       = false;

    const char*   _poolServerName = "time.nist.gov"; // Default time server
    int           _port           = NTP_DEFAULT_LOCAL_PORT;
    int           _timeOffset     = 0;

    unsigned long _updateInterval = 60000;  // In ms

    unsigned long _currentEpoc    = 0;      // In s
    unsigned long _lastUpdate     = 0;      // In ms

    byte          _packetBuffer[NTP_PACKET_SIZE];

    void          sendNTPPacket();

    /*
     * Callback handles.
     */
    callbackFunction on_before = NULL;
    callbackFunction on_after  = NULL;

    /*
     * The current time-data
     */
    time_data _data;

  public:
    NTPClient(UDP& udp);
    NTPClient(UDP& udp, int timeOffset);
    NTPClient(UDP& udp, const char* poolServerName);
    NTPClient(UDP& udp, const char* poolServerName, int timeOffset);
    NTPClient(UDP& udp, const char* poolServerName, int timeOffset, unsigned long updateInterval);

    /**
     * Invoke this user-function before we update.
     */
    void on_before_update(callbackFunction newFunction);

    /**
     * Invoke this user-function after we update.
     */
    void on_after_update(callbackFunction newFunction);

    /**
     * Starts the underlying UDP client with the default local port
     */
    void begin();

    /**
     * Starts the underlying UDP client with the specified local port
     */
    void begin(int port);

    /**
     * This should be called in the main loop of your application. By default an update from the NTP Server is only
     * made every 60 seconds. This can be configured in the NTPClient constructor.
     *
     * @return true on success, false on failure
     */
    bool update();

    /**
     * This will force the update from the NTP Server.
     *
     * @return true on success, false on failure
     */
    bool forceUpdate();

    // Day of week
    int getDay();

    // Hour / Minute / Seconds
    int getHours();
    int getMinutes();
    int getSeconds();

    // The day of the month
    int getDayOfMonth();

    // The month name, as a string.
    String getMonth( bool abbreviate = true);

    // The week-day, as a string.
    String getWeekDay(bool abbreviate = true);

    // The year.
    int getYear();

    /**
     * Return the time-data as a structure.
     */
    time_data parse_date_time();

    /**
     * Changes the time offset. Useful for changing timezones dynamically
     */
    void setTimeOffset(int timeOffset);

    /**
     * Set the update interval to another frequency. E.g. useful when the
     * timeOffset should not be set in the constructor
     */
    void setUpdateInterval(unsigned long updateInterval);

    /**
     * @return time formatted like `hh:mm:ss`
     */
    String getFormattedTime();

    /**
     * @return time in seconds since Jan. 1, 1970
     */
    unsigned long getEpochTime();

    /**
     * Stops the underlying UDP client
     */
    void end();
};
