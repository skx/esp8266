# Pixel Art

This project is a self-contained little toy for displaying pixel-art
upon an 8x8 LED matrix.

The WeMos Mini D1 drives the matrix, and you can control what is displayed
upon it wirelessly

# How it Works

The ESP8266 device will boot up, and receive an IP address.

When accessed via HTTP two files are served:

* http://192.168.10.51/index.html
   * This requests /app.js
* http://192.168.10.51/app.js
   * This is javascript magic.

In addition to this you can set the pixels by making a request such as this:

* http://192.168.10.51/?data=128,128,1,1,1,1,128

This will set the display to show a simple shape.

The javascript magic allows you to set/clear pixels with your left/right
mouse-buttons, and the state of those pixels will be displayed in real-time
on the matrix

# Credits

The pixel-editor is a hacked copy of that from here:

* https://github.com/carbontwelve/pixel-editor-tutorial.git

Specifically I took `step three` and added the ability to load preset-images,
as well as the AJAX POSTing when the pixels change.
