#include <Bounce2.h>
#include <AccelStepper.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include "SPIFFS.h"
#include <ESP32Encoder.h>
ESP32Encoder encoder;

// Pinout
#define ENDSTOP_1 22  // lang, hinten
#define ENDSTOP_2 23  // kurz, vorne
#define STEP 19
#define DIR 21

Bounce debouncer1 = Bounce(); 
Bounce debouncer2 = Bounce(); 

AccelStepper stepper(AccelStepper::DRIVER,STEP,DIR);

static const char ssid1[] = "BSWIFI";
static const char ssid2[] = "BSWIFIEXT";
static const char password[] = "7pi14159265";
MDNSResponder mdns;

WiFiMulti WiFiMulti;
WebServer server(80);

WebSocketsServer webSocket = WebSocketsServer(81);

boolean run = true;
//boolean serial_print =false;
boolean serial_print =true;
boolean forward = true;
int stroke = 0;
int max_stroke = 4200;  // offset plus stroke is constrained to this value so it is useful even if the max is set by the lookup table
int offset = 0;
int min_stroke = 0; // used to determine at what setting motion will begin and estop recovery occurs
int offset_plus_stroke = 0; 
int origin_offset = 0;  // origin and end offset are incremented when limit switches are hit 
int end_offset = 0;
int max_speed = 4000;   // not used for now since vibe and longer strokes use same max and are set by lookup table
int min_speed = 10;   // was 20
int readEncoder_1;
int encoder_multiplier;
int encoder_divider = 2;  // number of encoder ticks to read per motor tick
int minus_encoder_divider = -2;
long max_position = max_stroke;
long current_position = 0;

//int acceleration = 100000;  // this determines the ramping time to speed so needs to be carefully chosen to allow for fast short strokes and appropriate long strokes
int acceleration = 200000; // try lower values 
int motor_speed = 0;
char what='x';
String val="";
int value=0;

int speed_table[] = {// LOG 2 103 entries so div by 10 
        0 ,7 ,9 ,12 ,15 ,19 ,23 ,28 ,33 ,38 ,44 ,50 ,56 ,63 ,70 ,78 ,86 ,94 ,103 ,112 ,122 ,132 ,142 ,153 ,164 ,176 ,
        188 ,200 ,213 ,226 ,240 ,254 ,268 ,283 ,298 ,313 ,329 ,345 ,362 ,379 ,397 ,414 ,433 ,451 ,470 ,490 ,509 ,530 ,550 ,
        571 ,593 ,614 ,636 ,659 ,682 ,705 ,729 ,753 ,778 ,803 ,828 ,854 ,880 ,906 ,933 ,960 ,988 ,1016 ,1044 ,1073 ,1102 ,
        1132 ,1162 ,1192 ,1223 ,1254 ,1286 ,1318 ,1350 ,1383 ,1416 ,1450 ,1483 ,1518 ,1552 ,1588 ,1623 ,1659 ,1695 ,
        1732 ,1769 ,1806 ,1844 ,1882 ,1921 ,1960 ,2000,2100,2200,2300,2300,2500,2600,2700
}; // add a few extra to be sure

int stroke_table[] = {  // LOG 2 103 entries so div by 10
        0, 0 , 4 ,7 ,10 ,14 ,18 ,23 ,29 ,35 ,42 ,49 ,57 ,66 ,75 ,84 ,95 ,106 ,117 ,129 ,142 ,155 ,169 ,183 ,198 ,214 ,230 
        ,247 ,264 ,282 ,301 ,320 ,339 ,360 ,381 ,402 ,424 ,447 ,470 ,494 ,518 ,543 ,569 ,595 ,622 ,649 ,677 ,706 ,735 ,764 
        ,795 ,826 ,857 ,889 ,922 ,955 ,989 ,1023 ,1058 ,1094 ,1130 ,1167 ,1204 ,1242 ,1281 ,1320 ,1359 ,1400 ,1441 ,1482 
        ,1524 ,1567 ,1610 ,1654 ,1698 ,1743 ,1789 ,1835 ,1882 ,1929 ,1977 ,2025 ,2075 ,2124 ,2175 ,2225 ,2277 ,2329 
        ,2382 ,2435 ,2489 ,2543 ,2598 ,2654 ,2710 ,2767 ,2824 ,2882 ,2940 ,3000,3000,3000,3000,3000
}; // add a few extra to be sure

int offset_table[] ={ // 103 entries
    0 ,0 ,1 ,2 ,3 ,4 ,5 ,7 ,9 ,11 ,14 ,17 ,21 ,24 ,28 ,33 ,37 ,42 ,47 ,53 ,58 ,64 ,71 ,77 ,84 ,91 ,99 ,107 ,115 ,123 ,132 ,
    141 ,150 ,160 ,169 ,180 ,190 ,201 ,212 ,223 ,235 ,247 ,259 ,271 ,284 ,297 ,311 ,324 ,338 ,353 ,367 ,382 ,397 ,
    413 ,428 ,444 ,461 ,477 ,494 ,511 ,529 ,547 ,565 ,583 ,602 ,621 ,640 ,660 ,679 ,700 ,720 ,741 ,762 ,783 ,805 ,
    827 ,849 ,871 ,894 ,917 ,941 ,964 ,988 ,1012 ,1037 ,1062 ,1087 ,1112 ,1138 ,1164 ,1191 ,1217 ,1244 ,1271 ,
    1299 ,1327 ,1355 ,1383 ,1412 ,1441 ,1470 ,1500,1500,1500,1500,1500
}; // added a few extra to be sure

// it wil set the static IP address to 192, 168, 1, 184
IPAddress local_IP(192, 168, 1, 184);
//it wil set the gateway static IP address to 192, 168, 1,1
IPAddress gateway(192, 168, 1, 1);

// Following three settings are optional
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); 
IPAddress secondaryDNS(8, 8, 4, 4);

void setup() {
  ESP32Encoder::useInternalWeakPullResistors=true;
  encoder.attachFullQuad(32, 33); // Attache pins for use as encoder pins
  debouncer1.attach(ENDSTOP_1);
  debouncer1.interval(5); // interval in ms
  debouncer2.attach(ENDSTOP_2);
  debouncer2.interval(5); // interval in ms
    
  Serial.begin(115200);
//  Serial.println("setup begin");
  stepper.setMaxSpeed(max_speed);  // not really used but here for legacy purposes. max speed is set from read encoders
  stepper.setAcceleration(acceleration); 
  stepper.setPinsInverted(true,true,true);  
  stepper.setMinPulseWidth(20); // was 20
  stepper.setCurrentPosition(0)    ;
  current_position = 0; 

  pinMode(STEP, OUTPUT); // step
  pinMode(DIR, OUTPUT); // direction 
  pinMode(ENDSTOP_1, INPUT_PULLUP); // limit switch
  pinMode(ENDSTOP_2, INPUT_PULLUP); // limit switch
    
  /* Setup WiFi */
// This part of code will try create static IP address
 if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
 }
  WiFiMulti.addAP(ssid1, password);
//  WiFiMulti.addAP(ssid2, password);

  WiFi.mode(WIFI_STA);
  while(WiFiMulti.run() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Connect to http://espWebSock.local or http://");
  Serial.println(WiFi.localIP());

  /* Setup Webserver, Websocket and Filesystem */

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}
void loop() {
  webSocket.loop();
  read_encoder();
  server.handleClient();
  debouncer1.update();
  debouncer2.update();
  // Get the updated value :
  int value1 = debouncer1.read();
  int value2 = debouncer2.read();
  if ( value1 == LOW ) {
         cwpulse(); 
        delay (1);
//        end_offset++;   
  } 
  if ( value2 == LOW ) {
        ccwpulse(); 
        delay (1);
//        origin_offset++;        
  } 
      if (run) { 
        if (forward == true) {
          stepper.moveTo (offset_plus_stroke);
        }
        else {
          stepper.moveTo(offset);
        }
        stepper.run(); 
        if ((stepper.distanceToGo() == 0) && (stroke > 0)) {
          forward = !forward;       // reverse direction
        }
     }    
}

void cwpulse(void){
  digitalWrite(DIR, LOW); // pin 4 low for CW 
  delayMicroseconds(1);  //dir must preceed step by 200 ns
  digitalWrite(STEP, HIGH); 
  delayMicroseconds(1);      // needs to be 2, 1 is not enough for Chinese stepper might be enough for Gecko
  digitalWrite(STEP, LOW); 
//  delayMicroseconds(1);   
} 

void ccwpulse(void){
   digitalWrite(DIR, HIGH); 
   delayMicroseconds(1);  //dir must preceed step by 200 ns
   digitalWrite(STEP, HIGH); 
   delayMicroseconds(1);  
   digitalWrite(STEP, LOW); 
//   delayMicroseconds(1);   
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch(type) {
    case WStype_DISCONNECTED:
      {
      if (serial_print) Serial.printf("[%u] Disconnected!\r\n", num);
      motor_speed=0;
      offset_plus_stroke=0;
      offset=0;}
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        if (serial_print) Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }     
      break;
    case WStype_TEXT:
      {
        String buf = ((const char *)payload);
        what=payload[0];
        val=buf.substring(2);

        switch(what) {
          case 's':
            value=val.toInt();
            motor_speed=speed_table[value];
            stepper.setMaxSpeed(motor_speed);  
            if (serial_print) {
              Serial.print("speed: ");Serial.println(motor_speed);
            }
            break;
          case 't':
            value=val.toInt();
//            stroke=stroke_table[value] - end_offset;
            stroke=stroke_table[value];
            offset_plus_stroke = constrain ((offset + stroke),0,max_stroke);
            if (serial_print){
              Serial.print("move between: ");Serial.print(offset);Serial.print(" and ");Serial.println(offset_plus_stroke);
            }
            break;
          case 'o':
            value=val.toInt();
            offset=offset_table[value] - origin_offset;
            offset_plus_stroke = constrain ((offset + stroke),0,max_stroke);
            if (serial_print){
              Serial.print("move between: ");Serial.print(offset);Serial.print(" and ");Serial.println(offset_plus_stroke);
            }
            break;
          case 'p':
            if (serial_print){
              Serial.print("Machine: ");Serial.println(val);
            }
            if (val == "E-Stop") offset_plus();
            break;
         default:
            break;
        }
        run = true;
        if (motor_speed < min_speed) {
          run = false;  // change this to && to allow offset only ???
        }
      // send data to all connected clients
        webSocket.broadcastTXT(payload, length);
        break;
      }
   default:
      {
        if (serial_print) Serial.printf("Invalid WStype [%d]\r\n", type);
        break;
      }  
  }
}

void handleRoot() { // send the right file to the client (if it exists)
  String path = "/index.html";         // If a folder is requested, send the index file
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, "text/html"); // And send it to the client
    file.close();                                       // Then close the file again
  }
  Serial.println("\tFile Not Found");
}

void wifiConnect()
{
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    delay(1000);
    WiFi.begin(ssid1, password);
    return;
  }
}
void handleNotFound()
{
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
void read_encoder(){   // this is the main code. it acts on both joystick encoder and three knob controller
    readEncoder_1 = encoder.getCount();
       if (readEncoder_1 < 0) { 
          if (digitalRead(ENDSTOP_1) == HIGH) {
              cwpulse();
          }
        }          
        else if (readEncoder_1 > 0) {    
          if (digitalRead(ENDSTOP_2) == HIGH) {
              ccwpulse();
          }
        }    
        encoder.setCount(0);   // this must be here or it can accumulate high values and delay moving off limit switch

} 
void offset_plus(){
    offset = offset +1;
    offset_plus_stroke = constrain ((offset + stroke),0,max_stroke);
}
//  delay (2000);  // delay to make sure inputs are ready when read in setup
//  stepper.moveTo(-10000);
//  while (digitalRead(ENDSTOP_1)== HIGH){
//    stepper.run();
//  }
//  stepper.setCurrentPosition(0);
//  stepper.moveTo(10000);
//  while(digitalRead(ENDSTOP_2)==HIGH){
//    stepper.run();
//  }
//  max_position=stepper.currentPosition();
//  max_position-=100;
//  Serial.print("max pos=");Serial.println(max_position);
//  
//  stepper.setCurrentPosition(0);
//  stepper.moveTo(-max_position+50);
//  while (stepper.currentPosition() != -max_position+50) {
//    stepper.run();
//  }
//  stepper.setCurrentPosition(0);    
//  max_position = max_stroke;

//    while (ENDSTOP_1== HIGH){  // move to rear limit switch
//      ccwpulse();
//      delay (1);
//    }  
//    for (int i=0; i < 10; i++){ // be sure to be off switch for following test
//      cwpulse();
//      delay (1);
//    } 
