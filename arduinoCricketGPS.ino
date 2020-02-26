//cricketGPS
//
// The program....
//
// LED sequence
// RED -> started
// GREEN -> connected to wifi
// GREEN2 -> connected to gps
// GREEN2 (blinking) -> sent gps update
// 
//CONFIGURATION:
const char* serveraddress = "tdh-scripts.herokuapp.com";
const char passcode[] = "TaxaCricKetCHIRP";  //the passcode of the ArduinoGPS tracker which is looked up and verified by the server.  

struct wifiInfo {
  const char* ssid;
  const char* password;
};

struct wifiInfo wifis[] = { 
  {"noMAD 4G", "1234554321"},
  {"noMAD Wifi", "1234554321"},
  {"ThisGuy", "davephone321"},
  {"MADhouse","1234554321"},
  {"OTC", "D33pW@t3r"},
  {"OTC Guest", "Welcome!"}
};

//GPS
#include "TinyGPS++.h"
TinyGPSPlus gps;

#include "SoftwareSerial.h"
#define rxPin D3
#define txPin D4
SoftwareSerial gpsDevice(rxPin, txPin); //RX, TX
char gpsDeviceFound;

//Wireless
#include <ESP8266WiFi.h>
WiFiClient client;

//LED
#define GREENLED_GPS D0
#define GREENLED D1
#define REDLED D2

//Time
#include <Time.h>
#include <TimeLib.h>
TimeElements tm;
time_t epoch_ts;

String webpage;

void setup() {
  Serial.begin(115200);
  gpsDevice.begin(9600);
  
  //Configure LED
  pinMode(GREENLED_GPS,OUTPUT);
  pinMode(GREENLED,OUTPUT);  
  pinMode(REDLED,OUTPUT);

  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);

  //Setup LEDs initial state.  RED LED indicates on, but not connected.
  digitalWrite(GREENLED_GPS, LOW);
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, HIGH);

  //Connect to Wifi
  connectWifi();
}

void loop() {
  if(gpsDevice.available() > 0 ){
    digitalWrite(GREENLED_GPS, HIGH);
    if(gpsDeviceFound != '+'){
//      Serial.print("\n");  
    }
    gpsDeviceFound = '+';
//    Serial.print(gpsDeviceFound);
    
    gps.encode(gpsDevice.read());
    if(gps.location.isUpdated()){
      //check for WiFi connection
      if(WiFi.status()== !WL_CONNECTED ){
        digitalWrite(GREENLED, LOW);
        digitalWrite(REDLED, HIGH);
        connectWifi();
      }

      //Create a unix timestamp
      tm.Second = gps.time.second();
      tm.Minute = gps.time.minute();
      tm.Hour = gps.time.hour();
      tm.Day = gps.date.day();
      tm.Month = gps.date.month();
      tm.Year = gps.date.year() - 1970;
      epoch_ts = makeTime(tm);

      //show the new info.
      Serial.println();
      Serial.println("-------Coordinates Updated---------");
      Serial.println(String(gps.date.month()) + "-" + String(gps.date.day()) + "-" + String(gps.date.year()) + " " + String(gps.time.hour()) + ":" + String(gps.time.minute())+ ":" + String(gps.time.second()));
      Serial.println(String(gps.location.lat(),6)+","+String(gps.location.lng(),6));
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
        blink(GREENLED_GPS, 10, 500);
        
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

        //If there was a successful response wait before trying again.
        //TODO change delay based on speed.
        if(webpage.indexOf("200 OK") != -1){
          delay(5000);
        }
      } else {
        Serial.println("connection failed");
      }
    }
  }else{
    digitalWrite(GREENLED_GPS, LOW);
    if(gpsDeviceFound != '.'){
//      Serial.print("\n");  
    }
    gpsDeviceFound = '.';
//    Serial.print(gpsDeviceFound);
  }
}

//Look though available SSIDs for one that is known, then try to connect to it for max_seconds
void connectWifi(){
  int max_seconds = 30;

  while(WiFi.status() != WL_CONNECTED){
    //find available networks
    int n = WiFi.scanNetworks();
    String scanComplete1 = "\nWifi scan complete. Found ";
    String scanComplete2 = " networks.";
    Serial.println( scanComplete1 + n + scanComplete2);
    WiFi.mode(WIFI_STA);
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

void blink(int color, int times, int hold=250){
  int previous_state = digitalRead(color);
  //turn the LED off.
  digitalWrite(color,LOW);
  
  //blink {color} LED {times} times for {hold} msec
  // color should be GREENLED or REDLED
  for(int x = 0; x < times; x++){
    digitalWrite(color, HIGH);
    delay(hold);
    digitalWrite(color, LOW);
    delay(hold);
  }
  //return to the prior state.
  digitalWrite(color,previous_state);
}
