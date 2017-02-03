# NTP-Based Clock

This project combines an ESP8266 processor with a 4x7-segment display, such that
we can operate as an NTP-backed clock.

The code is self-contained, and described on this project homepage:

* https://steve.fi/Hardware/d1-ntp-clock/


# Configuration

Once compiled and uploaded the project will attempt to connect to your
local WiFi network.  If no access details have been stored previously
it will instead function as an access-point named `NTP-CLOCK`.

Connect to this access-point with your mobile, or other device, and
you can select which network it should auto-join in the future.

> (This "connect or configure" behaviour is implemented by the excellent [WiFiManager](https://github.com/tzapu/WiFiManager) class.)
