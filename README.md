# ESP8266-Thermostat

IoT thermostat using an ESP8266-12E and a DHT 22.  Access web server with a browser to change thermostat settings.

Requires the DHT library included with the project this is forked from.  Not sure if you can install it a different way, but it works fine once you install it into your libraries directory.

I trimmed out all the cloud-oriented stuff like Google charts and the flash-write-heavy statistics stuff.  I also moved from a heat on / heat off temperature scheme to a more conventional "target temp" with a built in skew allowing the furnace to run a bit longer before turning itself off.

I may include a RRDtool based poller and graph generator for you to run on a home server that calls some of the ajax endpoints like `/cur_temp`, `/heat_status`, etc so you can see the pretty graphs without uploading your data to the death star.