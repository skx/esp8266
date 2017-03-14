# Temperature / Humidity Reporting

This project uses a DHT 22 sensor to report the current temperature
every minute.

The information will be published to a MQQ / Mosquitto server on the
topic `temperature`.  Additionally the board will dump all its meta-info
to the topic `meta` on startup.

The meta-information includes:

* Hostname
* IP Address
* MAC Address
* etc.

## Configuration

You'll need to change the IP address of the MQQ server if you wish
to deploy this yourself.

## Wiring

The DHT22 is wired to pin D2.

# Extras

There is a simple Perl script included in `Record/` which will log
the received temperature/humidity in an SQLite database.

That can be viewed via the `View/view.cgi` CGI-script to present
graphs.
