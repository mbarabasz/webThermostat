#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2  // DS18B20 pin
#define RELAY_PIN D1 // Relay pin

const char* ssid = "barabasz";
const char* password = "My heart is beating";
const unsigned long timeBetweenMeasures = 5 * 60 * 1000UL; // fiveMinutes
const char* host = "time.nist.gov"; // Round-robin DAYTIME protocol

String storageFilePath ="/storage.json";
String temperatureSetting = "20"; //Defaults only for case when there is no storage yet
float temperatureCurrent = 19.0;
String relayState = "false";
static unsigned long lastSampleTime = 0 - timeBetweenMeasures;  
String TimeDate;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
ESP8266WebServer server(80);

String _index_html = "<!DOCTYPE html>\
               <html ng-app='myApp'>\
                <head>\
                  <script src='http://ajax.googleapis.com/ajax/libs/angularjs/1.4.8/angular.min.js'></script>\
                  <script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js'></script>\
                  <script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js'></script>\
                  <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css'>\
                  <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap-theme.min.css'>\
                </head>\
                <body ng-controller='myCtrl'>\
                  <h1>ESP8266 Thermostat</h1><hr>\
                  <div style='color:#C00000'>{{status}}</div><hr>\
                  Temperature reading: {{data.temperatureCurrent}}<br/>\
                  Relay status: {{data.relayState}}<hr>\
                  <button class='btn btn-default' ng-click='turnOn()' ng-disabled=\"data.relayState==='true'\">On</button>\
                  <button class='btn btn-default' ng-click='turnOff()' ng-disabled=\"data.relayState==='false'\">Off</button><br/>\
                  Temperature setting: <input type='text' name='temp' value='{{data.temperatureSetting}}'>\
                  <button class='btn btn-default' ng-click='save()'>Save</button><br/>\
                  <script>\
                    var app = angular.module('myApp', []);\
                    app.controller('myCtrl', ['$scope','$http','$interval',function($scope, $http, $interval) {\
                        initialize();\
                        function initialize(){\
                          $scope.status='Getting data...';\
                          $http.get('/api/data').then(function(response) {console.log(response.data);$scope.data=response.data;$scope.status='Ready';});\
                        }\
                        $interval(function(){initialize();}.bind(this), 300000);\
                        $scope.turnOn = function(){\\
                          $scope.status='Turning on...';\
                          $http.get('/api/on').then(function(response) {initialize();});\
                        };\
                        $scope.turnOff = function(){\
                         $scope.status='Turning off...';\
                          $http.get('/api/off').then(function(response) {initialize();});\
                        };\
                        $scope.save = function(){\
                          $scope.status='Saving...';\
                          $http.get('/api/save',{params: { temperatureSetting: $scope.data.temperatureSetting }}).then(function(response) {$scope.status='Ready';});\
                        };\
                    }]);\
                  </script>\
                </body>\
              </html>";

void persist(){
  // Create JSON
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["temperatureSetting"] = temperatureSetting;
  root["relayState"] = relayState;
  root["temperatureCurrent"] = temperatureCurrent;
  
  // Remove old file
  if(SPIFFS.exists(storageFilePath)){
    SPIFFS.remove(storageFilePath);
  }

  // Store new file
  File file = SPIFFS.open(storageFilePath, "w");
  root.printTo(file);
  file.close();
  Serial.println("Persisting");
  root.printTo(Serial);
  Serial.println("Persisted");
  
}

String readFromStorage(){
   Serial.println("Reading");
   String str = "";
    File file = SPIFFS.open(storageFilePath, "r");
    if (!file) {
      Serial.println("Can't open SPIFFS file !\r\n");          
    }
    else {
     str = file.readString();
     Serial.println(str);
     Serial.println("Read");
     file.close();
     
     StaticJsonBuffer<200> jsonBuffer;
     JsonObject& root = jsonBuffer.parseObject(str);
     
     if (root.containsKey("temperatureSetting")){
        temperatureSetting = root["temperatureSetting"].asString();
     }

     if (root.containsKey("relayState")){
        relayState = root["relayState"].asString();
     }

     if (root.containsKey("temperatureCurrent")){
        temperatureCurrent = root["temperatureCurrent"];
     }
     return str;
    }
    return "";
}


void relayOn(){
  digitalWrite(RELAY_PIN, 1);
  relayState = "true";
  persist();
}

void relayOff(){
  digitalWrite(RELAY_PIN, 0);
  relayState = "false";
  persist();
}

void setup(void){
  SPIFFS.begin();
  pinMode(RELAY_PIN, OUTPUT);
  // restore previous state
  readFromStorage();
  if (relayState == "true"){
    relayOn();
  }else{
    relayOff();
  }
  
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", [](){
    server.send(200, "text/html", _index_html);
  });
  server.on("/api/on",[](){
    relayOn();
    server.send(200, "text/html", "");
  });
  server.on("/api/off",[](){
    relayOff();
    server.send(200, "text/html", "");
  });

  server.on("/api/save",[](){
    temperatureSetting = server.arg("temperatureSetting");
    
    persist();
    server.send(200, "text/html", "");
  });

  server.on("/api/data",[](){
    server.send(200, "application/json", readFromStorage());
  });

  server.begin();
  Serial.println("HTTP server started");
}

float readTemperature(){
  float temp;
  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    Serial.print("Temperature: ");
    Serial.println(temp);
  } while (temp == 85.0 || temp == (-127.0));
  
  return temp;
}

void readTime(){
  Serial.print("connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 13;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // This will send the request to the server
  client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");

  delay(100);

  // Read all the lines of the reply from server and print them to Serial
  // expected line is like : Date: Thu, 01 Jan 2015 22:00:14 GMT
  char buffer[12];
  String dateTime = "";

  while(client.available())
  {
    String line = client.readStringUntil('\r');

    if (line.indexOf("Date") != -1)
    {
      Serial.print("=====>");
    } else
    {
      // Serial.print(line);
      // date starts at pos 7
      TimeDate = line.substring(7);
      Serial.println(TimeDate);
    }
  }
}

void doThermostatThingy(){
  unsigned long now = millis();
   if (now - lastSampleTime >= timeBetweenMeasures){
      lastSampleTime += timeBetweenMeasures;
      // TODO: read temperature and store it to temperatureCurrent
      // temperatureCurrent = readTemperature();
      // persist();
      // Serial.println("Checking temperature. Current reading is:" + temperatureCurrent + " Setting is:" + temperatureSetting);
      if (temperatureCurrent < temperatureSetting.toFloat()){
        Serial.println("Start heating!");
        relayOn();
      }else{
        Serial.println("Stop heating!");
        relayOff();
      }
   }
}

void loop(void){
   doThermostatThingy();
   //readTime();
   server.handleClient();
}
