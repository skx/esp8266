d1-alarm
--------

This project is akin to a door-bell:

* A simple device with a button triggers a sound-effect on a desktop PC.


Transmitter
-----------

The ESP8266 is wired up with a simple button, when the button is
pressed an event is sent through the MQ bus.  The MQ server will
default to 10.0.0.10, but you can change that by pointing your
browser at the IP-address of the alarm.


Receiver
--------

There is a simple Perl script which will listen for the MQ message
that the button transmits.  When it receives it it will invoke
"`alarm.sh`", which is a script local to my environment for playing
the `alarm.mp3` file.
