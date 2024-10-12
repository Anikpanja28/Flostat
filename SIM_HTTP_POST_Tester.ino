#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
HardwareSerial SerialAT(2);




#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif
#define TINY_GSM_YIELD() { delay(2); }



float Speed;

boolean debug = false;

float tempc, pres, hum;
bool bmeCond = false;
bool bmestatus;

bool gotUTC = false;
int32_t rtcOffset = -45;
int16_t offsetDays = 0; 
int8_t offsetHours = -5; 
int8_t offsetMinutes = -30; 
int8_t offsetSeconds = 16;
bool initiatedRTC = false;

int Year;
int Month;
int Day;
int Hour;
int Minute;
int Second;

bool enter_loop = false;
const char apn[]  = "airtelgprs.com"; // Change this to your Provider details
const char gprsUser[] = "";
const char gprsPass[] = "";
const char server[] = "34.201.127.182"; // Change this to your selection
const char resource[] = "/data";
const int  port = 8080;
unsigned long timeout;
int status;
int loop_enter = 5;
int iter = 0;
int last_sent_iter = 0;
int endIndex, startIndex;

const char tserver[] = "worldtimeapi.org";
const char tresource[] = "/api/timezone/Asia/Kolkata";
const int tport = 80;

// Buffer to store the JSON file content
char buffer[40000];
String postData="";
String rhtData = "";
String rainData = "";
String windData = "";

char acc[7], eventAcc[7], totalAcc[7], rInt[7], unit[4];
bool rainFlag = false;

unsigned long tim = millis();
unsigned long main_time;
unsigned long present_time;
String utc = "";

char timeString[20];
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoHttpClient.h>
#include <base64.h>
//#include <TimeLib.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include "RTClib.h"
#include <TinyGsmClient.h>


TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

HttpClient    http(client, server, port);

HttpClient    timec(client, tserver, tport);


time_t current_time;


void networkConnection(const char* apn, const char* gprsUser,const char* gprsPass,const char* server, const int port){
  SerialMon.print("Waiting for network...");
    modem.restart();
 if (!modem.waitForNetwork()) {
   SerialMon.println("Fail");
    delay(1000);
   return;
  }

 SerialMon.println("Success");

 if (modem.isNetworkConnected()) {
   SerialMon.println("Network connected");
     }

 SerialMon.print(F("Connecting to "));
 SerialMon.print(apn);
 
 if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
   SerialMon.println(" fail");
   

   delay(1000);
   return;
 }
 SerialMon.println(" success");
 
 
 if (modem.isGprsConnected()) {
   SerialMon.println("GPRS connected");
   
 delay(1000);

 }

 if (!modem.isGprsConnected()) {
   SerialMon.println("GPRS not connected");
   
 delay(1000);

 }

 if (!client.connect(server, port)) {

   SerialMon.println(" Failed to connect to the server");
   
 }
  if (client.connect(server, port)) {

   SerialMon.println(" Successfully connected to the server");
   
 
  }

}

void collectSensorData(){
  
   postData += "{\"utc\":\"";
   postData += "2023-10-09T15:15:11";
   postData += "\",\"tempc\":";
   postData += "31.0";
   postData += ",\"pres\":";
   postData += "100004";
   postData += ",\"hum\":";
   postData += "70";
   postData += ",\"windDir\":";
   postData += String("0");
   postData += ",\"windSpd\":";
   postData += String("0.00");
   postData += ",\"rainfall\":";
   postData += String(atof("5.954"),3);
   postData += ",\"rainacc\":";
   postData += String(atof("0.000"),3);
   postData += ",\"rainflag\":";
   postData += String("False");
   postData += ",\"iter\":";
   postData += iter;
   postData += "}";
   iter++;
   rainFlag = false;

  
    SerialMon.println("postData");
    SerialMon.println(postData);
}


void sendData(){


  String authtext = "Basic ";
  String auth = "admin1:1qazxsdC";
  String authenc = base64::encode(auth);
  authtext += authenc;

  SerialMon.println("Performing client POST request...");

 http.beginRequest();
 http.post(resource);
 http.sendHeader("Authorization", authtext);
 http.sendHeader("Content-Type", "application/json");
 http.sendHeader("Content-Length", postData.length());
 http.beginBody();
 http.print(postData);
 http.endRequest();

 status = http.responseStatusCode();
 SerialMon.print(F("Response status code: "));
 SerialMon.println(status);

  if (status == 200){
  postData = "";
  SerialMon.println("Data sent successfully");
  }

  if (status != 200){  
  postData = "";
  SerialMon.println("Data could not be sent successfully");
  http.stop();
 SerialMon.println(F("Server disconnected"));
 modem.gprsDisconnect();
 SerialMon.println(F("GPRS disconnected"));
    networkConnection(apn, gprsUser, gprsPass, server, port);
  
  }


}





void setup() {
  // put your setup code here, to run once:
  SerialMon.begin(115200);
  SerialAT.begin(115200,SERIAL_8N1, 16, 17, false);
  SerialMon.println("Initializing modem...");
  networkConnection(apn, gprsUser, gprsPass, server, port);



  
}

void loop() {
  // put your main code here, to run repeatedly:
 collectSensorData();
  sendData();
}
