# Common Libraries

This directory contains some common code which is used in my projects:

## Libraries

* `NtpClient.*`
   * From  https://github.com/arduino-libraries/NTPClient
   * Extended to allow fetching names of days/months.
   * Extended to add callbacks:
      * One before updating.
      * One after updating.
* `OneButton.*`
   * From https://github.com/mathertel/OneButton
* `PubSubClient.*`
   * From https://github.com/knolleary/pubsubclient
* `WiFiManager.*`
   * From https://github.com/tzapu/WiFiManager

## My Code

* `info.*`
    * Fetches information about the current board.
* `url_fetcher.*`
    * Simple HTTP-client.
    * Supports `http://` and `https://`.
