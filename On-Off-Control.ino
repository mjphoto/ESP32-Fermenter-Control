//#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RunningMedian.h>

// Load libraries
#include <WiFi.h>
#include <Wire.h>

//Load EEPROM Library
#include <EEPROM.h>

// define the number of bytes you want to access
#define EEPROM_SIZE 1

// either debug OR production mode, as both use the serial 
#define DEBUG true
#define PRODUCTION false
#define ONE_WIRE_BUS 15
#define FRIDGE_RELAY 22
#define HEATER_RELAY 23

/*  tuning variables- tweak for more precise control
 *  Note: negative values mean below the set temperature.
 *  Be careful to ensure that variables do not overlap, to prevent
 *  competitve cycling
 */
float set_temp;
float new_set_temp; 
float heater_on_thresh = -0.25;
float heater_off_thresh = -0.05;
float fridge_on_thresh = 0.25;
float fridge_off_thresh = 0.05;

// Replace with your network credentials
const char* ssid     = "AHRF455889";
const char* password = "zxcv1597";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Replace with your unique IFTTT URL resource
const char* resource = "https://maker.ifttt.com/trigger/fermenter_temp/with/key/bQJZjx9ezdfV_3muN_LuQIsfm8GRDRk3cD0sN7HTkE1";

// Maker Webhooks IFTTT
const char* server_ifttt = "maker.ifttt.com";

//timer for IFTTT
unsigned long IFTTT_TIMER = 120000; //10mins in milliseconds
unsigned long currentMillis;
unsigned long startMillis;

//set up non-tunable control variables
unsigned long RUN_THRESH = 180000; //3min in milliseconds, minimum time for heating/cooling elements to run
bool can_turn_off = true; //bool for short cycle timer
unsigned long start_time = 0; //variable for timing heating/cooling duration
unsigned long MAX_RUN_TIME = 14400000; //4 hours - maximum time for heating element to be on
unsigned long RELAX_TIME = 300000; //5min 

unsigned long error_counter = 0;

//system states
const int STATE_ERROR = -1;
const int STATE_RELAX = 0;
const int STATE_IDLE = 1;
const int STATE_COOL = 2;
const int STATE_HEAT = 3;
int state = STATE_IDLE; //initialise in idle

// set up 1-wire probes
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
//DeviceAddress AIR_TEMP_SENSOR = {0x28, 0xFF, 0x7A, 0xF6, 0x82, 0x16, 0x03, 0x69};
//DeviceAddress VAT_TEMP_SENSOR = {0x28, 0xFF, 0xE3, 0x9C, 0x82, 0x16, 0x04, 0x25};
DeviceAddress AIR_TEMP_SENSOR = {0x28, 0xFF, 0x16, 0x8D, 0x87, 0x16, 0x03, 0x50}; //Test sensor A
DeviceAddress VAT_TEMP_SENSOR = {0x28, 0xFF, 0x0A, 0x2E, 0x68, 0x14, 0x04, 0xA6}; //Test sensor B

//set up temp measurement variables
bool air_probe_connected;
bool vat_probe_connected;
float air_temp;
float vat_temp;
unsigned long last_temp_request = 0;
bool waiting_for_conversion = false;
unsigned long CONVERSION_DELAY = 1000; //time allocated for temperature conversion
unsigned long MEAS_INTERVAL = 1000; //take temperature measurement every 1s
RunningMedian vatTempMedian = RunningMedian(60);
RunningMedian airTempMedian = RunningMedian(60);

//unsigned long PUBLISH_PERIOD = 60000; //Publish values every minute
//unsigned long last_publish = 0;

// Initialisations  
const int VAT_ID = 1;
const int AIR_ID = 2;

void setup() {
  Serial.begin(115200);
  
   //Start EEPROM at defined size (above).
  EEPROM.begin(EEPROM_SIZE);
  
  Serial.print("EEPROM 0  = ");
  Serial.println(EEPROM.read(0));

    //set up set temp variables here.
  if (EEPROM.read(0) != 255) {
    set_temp = EEPROM.read(0);
  }
  if (EEPROM.read(0) == 0) {
    set_temp = 19.0;
    EEPROM.write(0, set_temp);
    EEPROM.commit();
    Serial.print("EEPROM Write Set Temp = ");
    Serial.println(EEPROM.read(0));
  }
  new_set_temp = set_temp;
   
  // Set up temperature probes
  sensors.setResolution(AIR_TEMP_SENSOR, 11); //resolution of 0.125deg cels, 
  sensors.setResolution(VAT_TEMP_SENSOR, 11); //takes approx 375ms
  if (sensors.getResolution(VAT_TEMP_SENSOR) == 0) {
    vat_probe_connected = false;
    state = STATE_ERROR;
  } else {
    vat_probe_connected = true;
  }
  if (sensors.getResolution(AIR_TEMP_SENSOR) == 0) {
    air_probe_connected = false;
    state = STATE_ERROR;
  } else {
    air_probe_connected = true;
  }
  /* Setting the waitForConversion flag to false ensures that a tempurate request returns immediately
   *  without waiting for a temperature conversion. If setting the flag to false, be sure to wait 
   *  the appropriate amount of time before retrieving the measurement, to allow time for the conversion
   *  to take place.
   */
  sensors.setWaitForConversion(false);
  #if (DEBUG) 
    Serial.print("Vat sensor resolution: ");
    Serial.println(sensors.getResolution(VAT_TEMP_SENSOR), DEC);
    Serial.print("Air sensor resolution: ");
    Serial.println(sensors.getResolution(AIR_TEMP_SENSOR), DEC);
  #endif

  //initialise relays
  pinMode(FRIDGE_RELAY, OUTPUT);
  digitalWrite(FRIDGE_RELAY, HIGH);
  pinMode(HEATER_RELAY, OUTPUT);
  digitalWrite(HEATER_RELAY, HIGH);

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
              new_set_temp = new_set_temp + 1;  
             }
            else if (header.indexOf("GET /set/down") >= 0) {
              Serial.println("Decrease New Set Temperature by 0.5");
              new_set_temp = new_set_temp - 1;           
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
            //Error Counter Line
            client.println("Error Counter =");
            client.println("<inline-block class=\"error\">" );
            client.println(error_counter);
            client.println(" Seconds</inline-block><br><br><br>");

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

void perform_action(String action) {
  //start timer so don't short cycle
  start_time = millis();
  if(action == "cool") {
    can_turn_off = false;
    #if (DEBUG)
      Serial.println("...Turning fridge on");
    #endif
    digitalWrite(FRIDGE_RELAY, LOW);
  } else if(action == "heat") {
    can_turn_off = false;
    #if (DEBUG)
      Serial.println("...Turning heating on");
    #endif
    digitalWrite(HEATER_RELAY, LOW);
  } else if (action == "disable") {
    #if (DEBUG) 
      Serial.println("...Switching elements off");
    #endif
    digitalWrite(HEATER_RELAY, HIGH);
    digitalWrite(FRIDGE_RELAY, HIGH);
  }
}

void proc_idle() {
  if (!vat_probe_connected || !air_probe_connected) {
    state = STATE_ERROR;
  }
  if (vat_temp - set_temp > fridge_on_thresh) {
    state = STATE_COOL; 
    perform_action("cool"); //activate cooling
  }else if (vat_temp - set_temp < heater_on_thresh) {
    state = STATE_HEAT;
    perform_action("heat");
  }
}

void proc_heat() {
  // check if probes are connected
  if (!vat_probe_connected || !air_probe_connected) {
    state = STATE_ERROR;
  }
  // check if minimum runtime has elapsed
  if ((millis() - start_time) > RUN_THRESH) {
    can_turn_off = true;
  }
  // check if conditions are right to turn heater off 
  if ((vat_temp - set_temp > heater_off_thresh) && can_turn_off) {
    state = STATE_IDLE;
    perform_action("disable");
  }
  // check if heater has been on for too long
  if ((millis() - start_time) > MAX_RUN_TIME) {
    #if (DEBUG) 
      Serial.println("Maximum heat time exceeded, entering STATE_RELAX");
    #endif
    state = STATE_RELAX;
    perform_action("disable");
  }
}

void proc_cool() {
  if (!vat_probe_connected || !air_probe_connected) {
    state = STATE_ERROR;
  }
  if ((millis() - start_time) > RUN_THRESH) {
    can_turn_off = true;
  } //fridge to turn off at set_temp + THRESH
  if ((vat_temp - set_temp < fridge_off_thresh) && can_turn_off) {
    state = STATE_IDLE;
    perform_action("disable");
  }
}

void proc_relax() {
  if (millis() - start_time > RELAX_TIME) {
    #if (DEBUG) 
      Serial.println("Relax time exceeded, entering STATE_IDLE");
    #endif
    state = STATE_IDLE;
  }
}

void error_handler() {
  if (millis() - start_time > RUN_THRESH) {
    can_turn_off = true;
    perform_action("disable"); 
  }
  // if probe comes back 
  if (vat_probe_connected && air_probe_connected) {
    if (can_turn_off) {
    #if (DEBUG)
      Serial.println("Exiting STATE_ERROR");
    #endif
      state = STATE_IDLE;
      //perform_action("disable");      
    }
  }
}

/* print status to Serial monitor, activated in debug mode only */
void status_update() {
  Serial.print("Vat temp:");
  Serial.print(vat_temp);
  Serial.print("\t");
  Serial.print("Air temp:");
  Serial.println(air_temp);
    if (state == STATE_HEAT) {
      Serial.print("Heater has been on for ");
      Serial.print((millis()-start_time)/1000);
      Serial.println(" seconds");
      if (can_turn_off) {
        Serial.println("Minimum time elapsed, heating can be turned off");
      }
    }else if (state == STATE_COOL) {
      Serial.print("Fridge has been on for ");
      Serial.print((millis()-start_time)/1000);
      Serial.println(" seconds");
      if (can_turn_off) {
        Serial.println("Minimum time elapsed, fridge can be turned off");
      }
    }else if (state == STATE_IDLE) {
      Serial.println("System is idle");
    } else if (state == STATE_ERROR) {
      Serial.println("ERROR");
      if (!vat_probe_connected) {
        Serial.println("Vat probe not connected");
      }
      if (!air_probe_connected) {
        Serial.println("Air probe not connected");
      }
    }
}

void loop() {
  if (!waiting_for_conversion && (millis() - last_temp_request) > MEAS_INTERVAL) {
    air_probe_connected = sensors.requestTemperaturesByAddress(AIR_TEMP_SENSOR);
    vat_probe_connected = sensors.requestTemperaturesByAddress(VAT_TEMP_SENSOR);
    last_temp_request = millis();
    waiting_for_conversion = true;
  }
  if (waiting_for_conversion && millis()-last_temp_request > CONVERSION_DELAY) {
    vat_temp = sensors.getTempC(VAT_TEMP_SENSOR);
    air_temp = sensors.getTempC(AIR_TEMP_SENSOR);
    waiting_for_conversion = false;

    vatTempMedian.add(vat_temp);
    airTempMedian.add(air_temp);

    
   if (state == STATE_ERROR){
      error_counter = error_counter + 1;
      Serial.print(" Seconds");
   }

    //start timer
    currentMillis = millis();
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

   
   Serial.print("Error Counter = ");
   Serial.println(error_counter);
   
     /* sent data to serial for collection from python script. 
     */
    /* if ( (millis() - last_publish) > PUBLISH_PERIOD) {
      #if (PRODUCTION)
        Serial.print("temperature_status;");
        Serial.print(vatTempMedian.getMedian());
        Serial.print(";");
        Serial.print(airTempMedian.getMedian());
        Serial.print(";");
        Serial.print(set_temp);
        Serial.print(";");
        Serial.println(state);
      #endif
      last_publish = millis();
     }*/
     
    //Send an update to the Serial monitor if DEBUG mode is on
    #if (DEBUG) 
      status_update();
    #endif
    
    // Manage states- can't heat while cooling and vice versa
    switch (state) {
      case STATE_IDLE:
        proc_idle();
        break;
      case STATE_HEAT:
        proc_heat();
        break;
      case STATE_COOL:
        proc_cool();
        break;
      case STATE_RELAX:
        proc_relax();
        break;
      case STATE_ERROR:
        error_handler();
        break;    
    }
  }
}
 
  
