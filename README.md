Arduino LIFX bulb
=================

LIFX bulb emulator by Kayne Richens (kayno@kayno.net)

Emulates a LIFX bulb. Connect an RGB LED (or LED strip via drivers) to redPin, greenPin and bluePin as you normally would on an ethernet-ready Arduino and control it from the LIFX app!

Notes:
- Currently you cannot control an Arduino bulb and real LIFX bulbs at the same time with the Android LIFX app. Untested with the iOS LIFX app.
- Only one client (e.g. app) can connect to the bulb at once

Setup:
- Connect an RGB LED to PWM pins on your ethernet-ready arduino
- Set the following variables in arduinolifx.ino to suit your Arduino and network environment:
-- mac (unique mac address for your arduino)
-- redPin (PWM pin for RED)
-- greenPin  (PWM pin for GREEN)
-- bluePin  (PWM pin for BLUE)
- Connect the arduino to power and network
- The LED should light up (warm white) once the arduino gets a DHCP lease from your network
- Start up the LIFX app - you should see a bulb named "Arduino bulb"
- Control your arduino bulb from the LIFX app!

Made possible by the work of magicmonkey: https://github.com/magicmonkey/lifxjs/ - you can use this to control your arduino bulb as well as real LIFX bulbs at the same time!

And also the RGBMood library by Harold Waterkeyn, which was modified slightly to support powering down the LED
