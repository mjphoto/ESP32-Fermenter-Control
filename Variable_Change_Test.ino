//Testing webserver, temperature probe readings and changing a variable from the webserver on an
//ESP32 module. This code will be merged with some existing arduino code that controls a fermentation
//chamber once functions are proven

// Load libraries
#include <WiFi.h>
#include <Wire.h>

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

//replace addresses for each new probe see - http://henrysbench.capnfatz.com/henrys-bench/arduino-temperature-measurements/ds18b20-arduino-user-manual-introduction-and-contents/ds18b20-user-manual-part-2-getting-the-device-address/
DeviceAddress VAT_TEMP_SENSOR = {0x28, 0xFF, 0x16, 0x8D, 0x87, 0x16, 0x03, 0x50}; //Test sensor A
DeviceAddress AIR_TEMP_SENSOR = {0x28, 0xFF, 0x0A, 0x2E, 0x68, 0x14, 0x04, 0xA6}; //Test sensor B

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
const char* ssid     = "AHRF455889";
const char* password = "zxcv1597";
//const char* ssid     = "ScottGuest2";
//const char* password = "scott630";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Replace with your unique IFTTT URL resource
const char* resource = "https://maker.ifttt.com/trigger/fermenter_temp/with/key/bQJZjx9ezdfV_3muN_LuQIsfm8GRDRk3cD0sN7HTkE1";

// Maker Webhooks IFTTT
const char* server_ifttt = "maker.ifttt.com";

//timer for IFTTT
unsigned long IFTTT_TIMER = 120000; //2mins in milliseconds
unsigned long currentMillis;
unsigned long startMillis;

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
  
  //Initalize timer
  startMillis = millis();
}

//web server function
void web_server(WiFiClient client){
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
           
            //Disconnect from client button to make it easier for the logging code to keep running after changing set temp
            if (header.indexOf("GET /set/disconnect") >= 0) {
              client.stop();
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

            //Disconnect Button
            client.println("<a href=\"/set/disconnect\"><button class=\"button\">DISCONNECT</button></a></p><br><br><br>");
            
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

// Make an HTTP request to the IFTTT web service
void makeIFTTTRequest() {
  Serial.print("Connecting to "); 
  Serial.print(server_ifttt);
  
  WiFiClient client;
  int retries = 10;
  while(!!!client.connect(server_ifttt, 80) && (retries-- > 0)) {
    Serial.print(".");
  }
  Serial.println();
  if(!!!client.connected()) {
    Serial.println("Failed to connect...");
  }
  
  Serial.print("Request resource: "); 
  Serial.println(resource);

  String jsonObject = String("{\"value1\":\"") + set_temp + "\",\"value2\":\"" + vat_temp
                      + "\",\"value3\":\"" + air_temp + "\"}";
                    
  client.println(String("POST ") + resource + " HTTP/1.1");
  client.println(String("Host: ") + server_ifttt); 
  client.println("Connection: close\r\nContent-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonObject.length());
  client.println();
  client.println(jsonObject);

  int timeout_b = 10 * 10;  
  while(!!!client.available() && (timeout_b-- > 0)){
    delay(100);
  }
  if(!!!client.available()) {
    Serial.println("No response...");
  }
  while(client.available()){
    Serial.write(client.read());
  } 
  Serial.println("\nclosing connection");
  client.stop();

  //startMillis resets
  startMillis = millis();
}

void loop(){
  
  //start timer
  currentMillis = millis();
  
  //Get temps
  sensors.requestTemperaturesByAddress(AIR_TEMP_SENSOR);
  sensors.requestTemperaturesByAddress(VAT_TEMP_SENSOR);
  vat_temp = sensors.getTempC(VAT_TEMP_SENSOR);
  air_temp = sensors.getTempC(AIR_TEMP_SENSOR);

  WiFiClient client = server.available();   // Listen for incoming clients
  
  //conditional statements to run webserver or google sheets function
  if (client) {   
    web_server(client);
  }
  else if ((currentMillis - startMillis >= IFTTT_TIMER) && !client) {
    makeIFTTTRequest();
  }
  else if ((currentMillis - startMillis < IFTTT_TIMER) && !client) { 
    Serial.print((currentMillis - startMillis)/1000);
    Serial.print("  Set Temp = ");
    Serial.print(set_temp);
    Serial.print("  Vat Temp = ");
    Serial.print(vat_temp);
    Serial.print("  Air Temp = ");
    Serial.println(air_temp);
  }
}
