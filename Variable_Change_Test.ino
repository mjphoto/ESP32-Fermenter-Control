//Testing webserver, temperature probe readings and changing a variable from the webserver on an
//ESP32 module. This code will be merged with some existing arduino code that controls a fermentation
//chamber once functions are proven

// Load Wi-Fi library
#include <WiFi.h>

//Load Temp Sensor Libraries
#include <OneWire.h>
#include <DallasTemperature.h>

//Load EEPROM Library
#include <EEPROM.h>

// define the number of bytes you want to access
#define EEPROM_SIZE 1

//Define temp probe pin
#define ONE_WIRE_BUS 15

// set up 1-wire probes
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress AIR_TEMP_SENSOR = {0x28, 0xFF, 0x16, 0x8D, 0x87, 0x16, 0x03, 0x50}; //Test sensor A
DeviceAddress VAT_TEMP_SENSOR = {0x28, 0xFF, 0x0A, 0x2E, 0x68, 0x14, 0x04, 0xA6}; //Test sensor B

//variables for temp sensors
float air_temp;
float vat_temp;

//set temp variables
float set_temp;
float new_set_temp;

//define state variables for testing
const int STATE_ERROR = -1;
const int STATE_RELAX = 0;
const int STATE_IDLE = 1;
const int STATE_COOL = 2;
const int STATE_HEAT = 3;
int state = STATE_COOL;

// Replace with your network credentials
//const char* ssid     = "AHRF455889";
//const char* password = "zxcv1597";
const char* ssid     = "vodafone513DFE";
const char* password = "12345678";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

void setup() {
  Serial.begin(115200);
  
  //Start EEPROM at defined size (above).
  EEPROM.begin(EEPROM_SIZE);

  // Set up temperature probes
  sensors.setResolution(AIR_TEMP_SENSOR, 11); //resolution of 0.125deg cels, 
  sensors.setResolution(VAT_TEMP_SENSOR, 11); //takes approx 375ms

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  
  //set up set temp variables here.
  if (EEPROM.read(0) != 255) {
    set_temp = EEPROM.read(0);
  }
  if (EEPROM.read(0) == 255) {
    set_temp = 19.0;
  }
  new_set_temp = set_temp;
}

void loop(){
  
  //Get temps
  sensors.requestTemperaturesByAddress(AIR_TEMP_SENSOR);
  sensors.requestTemperaturesByAddress(VAT_TEMP_SENSOR);
  vat_temp = sensors.getTempC(VAT_TEMP_SENSOR);
  air_temp = sensors.getTempC(AIR_TEMP_SENSOR);

  //run web server
  web_server();
}

void web_server(){
  
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


            //Set Temp Changes By Button
            if (header.indexOf("GET /set/up") >= 0) {
              Serial.println("Increase New Set Temperature by 0.5");
              new_set_temp = new_set_temp + 0.5;  
             }
            else if (header.indexOf("GET /set/down") >= 0) {
              Serial.println("Decrease New Set Temperature by 0.5");
              new_set_temp = new_set_temp - 0.5;           
             }
            //Submit button for changing set temp
            if (header.indexOf("GET /set/submit") >= 0) {
              Serial.print("Change Set Temp To ");
              set_temp = new_set_temp;
              Serial.println(set_temp); 
              EEPROM.write(0, set_temp);
              EEPROM.commit();
              Serial.println("EEPROM Write");
             }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");           
            client.println("<link rel=\"icon\" href=\"data:,\">");

            //CSS Styles
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; font-size: 15px}");
            client.println(".button {background-color: lightgrey; color: black; padding: 10px 20px; text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer; border-color: black; border-style: solid; border-radius: 10px;}");
            client.println(".temp {background-color: #4CAF50; color: white; padding: 10px 30px; text-decoration: none; font-size: 15px; margin: 2px; border-color: black; border-style: none; border-radius: 10px;}");
            client.println(".heat {background-color: orange; color: white; padding: 10px 30px; text-decoration: none; font-size: 15px; margin: 2px; border-color: black; border-style: none; border-radius: 10px;}");
            client.println(".cool {background-color: lightblue; color: white; padding: 10px 30px; text-decoration: none; font-size: 15px; margin: 2px; border-color: black; border-style: none; border-radius: 10px;}");
            client.println(".idle {background-color: lightgrey; color: white; padding: 10px 30px; text-decoration: none; font-size: 15px; margin: 2px; border-color: black; border-style: none; border-radius: 10px;}");
            client.println(".error {background-color: red; color: white; padding: 10px 30px; text-decoration: none; font-size: 15px; margin: 2px; border-color: black; border-style: none; border-radius: 10px;}");
            client.println(".relax {background-color: white; color: white; padding: 10px 30px; text-decoration: none; font-size: 15px; margin: 2px; border-color: black; border-style: none; border-radius: 10px;}");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println("</style></head>");
          
            // Web Page Heading
            client.println("<body><h1>Laser Snake Temperature</h1>");

            //Set Temp Line
            client.println("Set Temperature =");
            client.println("<inline-block class=\"temp\">" );
            client.println(set_temp);
            client.println("&#8451</inline-block><br><br><br>");         
            
            //Change Set Temp Line
            client.println("New Set Temperature =");
            client.println("<inline-block class=\"temp\">" );
            client.println(new_set_temp);
            client.println("&#8451</inline-block>");         
            client.println("<a href=\"/set/up\"><button class=\"button\">+</button></a><a href=\"/set/down\"><button class=\"button\">-</button></a><a href=\"/set/submit\"><button class=\"button\">SUBMIT</button></a></p><br>");
            
            //Fermenter Temp Line
            client.println("Fermenter Temperature =");
            client.println("<inline-block class=\"temp\">" );
            client.println(vat_temp);
            client.println("&#8451</inline-block><br><br><br>");

            //Fridge Temp Line
            client.println("Fridge Temperature =");
            client.println("<inline-block class=\"temp\">" );
            client.println(air_temp);
            client.println("&#8451</inline-block><br><br><br>");

            //Status Temp Line
            client.println("Current State =");
            if(state == STATE_IDLE) {      
              client.println("<inline-block class=\"idle\">" );
              client.println("IDLE");
              client.println("</inline-block><br><br><br>");
            }
            else if(state == STATE_HEAT) {      
              client.println("<inline-block class=\"heat\">" );
              client.println("HEATING");
              client.println("</inline-block><br><br><br>");
            }
            else if(state == STATE_COOL) {      
              client.println("<inline-block class=\"cool\">" );
              client.println("COOLING");
              client.println("</inline-block><br><br><br>");
            }
            else if(state == STATE_ERROR) {      
              client.println("<inline-block class=\"error\">" );
              client.println("ERROR");
              client.println("</inline-block><br><br><br>");
            }
            else if(state == STATE_RELAX) {      
              client.println("<inline-block class=\"relax\">" );
              client.println("RELAX");
              client.println("</inline-block><br><br><br>");
            }
            client.println("</body></html>");
        
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
         } else { // if you got a newline, then clear currentLine
           currentLine = "";
         }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
       currentLine += c;      // add it to the end of the currentLine
       }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
 }
