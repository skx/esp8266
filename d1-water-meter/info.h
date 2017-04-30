//
//    FILE: info.h
//  AUTHOR: Steve Kemp
// VERSION: 0.0.1
// PURPOSE: Get information.
//     URL: http://steve.fi/Hardware
//
// HISTORY:
// see dht.cpp file
//

#ifndef info_h
#define info_h



#if ARDUINO < 100
#include <WProgram.h>
#include <pins_arduino.h>  // fix for broken pre 1.0 version - TODO TEST
#else
#include <Arduino.h>
#endif

class info
{
public:
    info() {};

    /*
     * MAC address.
     */
    String mac();

    /*
     * The chip ID
     */
    String id();

    /*
     * The current IP address.
     */
    String ip();

    /*
     * Size of flash RAM in bytes.
     */
    int flash();

    /*
     * Speed of processor.
     */
    int speed();

    /*
     * The hostname
     */
    String hostname();

    /*
     * All information as a JSON array.
     */
    String to_JSON();
private:
};
#endif
//
// END OF FILE
//
