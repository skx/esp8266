# Water-Flow

This project uses a hall-effect / flow-sensor to report the current volume
of water being received by my washing-machine.
every minute.

The information will be published to a MQQ / Mosquitto server on the
topic `water`.  Additionally the board will dump all its meta-info
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

The flow-sensor is wired to pin D2, which is hardwired.
