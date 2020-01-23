//cricketGPS
//
// The program....
//
// LED sequence
// RED -> started
// GREEN -> connected to wifi
// 5x RED -> 5 quick red blinks indicate sensor issue .
// 3x GREEN -> 3 green blinks indicates a successful connection to the server
// 
//CONFIGURATION:
const char serveraddress[] = "site.com";
const char passcode[] = "code";  //the passcode of the ArduinoGPS tracker which is looked up and verified by the server.  

struct wifiInfo {
  const char* ssid;
  const char* password;
};

struct wifiInfo wifis[] = { 
  {"ssid1","password1"},
  {"ssid2", "password2"},
  
};

//GPS
#include "TinyGPS++.h"
TinyGPSPlus gps;

#include "SoftwareSerial.h"
#define rxPin D3
#define txPin D4
SoftwareSerial gpsDevice(rxPin,txPin); //RX, TX

//Wireless
#include <ESP8266WiFi.h>
WiFiClient client;

//LED
#define GREENLED LED_BUILTIN//D1
#define REDLED D2

//Time
#include <Time.h>
#include <TimeLib.h>
TimeElements tm;
time_t epoch_ts;

String webpage;

void setup() {
  //Configure LED
  pinMode(GREENLED,OUTPUT);  
  pinMode(REDLED,OUTPUT);

  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);

  //Setup LEDs initial state.  RED LED indicates on, but not connected.
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, HIGH);

  Serial.begin(9600);
  gpsDevice.begin(9600);
  

  //Connect to Wifi
  connectWifi();
}

void loop() {
  while(gpsDevice.available() > 0 ){
    if(gps.encode(gpsDevice.read())){
      if(gps.location.isUpdated()){
        //check for WiFi connection
        if(WiFi.status()== !WL_CONNECTED ){
          connectWifi();
        }
        Serial.println(String(gps.location.lat(),6)+","+String(gps.location.lng(),6));

        //Create a unix timestamp
        tm.Second = gps.time.second();
        tm.Minute = gps.time.minute();
        tm.Hour = gps.time.hour();
        tm.Day = gps.date.day();
        tm.Month = gps.date.month();
        tm.Year = gps.date.year() - 1970;
        epoch_ts = makeTime(tm);

        //get the speed.
        Serial.println("speed: " + String(gps.speed.mph()) + "mph");
        Serial.println("altitude: " +String(gps.altitude.feet())+ "feet");
        
        //Create a string to post        
        String postString = "pass="+String(passcode)+"&lat=" + String(gps.location.lat(),6) 
          +"&long="+String(gps.location.lng(),6) 
          +"&timestamp="+String(epoch_ts)
          +"&speed="+String(gps.speed.mph())
          +"&alt="+String(gps.altitude.feet());
        Serial.println("Sending: "+postString);
        
        //connect to server
        Serial.println("\nStarting connection...");
        if (client.connect(serveraddress, 80)) {
          Serial.println("Connected to server.");
          digitalWrite(GREENLED, HIGH);
          digitalWrite(REDLED, LOW);
          
          // Make a HTTP POST request:
          client.println("POST /gps-tracker/ HTTP/1.1"); 
          client.println("Host: " + String(serveraddress));
          client.println("Content-Type: application/x-www-form-urlencoded");
          client.println("Connection: close");
          client.print("Content-Length: ");
          client.println(postString.length());
          client.println();
          client.print(postString);
          client.println();    

          //server response
          
          Serial.print("POST Sent. \nWaiting for response");
          while(client.available() == 0){
            Serial.print(".");
          }
          
          webpage = "";

          while (!webpage.endsWith("Connection: close")){
            char c = client.read();
            webpage += c;
            //Serial.println(webpage);
            //yield();  yields cpu time to other processes
          }
          Serial.println(webpage);

//            //If there was a successful response wait before trying again.
//            //TODO change delay based on speed.
          if(webpage.indexOf("200 OK") != -1){
            delay(5000);
          }
        } else {
          Serial.println("connection failed");
        }
      }
    }
  }
  Serial.print(".");
}

//Look though available SSIDs for one that is known, then try to connect to it for max_seconds
void connectWifi(){
  int max_seconds = 30;

  while(WiFi.status() != WL_CONNECTED){
    //find available networks
    int n = WiFi.scanNetworks();
    String scanComplete1 = "Wifi scan complete. Found ";
    String scanComplete2 = " networks.";
    Serial.println( scanComplete1 + n + scanComplete2);
    for( int j = 0; j < n ; j++){
      int i = 0;
      Serial.print("SSID: ");
      Serial.print(WiFi.SSID(j));
      Serial.print("\tstength: ");
      Serial.println(WiFi.RSSI(j));
      
      //check if scanned wifi is a known wifi
      for ( i = 0; i < sizeof(wifis); i++){
        if(WiFi.SSID(j) == wifis[i].ssid){
          int attempts = 0;
          
          Serial.print("\nAttempting to connect to WiFi SSID: ");
          Serial.println(wifis[i].ssid);
          
          WiFi.begin(wifis[i].ssid, wifis[i].password);
          
          //try for max_secondds to connect, then move to the next wifi
          while(WiFi.status() != WL_CONNECTED && attempts <= (max_seconds*2)) {
            Serial.print(attempts);
            Serial.print(".");
            attempts += 1;
            delay(500);
          }
          if(WiFi.status()== WL_CONNECTED ){ break; }
          Serial.print("\nFAILED to connect to WiFi SSID: ");
          Serial.println(wifis[i].ssid);
        }
      } 
      if(WiFi.status()== WL_CONNECTED ){ break; }
    }    
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  digitalWrite(GREENLED, HIGH);
  digitalWrite(REDLED, LOW);
}
