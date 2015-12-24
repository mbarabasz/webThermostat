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
String page = "<!DOCTYPE html>\
               <html ng-app='myApp'>\
                <head>\
                  <script src='http://ajax.googleapis.com/ajax/libs/angularjs/1.4.8/angular.min.js'></script>\
                  <script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js'></script>\
                  <script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js'></script>\
                  <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css'>\
                  <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap-theme.min.css'>\
                </head>\
                <body>\
                  <h1>Hello from esp8266!</h1><br/>\
                  Temperature reading: _temperatureCurrent_<br/>\
                  Relay is: _relayState_<br/>\
                  <a href='on'>on</a></a><br/>\
                  <a href='off'>off</a></a><br/>\
                  <form action='update' method='get'>\
                  Temperature setting: <input type='text' name='temp' value='_temperatureSetting_'><br>\
                  <input type='submit' value='Submit'>\
                  </form>\
                  <div  ng-controller='myCtrl'>\
                    <p>Input something in the input box:</p>\
                    <p>Name : <input type='text' ng-model='name' placeholder='Enter name here'></p>\
                    <h1>Hello {{name}}</h1>\
                  </div>\
                  <script>\
                    var app = angular.module('myApp', []);\
                    app.controller('myCtrl', function($scope) {\
                        $scope.firstName= 'John';\
                    });\
                  </script>\
                </body>\
              </html>";

void persist(){
  // Create storage
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["temperatureSetting"] = temperatureSetting;
  root["relayState"] = relayState;

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

void readFromStorage(){
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
    }
}

String getPage(){
  readFromStorage();
  String preview = page;
  preview.replace("_temperatureSetting_",temperatureSetting);
  preview.replace("_temperatureCurrent_",temperatureCurrent);
  preview.replace("_relayState_",relayState);
  return preview;
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
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
    server.send(200, "text/html", getPage());
  });
  server.on("/on",[](){
    digitalWrite(D1, 1);
    relayState = "true";
    persist();
    server.send(200, "text/html", getPage());
  });
  server.on("/off",[](){
    digitalWrite(D1, 0);
    relayState = "false";
    persist();
    server.send(200, "text/html", getPage());
  });

  server.on("/update",[](){
    temperatureSetting = server.arg(0);
    persist();
    server.send(200, "text/html", getPage());
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/info", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
}
