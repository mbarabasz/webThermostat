#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

const char* ssid = "barabasz";
const char* password = "My heart is beating";
ESP8266WebServer server(80);
String path ="/storage.json";
String temperatureSetting = "20";
String temperatureCurrent = "19";
String relayState = "false";
const unsigned long timeBetweenMeasures = 5 * 60 * 1000UL; // fiveMinutes
static unsigned long lastSampleTime = 0 - timeBetweenMeasures;  // initialize such that a reading is due the first time through loop()
  
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
                  <h1>ESP8266 Thermostat</h1><br/>\
                  Temperature reading: {{data.temperatureCurrent}}<br/>\
                  Relay status: {{data.relayState}}\
                  <button class='btn btn-default' ng-click='turnOn()' ng-disabled=\"data.relayState==='true'\">On</button>\
                  <button class='btn btn-default' ng-click='turnOff()' ng-disabled=\"data.relayState==='false'\">Off</button><br/>\
                  Temperature setting: <input type='text' name='temp' value='{{data.temperatureSetting}}'>\
                  <button class='btn btn-default' ng-click='save()'>Save</button><br/>\
                  <script>\
                    var app = angular.module('myApp', []);\
                    app.controller('myCtrl', ['$scope','$http',function($scope,$http) {\
                        initialize();\
                        function initialize(){\
                          $http.get('/api/data').then(function(response) {console.log(response.data);$scope.data=response.data;});\
                        }\
                        $scope.turnOn = function(){\
                          $http.get('/api/on').then(function(response) {initialize();});\
                        };\
                        $scope.turnOff = function(){\
                          $http.get('/api/off').then(function(response) {initialize();});\
                        };\
                        $scope.save = function(){\
                          $http.get('/api/save',{params: { temperatureSetting: $scope.data.temperatureSetting }}).then(function(response) {});\
                        };\
                    }]);\
                  </script>\
                </body>\
              </html>";

void persist(){
  // Create storage
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["temperatureSetting"] = temperatureSetting;
  root["relayState"] = relayState;
  root["temperatureCurrent"] = temperatureCurrent;
  
  // remove old file
  if(SPIFFS.exists(path)){
    SPIFFS.remove(path);
  }

  // store new file
  File file = SPIFFS.open(path, "w");
  root.printTo(file);
  file.close();
  Serial.println("Written");
  
}

String readFromStorage(){
  Serial.println("Reading value");
   String str = "";
    File file = SPIFFS.open(path, "r");
    if (!file) {
      Serial.println("Can't open SPIFFS file !\r\n");          
    }
    else {
     str = file.readString();
     Serial.println("Value read:");
     Serial.println(str);
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
        temperatureCurrent = root["temperatureCurrent"].asString();
     }
     return str;
    }
    return "";
}


void setup(void){
  SPIFFS.begin();
  pinMode(D1, OUTPUT);

  readFromStorage();
  if (relayState == "true"){
    digitalWrite(D1, 1);
  }else{
    digitalWrite(D1, 0);
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
    digitalWrite(D1, 1);
    relayState = "true";
    persist();
    server.send(200, "text/html", "");
  });
  server.on("/api/off",[](){
    digitalWrite(D1, 0);
    relayState = "false";
    persist();
    server.send(200, "text/html", "");
  });

  server.on("/api/save",[](){
    temperatureSetting = server.arg(0);
    persist();
    server.send(200, "text/html", "");
  });

  server.on("/api/data",[](){
    server.send(200, "application/json", readFromStorage());
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
   unsigned long now = millis();
   if (now - lastSampleTime >= timeBetweenMeasures){
      lastSampleTime += timeBetweenMeasures;
      Serial.println("Checking temperature");
      // read temperature and store it to temperatureCurrent
      if (temperatureCurrent < temperatureSetting){
        Serial.println("Heating!");
        digitalWrite(D1, 1);
      }else{
        Serial.println("No more heating!");
        digitalWrite(D1, 0);
      }
   }
   server.handleClient();
}
