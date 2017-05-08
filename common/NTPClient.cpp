/**
 * The MIT License (MIT)
 * Copyright (c) 2015 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NTPClient.h"

#ifndef LEAP_YEAR
#  define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )
#endif


NTPClient::NTPClient(UDP& udp) {
  this->_udp            = &udp;
}

NTPClient::NTPClient(UDP& udp, int timeOffset) {
  this->_udp            = &udp;
  this->_timeOffset     = timeOffset;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName) {
  this->_udp            = &udp;
  this->_poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName, int timeOffset) {
  this->_udp            = &udp;
  this->_timeOffset     = timeOffset;
  this->_poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName, int timeOffset, unsigned long updateInterval) {
  this->_udp            = &udp;
  this->_timeOffset     = timeOffset;
  this->_poolServerName = poolServerName;
  this->_updateInterval = updateInterval;
}

void NTPClient::on_before_update( callbackFunction newFunction )
{
    this->on_before = newFunction;
}

void NTPClient::on_after_update( callbackFunction newFunction )
{
    this->on_after = newFunction;
}

void NTPClient::begin() {
  this->begin(NTP_DEFAULT_LOCAL_PORT);
}

void NTPClient::begin(int port) {
  this->_port = port;

  this->_udp->begin(this->_port);

  this->_udpSetup = true;
}

bool NTPClient::forceUpdate() {
  #ifdef DEBUG_NTPClient
    Serial.println("Update from NTP Server");
  #endif

  if ( on_before )
    on_before();

  this->sendNTPPacket();

  // Wait till data is there or timeout...
  byte timeout = 0;
  int cb = 0;
  do {
    delay ( 10 );
    cb = this->_udp->parsePacket();
    if (timeout > 100) return false; // timeout after 1000 ms
    timeout++;
  } while (cb == 0);

  this->_lastUpdate = millis() - (10 * (timeout + 1)); // Account for delay in reading the time

  this->_udp->read(this->_packetBuffer, NTP_PACKET_SIZE);

  unsigned long highWord = word(this->_packetBuffer[40], this->_packetBuffer[41]);
  unsigned long lowWord = word(this->_packetBuffer[42], this->_packetBuffer[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;

  this->_currentEpoc = secsSince1900 - SEVENZYYEARS;

  if ( on_after )
      on_after();

  return true;
}

bool NTPClient::update() {
  if ((millis() - this->_lastUpdate >= this->_updateInterval)     // Update after _updateInterval
    || this->_lastUpdate == 0) {                                // Update if there was no update yet.
    if (!this->_udpSetup) this->begin();                         // setup the UDP client if needed
    return this->forceUpdate();
  }
  return true;
}

unsigned long NTPClient::getEpochTime() {
  return this->_timeOffset + // User offset
         this->_currentEpoc + // Epoc returned by the NTP server
         ((millis() - this->_lastUpdate) / 1000); // Time since last update
}

int NTPClient::getDay() {
    parse_date_time();
    return(_data.Wday);
}
int NTPClient::getHours() {
    parse_date_time();
    return(_data.Hour);
}
int NTPClient::getMinutes() {
    parse_date_time();
    return(_data.Minute);
}
int NTPClient::getSeconds() {
    parse_date_time();
    return(_data.Second);
}

// The day of week.
String NTPClient::getWeekDay(bool abbreviated)
{
    const char *day_s[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    const char *day_l[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturaday" };

    parse_date_time();

    if (abbreviated)
        return( day_s[ _data.Wday ] );
    else
        return( day_l[ _data.Wday ] );
}

// The name of the month.
String NTPClient::getMonth(bool abbreviated)
{
    const char *mon_s[] = { "NOP", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char *mon_l[] = { "NOP", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    parse_date_time();

    if (abbreviated)
        return( mon_s[_data.Month] );
    else
        return( mon_l[_data.Month] );
}

// Get the year
int NTPClient::getYear() {
    parse_date_time();
    return(_data.Year);
}

// The day of the month.
int NTPClient::getDayOfMonth() {
    parse_date_time();
    return(_data.Day);
}

// Parse our date/time into a structure, which we can then use elsewhere.
time_data NTPClient::parse_date_time() {
    // Get epoch-time
    unsigned long rawTime = this->getEpochTime();

    // Get basics
    _data.Hour   = ((rawTime % 86400L) / 3600);
    _data.Minute = ((rawTime % 3600) / 60);
    _data.Second = (rawTime % 60);
    _data.Wday   = (((rawTime / 86400L) + 4 ) % 7);

    // Now set to be in-days.
    rawTime /= 86400L;

    // Setup.
    unsigned long days = 0, year = 1970;
    uint8_t month;
    static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};

    // Walk forward until we've found the number of years.
    while((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
        year++;

    // now it is days in this year, starting at 0
    rawTime -= days - (LEAP_YEAR(year) ? 366 : 365);
    days=0;

    // Count forward until we've run out of days.
    for (month=0; month<12; month++)
    {
        uint8_t monthLength;
        if (month==1) {
            // february
            monthLength = LEAP_YEAR(year) ? 29 : 28;
        } else {
            monthLength = monthDays[month];
        }
        if (rawTime < monthLength)
            break;
        rawTime -= monthLength;
    }

    /*
     * Store these final values.
     */
    _data.Month = month + 1;
    _data.Year = year;
    _data.Day  = rawTime + 1;

    return(_data);
}


String NTPClient::getFormattedTime() {

    char buf[128] = {'\0'};
    snprintf(buf, sizeof(buf)-1, "%02d:%02d:%02d - %s %02d/%02d/%04d" , _data.Hour, _data.Minute, _data.Second, this->getWeekDay().c_str(),_data.Day, _data.Month, _data.Year  );

    return(buf);
}

void NTPClient::end() {
  this->_udp->stop();

  this->_udpSetup = false;
}

void NTPClient::setTimeOffset(int timeOffset) {
  this->_timeOffset     = timeOffset;
}

void NTPClient::setUpdateInterval(unsigned long updateInterval) {
  this->_updateInterval = updateInterval;
}

void NTPClient::sendNTPPacket() {
  // set all bytes in the buffer to 0
  memset(this->_packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  this->_packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  this->_packetBuffer[1] = 0;     // Stratum, or type of clock
  this->_packetBuffer[2] = 6;     // Polling Interval
  this->_packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  this->_packetBuffer[12]  = 49;
  this->_packetBuffer[13]  = 0x4E;
  this->_packetBuffer[14]  = 49;
  this->_packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  this->_udp->beginPacket(this->_poolServerName, 123); //NTP requests are to port 123
  this->_udp->write(this->_packetBuffer, NTP_PACKET_SIZE);
  this->_udp->endPacket();
}
