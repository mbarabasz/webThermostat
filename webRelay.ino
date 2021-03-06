 #include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266HTTPClient.h>

#define ONE_WIRE_BUS D2  // DS18B20 pin
#define RELAY_PIN D1 // Relay pin

const char* ssid = "barabasz";
const char* password = "My heart is beating";
const unsigned long timeBetweenMeasures = 5 * 60 * 1000UL; // fiveMinutes
String thingSpeakApiKey="YCBDILDHYMXXBLUE";
String thingSpeakFieldName="field1";
String pageUsername= "admin";
String pagePassword= "admin";
String storageFilePath ="/storage.json";
String temperatureSetting = "20"; //Defaults only for case when there is no storage yet
float temperatureCurrent = 19.0;
String relayState = "false";
static unsigned long lastSampleTime = 0 - timeBetweenMeasures;  
String TimeDate;
boolean automaticTemperature;

HTTPClient http;
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
                  <label>Automatic: \
                  <input type='checkbox' ng-model='data.automaticTemperature' ng-true-value='1' ng-false-value='0'>\
                  </label><br/>\
                  <div ng-hide='data.automaticTemperature'>\
                  <button class='btn btn-default' ng-click='turnOn()' ng-disabled=\"data.relayState==='true'\">On</button>\
                  <button class='btn btn-default' ng-click='turnOff()' ng-disabled=\"data.relayState==='false'\">Off</button><br/>\
                  </div>\
                  <div ng-show='data.automaticTemperature'>\
                  Temperature setting: <input type='text' name='temp' ng-model='data.temperatureSetting'>\
                  </div>\
                  <button class='btn btn-default' ng-click='save()'>Save</button><br/>\
                  <div class='container'>\
                  <p class='text-muted text-center'>&copy; 2016 Marcin Barabasz</p>\
                  </div>\
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
                          $http.get('/api/save',{params: { temperatureSetting: $scope.data.temperatureSetting, automaticTemperature: $scope.data.automaticTemperature }}).then(function(response) {initialize();});\
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
  root["automaticTemperature"] = automaticTemperature;
  
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

     if (root.containsKey("automaticTemperature")){
        automaticTemperature = root["automaticTemperature"];
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

//Check if header is present and correct
bool is_authentified(){
  Serial.println("Enter is_authentified");
  if (server.hasHeader("Cookie")){   
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;  
}

//login page, also called for disconnect
void handleLogin(){
  String msg;
  if (server.hasHeader("Cookie")){   
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")){
    Serial.println("Disconnection");
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  if (server.hasArg("USERNAME") && server.hasArg("PASSWORD")){
    if (server.arg("USERNAME") == pageUsername &&  server.arg("PASSWORD") == pagePassword ){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      Serial.println("Log in Successful");
      return;
    }
  msg = "Wrong username/password! try again.";
  Serial.println("Log in Failed");
  }
  String content = "<html><body><form action='/login' method='POST'>To log in, please use : admin/admin<br>";
  content += "User:<input type='text' name='USERNAME' placeholder='user name'><br>";
  content += "Password:<input type='password' name='PASSWORD' placeholder='password'><br>";
  content += "<input type='submit' name='SUBMIT' value='Submit'></form>" + msg + "<br>";
  content += "You also can go <a href='/inline'>here</a></body></html>";
  server.send(200, "text/html", content);
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

  server.on("/", [](){
    if (!is_authentified()){
      String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      return;
    }
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
    if (server.arg("automaticTemperature") == "1"){
      automaticTemperature = true;
    }else{
      automaticTemperature = false;
    }
    persist();
    server.send(200, "text/html", "");
  });

  server.on("/api/data",[](){
    server.send(200, "application/json", readFromStorage());
  });

  server.on("/login", handleLogin);


  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
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

void doSendToThingSpeak(){
  Serial.print("Sending to thingspeak");
  http.begin("api.thingspeak.com", 80, "/update?api_key=" + thingSpeakApiKey + "&" + thingSpeakFieldName + "=" + String(temperatureCurrent)); 
  int httpCode = http.GET();
  Serial.print("Connected");
  if(httpCode) {
    if(httpCode == 200) {
      Serial.print("Sending to thingspeak done!");
    }else{
      Serial.print("Problem sending to thingspeak httpCode=" + httpCode);
    }
  }else{
    Serial.print("Problem sending to thingspeak couldn't get anything");
  }
}


void doThermostat(){
  unsigned long now = millis();
   if (now - lastSampleTime >= timeBetweenMeasures){
      lastSampleTime += timeBetweenMeasures;
      temperatureCurrent = readTemperature();
      doSendToThingSpeak();
      persist();
      Serial.print("Checking temperature. Current reading is: ");
      Serial.print(temperatureCurrent);
      Serial.print(" Temperature setting is: ");
      Serial.print(temperatureSetting);
      Serial.println();
      if (automaticTemperature){
        if (temperatureCurrent < temperatureSetting.toFloat()){
          Serial.println("Start heating!");
          relayOn();
        }else{
          Serial.println("Stop heating!");
          relayOff();
        }
      }
   }
}

void loop(void){
   doThermostat();
   server.handleClient();
}
