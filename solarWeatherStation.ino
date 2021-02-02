 /*----------------------------------------------------------------------------------------------------
  Project Name : Solar Powered WiFi Weather Station V2.34
  Features: temperature, dewpoint, dewpoint spread, heat index, humidity, absolute pressure, relative pressure, battery status and
  Authors: Keith Hungerford, Debasish Dutta and Marc Stähli (modified by Patrick)
  Website : www.opengreenenergy.com
  Github: https://github.com/3KUdelta/Solar_WiFi_Weather_Station
  Modified: https://github.com/PatrickSt1991/solarWeatherStation
    
////  Features :  /////////////////////////////////////////////////////////////////////////////////////////////////////////////                                                                                                                   
// 1. Connect to Wi-Fi, and upload the data to MQTT broker.
// 2. Monitoring Weather parameters like Temperature and Humidity.
// 3. Remote Battery Status Monitoring
// 4. Using Sleep mode to reduce the energy consumed                                        
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************
 * VERY IMPORTANT:                                 *
 *                                                 *
 * Enter your personal settings in Settings.h !    *
 *                                                 *
 **************************************************/

#include "Settings.h"

#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "FS.h"
#include <EasyNTPClient.h>       //https://github.com/aharshac/EasyNTPClient
#include <TimeLib.h>             //https://github.com/PaulStoffregen/Time.git
#include <PubSubClient.h>        // For MQTT (in this case publishing only)
#include <DHT.h>

//CONSTANTS
#define DHTPIN 4
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);
WiFiUDP udp;
EasyNTPClient ntpClient(udp, NTP_SERVER, TZ_SEC + DST_SEC);

float measured_temp;
float measured_humi;
float volt;

const char* mqtt_user = "********";
const char* mqtt_pass = "********";
const char* mqtt_device = "SolarWeatherStationNumber1";

// FORECAST CALCULATION
unsigned long current_timestamp;    // Actual timestamp read from NTPtime_t now;
unsigned long saved_timestamp;      // Timestamp stored in SPIFFS
int accuracy;

void(* resetFunc) (void) = 0;       // declare reset function @ address 0

WiFiClient espClient;               // MQTT
PubSubClient client(espClient);     // MQTT

void setup() {
  
  Serial.begin(115200);
  dht.begin();
  Serial.println();
  Serial.println("Start of SolarWiFiWeatherStation V2.33");

  //******Battery Voltage Monitoring (first thing to do: is battery still ok?)***********
  
  // Voltage divider R1 = 220k+100k+100k =420k and R2=100k
  float calib_factor = 4.18; // change this value to calibrate the battery voltage
  unsigned long raw = analogRead(A0);
  volt = raw * calib_factor/1024; 
  
  Serial.print( "Voltage = ");
  Serial.print(volt, 2); // print with 2 decimal places
  Serial.println (" V");

  // **************Application going online**********************************************
  
  WiFi.mode(WIFI_STA);
  WiFi.hostname("SolarWeatherStationBuiten"); //This changes the hostname of the ESP8266 to display neatly on the network esp on router.
  WiFi.begin(ssid, pass);
  Serial.print("---> Connecting to WiFi ");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    i++;
    if (i > 20) {
      Serial.println("Could not connect to WiFi!");
      Serial.println("Going to sleep for 10 minutes and try again.");
      if (volt > 3.3){
        goToSleep(10);   // go to sleep and retry after 10 min
      }  
      else{
        goToSleep(0);   // hybernate because batt empty - this is just to avoid that an endless
      }                 // try to get a WiFi signal will drain the battery empty
    }
  Serial.print(".");
  }
  Serial.println(" Wifi connected ok"); 
   
  connect_to_MQTT();            // connecting to MQTT broker

  client.publish("home/debug", "SolarWeatherstationBuiten: Sensor started");
  delay(50);
  
  //*****************Checking if SPIFFS available********************************

  Serial.println("SPIFFS Initialization: (First time run can last up to 30 sec - be patient)");
  
  boolean mounted = SPIFFS.begin();               // load config if it exists. Otherwise use defaults.
  if (!mounted) {
    Serial.println("FS not formatted. Doing that now... (can last up to 30 sec).");
    SPIFFS.format();
    Serial.println("FS formatted...");
    SPIFFS.begin();
  }
  
  //******** GETTING THE TIME FROM NTP SERVER  ***********************************
  
  Serial.println("---> Now reading time from NTP Server");
  int ii = 0;
  while(!ntpClient.getUnixTime()){
    delay(100); 
    ii++;
    if (ii > 20) {
      Serial.println("Could not connect to NTP Server!");
      Serial.println("Doing a reset now and retry a connection from scratch.");
      resetFunc();
      }  
    Serial.print("."); 
  }
  current_timestamp = ntpClient.getUnixTime();      // get UNIX timestamp (seconds from 1.1.1970 on)
  
  Serial.print("Current UNIX Timestamp: ");
  Serial.println(current_timestamp);

  Serial.print("Time & Date: ");
  Serial.print(hour(current_timestamp));
  Serial.print(":");
  Serial.print(minute(current_timestamp));
  Serial.print(":"); 
  Serial.print(second(current_timestamp));
  Serial.print("; ");
  Serial.print(day(current_timestamp));
  Serial.print(".");
  Serial.print(month(current_timestamp));         // needed later: month as integer for Zambretti calcualtion
  Serial.print(".");
  Serial.println(year(current_timestamp));      
             
  //******** GETTING RELATIVE PRESSURE DATA FROM SENSOR (BME680)  ******************** 
  
  measurementEvent();            //calling function to get all data from the different sensors
  
  //*******************SPIFFS operations***************************************************************

  ReadFromSPIFFS();              //read stored values and update data if more recent data is available

  Serial.print("Timestamp difference: ");
  Serial.println(current_timestamp - saved_timestamp);

  if (current_timestamp - saved_timestamp > 21600){    // last save older than 6 hours -> re-initialize values
    FirstTimeRun();
  }
  else if (current_timestamp - saved_timestamp > 1800){ // it is time for pressure update (1800 sec = 30 min)
  
    if (accuracy < 12) {
      accuracy = accuracy + 1;                            // one value more -> accuracy rises (up to 12 = 100%)
      }
      WriteToSPIFFS(current_timestamp);                   // update timestamp on storage
    }
  else {         
    WriteToSPIFFS(saved_timestamp);                     // do not update timestamp on storage
  }

  //*******************************************************************************
  // code block for publishing all data to MQTT
  
  char _measured_temp[8];                                // Buffer big enough for 7-character float
  dtostrf(measured_temp, 3, 1, _measured_temp);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation1/tempc", _measured_temp, 1);      // ,1 = retained
  Serial.println("Publish Temp");
  delay(150);
  
  char _measured_humi[8];                                // Buffer big enough for 7-character float
  dtostrf(measured_humi, 3, 0, _measured_humi);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation1/humi", _measured_humi, 1);      // ,1 = retained
  Serial.println("Publish HUM");
  delay(150);

  char _volt[8];                                // Buffer big enough for 7-character float
  dtostrf(volt, 3, 2, _volt);               // Leave room for too large numbers!

  client.publish("home/weather/solarweatherstation1/battv", _volt, 1);      // ,1 = retained
  Serial.println("Publish BAT");
  delay(150);

  if (volt > 3.3) {          //check if batt still ok, if yes
    goToSleep(10); //go for a nap
  }
  else{                      //if not,
    goToSleep(0);            //hybernate because batt is empty
  }
} // end of void setup()

void loop() {               //loop is not used
} // end of void loop()

void measurementEvent() { 
    
  //Measures absolute Temperature, Humidity, Voltage
  
  // Get temperature
  measured_temp = dht.readTemperature();
  // print on serial monitor
  Serial.print("Temp: ");
  Serial.print(measured_temp);
  Serial.print("°C; ");
 
  // Get humidity
  measured_humi = dht.readHumidity();
  // print on serial monitor
  Serial.print("Humidity: ");
  Serial.print(measured_humi);
  Serial.print("%; ");
 
} // end of void measurementEvent()

void ReadFromSPIFFS() {
  char filename [] = "/data.txt";
  File myDataFile = SPIFFS.open(filename, "r");       // Open file for reading
  if (!myDataFile) {
    Serial.println("Failed to open file");
    FirstTimeRun();                                   // no file there -> initializing
  }
  
  Serial.println("---> Now reading from SPIFFS");
  
  String temp_data;
  
  temp_data = myDataFile.readStringUntil('\n');  
  saved_timestamp = temp_data.toInt();
  Serial.print("Timestamp from SPIFFS: ");  Serial.println(saved_timestamp);
  
  temp_data = myDataFile.readStringUntil('\n');  
  accuracy = temp_data.toInt();
  Serial.print("Accuracy value read from SPIFFS: ");  Serial.println(accuracy);
  
  myDataFile.close();
  Serial.println();
}

void WriteToSPIFFS(int write_timestamp) {
  char filename [] = "/data.txt";
  File myDataFile = SPIFFS.open(filename, "w");        // Open file for writing (appending)
  if (!myDataFile) {
    Serial.println("Failed to open file");
  }
  
  Serial.println("---> Now writing to SPIFFS");
  
  myDataFile.println(write_timestamp);                 // Saving timestamp to /data.txt
  myDataFile.close();
  
  Serial.println("File written. Now reading file again.");
  myDataFile = SPIFFS.open(filename, "r");             // Open file for reading
  Serial.print("Found in /data.txt = "); 
  while (myDataFile.available()) { 
    Serial.print(myDataFile.readStringUntil('\n'));
    Serial.print("; ");
  }
  Serial.println();
  myDataFile.close();
}

void FirstTimeRun(){
  Serial.println("---> Starting initializing process.");
  accuracy = 1;
  char filename [] = "/data.txt";
  File myDataFile = SPIFFS.open(filename, "w");            // Open a file for writing
  if (!myDataFile) {
    Serial.println("Failed to open file");
    Serial.println("Stopping process - maybe flash size not set (SPIFFS).");
    exit(0);
  }
  myDataFile.println(current_timestamp);                   // Saving timestamp to /data.txt
  myDataFile.close();
  Serial.println("---> Doing a reset now.");
  resetFunc();                                              //call reset
}

void connect_to_MQTT() {
  Serial.print("---> Connecting to MQTT, ");
  client.setServer(mqtt_server, 1883);
  
  while (!client.connected()) {
    Serial.println("reconnecting MQTT...");
    reconnect(); 
  }
  Serial.println("MQTT connected ok.");
} //end connect_to_MQTT

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection with ");
    // Attempt to connect
    if(client.connect(mqtt_device, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
       // Once connected, publish an announcement...
      client.publish("home/debug", "SolarWeatherstationBuiten: client started...");
      delay(50);
    } else {
      Serial.print(" ...failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
} //end void reconnect*/

void goToSleep(unsigned int sleepmin) {
  char tmp[128];
  String sleepmessage = "SolarWeatherstationBuiten: Taking a nap for " + String (sleepmin) + " Minutes";
  sleepmessage.toCharArray(tmp, 128);
  client.publish("home/debug",tmp);
  delay(50);
 
  Serial.println("INFO: Closing the MQTT connection");
  client.disconnect();
  
  Serial.println("INFO: Closing the Wifi connection");
  WiFi.disconnect();

  while (client.connected() || (WiFi.status() == WL_CONNECTED)) {
    Serial.println("Waiting for shutdown before sleeping");
    delay(10);
  }
  delay(50);
  
  Serial.print ("Going to sleep now for ");
  Serial.print (sleepmin);
  Serial.print (" Minute(s).");
  ESP.deepSleep(sleepmin * 60 * 1000000); // convert to microseconds
} // end of goToSleep()
