# Helsinki Tram Times

This project combines an ESP8266 processor with a LCD display, such that
it shows the next departures from a given Helsinki tram-stop.

> **NOTE**: In all this project we talk about "trams", but what we _actually_ display is departures from a given location.  If the location is a tram-stop you'll see tram-details, and if the location is a bus-stop you'll see bus-details.  (There are no stops in Helsinki which allow both trams _and_ busses!)

The code is self-contained, and is introduced on the project homepage:

* https://steve.fi/Hardware/helsinki-tram-times/

# Images

These images show the final result:

![Components in a case](images/case.fitted.jpg)
![Usual-view](images/tram.boxed.jpg)

This is the first time I've had anything 3d-printed, but the process was pretty simple, because somebody else designed the case and I could download that design for free!

* http://blog.steve.fi/3d_printing_is_cool.html


# Configuration

Once compiled and uploaded the project will attempt to connect to your
local WiFi network.  If no access details have been stored previously
it will instead function as an access-point named `TRAM-TIMES`.

Connect to this access-point with your mobile, or other device, and
you can select which network it should auto-join in the future.

> (This "connect or configure" behaviour is implemented by the excellent [WiFiManager](https://github.com/tzapu/WiFiManager) class.)

The only addition configuration which is required is the setup of the
stop to view - by default it will show the departures of the stop near my house:

* [Kytösuontie](https://beta.reittiopas.fi/pysakit/HSL:1160404).

To change the location which is displayed open the IP address of the device
in your browser and use the HTML-form to submit the new ID.  (Using the
map above will let you find IDs.)


## Remote API

The code in this project polls a remote end-point every two minutes, parsing
the data there and displaying it.

The remote URL returns the data as a set of lines in CSV format, with the following three fields:

* Identifier
  * (i.e. tram-route, or bus-route).
* Time
  * HH:MM:SS
* Destination
  * i.e. "Kirurgi", "Olympiaterminaali - Töölö - Länsi-Pasila", etc.

You can use the web-UI to change the URL which is polled to one of your
own choosing, allowing you to self-host things if you wish.  Or return
different data.


# Optional Button

If you wire a button between D0 & D8 you gain additional functionality:

* Short-Press the button to toggle the backlight.
* Double-click to show a brief information-page.
* Long-Press the button to resync the device:
  * Date/time.
  * Departure-data.
  * Temperature data (if appropriate).

The info-page looks like this:

![Info-view](images/tram.info.jpg)
