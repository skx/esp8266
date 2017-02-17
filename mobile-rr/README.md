# Rick Roll Access Point

This repository contains code for building a Rick-Roll access-point.

The repository consists of three distinct projects, which have been
merged together to allow building in a standalone fashion:

* The [original mobile-rr project](https://github.com/idolpx/mobile-rr)
* [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
   * License GNU Lesser General Public License 2+
* [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
   * License GNU Lesser General Public License 2+


## Building & Installation

Build the project, and transfer to your D1, as per any other project.

This will deploy the code, but it will __not__ deploy the media.

This project sends large files over HTTP, and those files are extracted
and served from the on-board flash.  So you need to upload them after
you've built the project.

(The two steps are unrelated, you can rebuild the project multiple times
or re-upload the media multiple times.  They use separate storage areas
on the device.)

## Flash Upload

You can use the PlatformIO suit, as per the instructions in the [original repository](https://github.com/idolpx/mobile-rr), but the far simpler approach is to use the Arduino IDE.

Install the [arduino-esp8266fs-plugin](https://github.com/esp8266/arduino-esp8266fs-plugin), according to the instructions and restart your Arduino IDE.

Once installed you'll find a new menu-item:

* `Tools | ESP8266 Sketch Data Upload`

Click that and you'll find the contents of the `data/` sub-directory are automatically uploaded to your devices flash storage area.



## Changes

Changes have been made to allow this project to compile in a standalone fashion:

* Changed include-paths from `#include <foo.h>` to `#include "foo.h"`.
