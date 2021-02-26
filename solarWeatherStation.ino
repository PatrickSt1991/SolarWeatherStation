#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <Arduino.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include "DHT.h"                 // DHT Library


#define DHTPIN 4     // Connect your DHT22 to Pin4 of Wemos D1 Mini Module.
#define DHTTYPE DHT22   // DHT 22

// Intiating HTTP Request to your Website. 
HTTPClient http;
DHT dht(DHTPIN, DHTTYPE);  // Initialize DHT11 Module and Library

void setup() {
  Serial.begin(115200);
  
  WiFiManager wifiManager;
  
  IPAddress _ip = IPAddress(192, 168, 0, 000);  // Change These 3 Settings According to your Router's IP and GateWay.
  IPAddress _gw = IPAddress(192, 168, 0, 0);
  IPAddress _sn = IPAddress(255, 255, 255, 0);
  
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
  dht.begin();  // Intialize DHT Library. Credits: Adafruit. 

  if (!wifiManager.autoConnect("Solar_Weather_Station_Nr2", "YOUR_PASSWORD_HERE")) {           // No Need for Changing These.
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  Serial.println("local ip");
  Serial.println(WiFi.localIP());  
}

void loop() {
  delay(2000);

  // Voltage divider R1 = 220k+100k+100k =420k and R2=100k
  float volt;
  float calib_factor = 4.16; // change this value to calibrate the battery voltage
  unsigned long raw = analogRead(A0);
  volt = raw * calib_factor/1024; 
  
  Serial.print( "Voltage = ");
  Serial.print(volt, 2); // print with 2 decimal places
  Serial.println (" V");
  http.begin("http://192.168.0.125:8080/json.htm?type=command&param=udevice&idx=63&nvalue=0&svalue="+ String(volt));
  int httpVolt = http.GET();
  http.end();
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  Serial.println(h);
  Serial.println(t);
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  http.begin("http://192.168.0.125:8080/json.htm?type=command&param=udevice&idx=69&nvalue=0&svalue="+ String(t) +";"+ String(h)+";0");
  int httpCode = http.GET();
  
  if(httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    } 
  } else {
    Serial.println("GET Request Failed!");
  }
  http.end();

 ESP.deepSleep(30 * 60 * 1000000); // convert to microseconds
}
