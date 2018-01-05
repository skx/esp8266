# epaper-web-image

This directory contains code to retrieve a collection of image-data from
a remote HTTP-site and display it upon a 4.2" epaper display.


## How it works

The code works indirectly, rather than making a simple request for a JPG, a PNG,
or some other known-image format it instead fetches a file that contains drawing
data.

To display a specific image you need to process it, to create a data-file.

The supplied files beneath [util/](util/) do that.  In the example we
fetch the following two URLs:

* http://plain.steve.fi//Hardware/d1-epaper/knot.dat
  * Which corresponds to the image http://plain.steve.fi//Hardware/d1-epaper/knot.png
* http://plain.steve.fi//Hardware/d1-epaper/skull.dat
  * Which corresponds to the image http://plain.steve.fi//Hardware/d1-epaper/skull.png

These were each created via the `utils/export` script, for example:

    ./utils/export ./knot.jpg > knot.dat

You can transform in the opposite direction via:

    ./utils/import ./knot.dat
    display knot.dat-out.png


## Required Libraries

To make this example complete you'll need to install two libraries:

* https://github.com/ZinggJM/GxEPD
* https://github.com/adafruit/Adafruit-GFX-Library

Installation instructions are outside the scope of this document.


## Further Reading

For details of wiring, and an overview of the whole process please see the
following writeup:

* https://steve.fi/Hardware/d1-epaper/

Steve
--
