# ESP8266 Projects

This repository contains a small number of projects for use with/on the ESP8266 chip, as packaged by the WeMos Mini D1.


## Helsinki Tram Display

The [d1-helsinki-tram-times](d1-helsinki-tram-times) project shows the departure times of Trams in Helsinki, and is more fully documented [on the project homepage](https://steve.fi/Hardware/helsinki-tram-times/).

## NTP-Based Clock

The [d1-ntp-clock](d1-ntp-clock) does exactly what the name implies, operates as a clock getting the time via NTP.

The project is documented [on the project homepage](https://steve.fi/Hardware/d1-ntp-clock/).


## Pixel Editor

The [d1-pixels](d1-pixels) project presents a simple pixel-editor which can be used to update an 8x8 LED Matrix in real-time, via your browser.

The project is documented [on the project homepage](https://steve.fi/Hardware/d1-pixels/).



## Rick-Roll Access Point

The [mobile-rr](mobile-rr) project is an access-point & captive portal that pretends to offer free WiFi, but instead [rickrolls](https://en.wikipedia.org/wiki/Rickrolling) visitors.

* This is __not my project__, but was very interesting to read and cleanup.

## Temperature & Humidity Measurement

This [simple project](d1-temp) records the current temperature & humidity value via a DHT22 sensor, and submits to a local MQ bus.


## Weather Station

The [d1-weather-station](d1-weather-station) uses an OLED display to show the current date, time, weather-summary & three-day summary via remote HTTP fetches from wunderground.com.

* This is __not my project__, but was very interesting to read and cleanup.


## Water Flow Counting

The [d1-water-meter](d1-water-meter) project is a simple one that measures
the flow of water being sent to our washing-machine.  The data is published
on an MQ bus.
