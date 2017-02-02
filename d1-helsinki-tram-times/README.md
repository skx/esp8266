# Helsinki Tram Times

This project combines an ESP8266 processor with a LCD display, such that
it shows the next departures from a given Helsinki tram-stop.

The code is self-contained, and described on this project homepage:

* https://steve.fi/Hardware/helsinki-tram-times/


# Configuration

Once compiled and uploaded the project will attempt to connect to your
local WiFi network.  If no access details have been stored previously
it will instead function as an access-point named `TRAM-TIMES`.

Connect to this access-point with your mobile, or other device, and
you can select which network it should auto-join in the future.

(This behaviour is implemented by the excellent [WiFiManager](https://github.com/tzapu/WiFiManager) class.)

The only addition configuration required is to set the tram-stop to
view - by default it will show the departures of [Kytösuontie](https://hsl.trapeze.fi/omatpysakit/web?command=fullscreen2&stop=1160404).   To change
the tram-stop open the IP address in your browser and use the HTML-form
to submit the new ID.
