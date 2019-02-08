# ESP32 Web Server Test

Testing the functions of the ESP32 webserver with the intention to merge with some existing arduino control code for a beer fermentation chamber.

### Prerequisites

Install ESP32 board profile in Arudino IDE.
Install 1-Wire Library -
Install Dallas Temperature Library - 

Find 1-wire probe addresses -

### Installing

Connect 1-wire DS18B20's to pin 15 on the ESP32.
Change the 1-wire addresses to match your sensors.
Change the SSID and Password to match your home network credentials.

## Running The Software

Plug the ESP32 in to the USB port and open the serial port to find it's local IP address.
Type that IP Address in to a browser and the web server will be visible.
You can adjust the set temp up and down and read the temp sensors.

## Future Development

Write the set_temp variable to EEPROM so the set temp is remember when the power is turned off and on again.
Log the temperature readings, machine state and set temp to google sheets.
Have the browser/web server update live so you dont have to refresh the page manually.
Tidy up the UI and make it pretty.






