Weather-Station
===============

This is a weather station created with components from Sparkfun. The Arduino firmware is a modified version of Sparkfun's Wimp Weather Station. While the Imp is an interesting device, I wanted to get the weather data back to a BeagleBone Black for further processing/evaluation. The BeagleBone Black can easily push the data to Weather Underground. 

Equipment
_________

To set up your station, please follow the [Sparkfun tutorial](https://learn.sparkfun.com/tutorials/weather-station-wirelessly-connected-to-wunderground). Obviously, you won't need the Imp or the Imp shield. In its place, you will need an XBee shield, an XBee breakout (with 1mm headers) and and two XBees. I am using XBee Pro modules due to the distance and other factors. You will need *three* sets of Arduino headers (one for the XBee shield and two for the weather shield) because the weather shield is so tall. So that we can turn the XBee off and on to conserve power, you will need a connection between the XBee pin 9 and digital pin 4 on the RedBoard.

I am using a BeagleBone Black Rev. C (a Rev. B board would work as well if you have one) running [Debian](http://beagleboard.org/latest-images/). You will need to have your BeagleBone Black connected to the internet (wired or wireless). On the P9 header of the BeagleBone Black, connect pin 1 to ground on the XBee breakout, pin 3 to VCC on the XBee breakout, pin 24 to DIN on the XBee breakout, and pin 26 to DOUT on the XBee breakout.

To house the electronics, I 3-d printed a Stevenson Screen. I modified one from [Thingiverse](http://www.thingiverse.com/thing:83969). I have included the .STL files for printing and the OpenScad file for the bottom so that you may modify it for your own mounting system. The Stevenson screen is printed in white PLA. It remains to be seen how durable the PLA will be. You build the assembly with a top, a middle, a middle grid, two more middles, and a bottom. The battery and Sunny Buddy sit on top of the middle_grid. The bottom has holes to mount the RedBoard and the two shields.
For now, I have the solar panel mounted in a manner similar to the Sparkfun Wimp Weather Station. I hope to have a design for a gimbal driven by two servos to track the sun in the near future. There should still be enough memory on the RedBoard for this.

Libraries
_________
You will need the [xbee-arduino library](https://code.google.com/p/xbee-arduino/) as well as the pressure and humidity sensor libraries from Sparkfun. 
For the BeagleBone Black, you will need (in addition to libraries bonescript, socketio, and serial installed by default) [xbee-api](https://www.npmjs.org/package/xbee-api) and [moment](https://www.npmjs.org/package/moment).
