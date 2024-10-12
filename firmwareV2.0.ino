// Arduino file start here
#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
HardwareSerial SerialAT(2);
HardwareSerial SerialRG(1);


#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif
#define TINY_GSM_YIELD() { delay(2); }

#define SEALEVELPRESSURE_HPA (1013.25)

#define windSpeedPin 25
#define windDirPin 26
#define windSpeedINT 0 // INT0
#define windDirINT 1   // INT1

// Pin 13 has an LED connected on most Arduino boards.
int LED = 13;
 int sdSuccess = 0;
float floatDataNotFound = -4000.0;
int intDataNotFound = -4000;
int sensorResponse;
int j=0;                                          
const unsigned long DEBOUNCE = 10000ul;      // Minimum switch time in microseconds
const unsigned long DIRECTION_OFFSET = 0ul;  // Manual direction offset in degrees, if required
const unsigned long TIMEOUT = 1500000ul;       // Maximum time allowed between speed pulses in microseconds
const unsigned long UPDATE_RATE = 500ul;     // How often to send out NMEA data in milliseconds
const float filterGain = 0.25;               // Filter gain on direction output filter. Range: 0.0 to 1.0
                                             // 1.0 means no filtering. A smaller number increases the filtering

// Knots is actually stored as (Knots * 100). Deviations below should match these units.
const int BAND_0 =  10 * 100;
const int BAND_1 =  80 * 100;

const int SPEED_DEV_LIMIT_0 =  5 * 100;     // Deviation from last measurement to be valid. Band_0: 0 to 10 knots
const int SPEED_DEV_LIMIT_1 = 10 * 100;     // Deviation from last measurement to be valid. Band_1: 10 to 80 knots
const int SPEED_DEV_LIMIT_2 = 30 * 100;     // Deviation from last measurement to be valid. Band_2: 80+ knots

// Should be larger limits as lower speed, as the direction can change more per speed update
const int DIR_DEV_LIMIT_0 = 25;     // Deviation from last measurement to be valid. Band_0: 0 to 10 knots
const int DIR_DEV_LIMIT_1 = 18;     // Deviation from last measurement to be valid. Band_1: 10 to 80 knots
const int DIR_DEV_LIMIT_2 = 10;     // Deviation from last measurement to be valid. Band_2: 80+ knots

volatile unsigned long speedPulse = 0ul;    // Time capture of speed pulse
volatile unsigned long dirPulse = 0ul;      // Time capture of direction pulse
volatile unsigned long speedTime = 0ul;     // Time between speed pulses (microseconds)
volatile unsigned long directionTime = 0ul; // Time between direction pulses (microseconds)
volatile boolean newData = false;           // New speed pulse received
volatile unsigned long lastUpdate = 0ul;    // Time of last serial output

volatile int knotsOut = 0;    // Wind speed output in knots * 100
volatile int dirOut = 0;      // Direction output in degrees
volatile boolean ignoreNextReading = false;
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

Adafruit_BME280 bme;
RTC_DS3231 rtc;
DateTime now;

#include <TinyGsmClient.h>
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

HttpClient    http(client, server, port);

HttpClient    timec(client, tserver, tport);


time_t current_time;


String parseTimeJson(String timeJson, String id) {
  String key;
  String val;
 
  bool col = false;
  bool found = false;
 
  for (int i = 0; i < timeJson.length(); i++) {
   if (timeJson[i] == '"' || timeJson[i] == '\'' || timeJson[i] == '{') {
     i++;
   }
 
   if (timeJson[i] == ':' && !col) {
     col = true;
     if (key.equals(id)) {
       found = true;
     }
     i++;
   } else if ((timeJson[i] == ',' || timeJson[i] == '}' || timeJson[i] == '"') && found) {
     col = false;
     return val;
   } else if (timeJson[i] == ',') {
     col = false;
     i++;
   } else if (timeJson[i] == '}') {
     String failed = "Fail";
     return failed;
   }
 
   if (timeJson[i] == '"' || timeJson[i] == '\'') {
     i++;
   }
 
   if (!col) {
     val = "";
     key.concat(timeJson[i]);
   } else {
     key = "";
     val.concat(timeJson[i]);
   }
  }
}

void initiateSD(){
  if(!SD.begin()){
      sdSuccess = 0;
    Serial.println("Card Mount Failed");
   
  }

  else{
    delay(500);
    sdSuccess = 1;
    SerialMon.println("SD success");
  }
}
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  // Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    // Serial.println("Dir created");
    Serial.println("");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  // Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    // Serial.println("Dir removed");
    Serial.println("");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  // Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
  }

  // Serial.print("Read from file: ");
  while(file.available()){
  Serial.write(file.read());
  }
  file.close();
 
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  // Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    // Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
    Serial.println("");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
//   Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
      Serial.println("Failed to open file for appending");
    sdSuccess = 0;
    return;
  }
    if(file.print(message)){
//        Serial.println("Message appended");
//        Serial.println("");
        sdSuccess = 1;
    } else {
      sdSuccess = 0;
      Serial.println("Append failed");
    }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    // Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    // Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  // Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

IRAM_ATTR void readWindSpeed(){

    // Despite the interrupt being set to FALLING edge, double check the pin is now LOW
    if (((micros() - speedPulse) > DEBOUNCE) && (digitalRead(windSpeedPin) == LOW))
    {
        // Work out time difference between last pulse and now
        speedTime = micros() - speedPulse;

        // Direction pulse should have occured after the last speed pulse
        if (dirPulse - speedPulse >= 0) directionTime = dirPulse - speedPulse;

        newData = true;
        speedPulse = micros();    // Capture time of the new speed pulse
    }
}

IRAM_ATTR void readWindDir(){
    if (((micros() - dirPulse) > DEBOUNCE) && (digitalRead(windDirPin) == LOW))
    {
      dirPulse = micros();        // Capture time of direction pulse
    }
}

boolean checkDirDev(long knots, int dev){
    if (knots < BAND_0)
    {
        if ((abs(dev) < DIR_DEV_LIMIT_0) || (abs(dev) > 360 - DIR_DEV_LIMIT_0)) return true;
    }
    else if (knots < BAND_1)
    {
        if ((abs(dev) < DIR_DEV_LIMIT_1) || (abs(dev) > 360 - DIR_DEV_LIMIT_1)) return true;
    }
    else
    {
        if ((abs(dev) < DIR_DEV_LIMIT_2) || (abs(dev) > 360 - DIR_DEV_LIMIT_2)) return true;
    }
    return false;
}

boolean checkSpeedDev(long knots, int dev){
    if (knots < BAND_0)
    {
        if (abs(dev) < SPEED_DEV_LIMIT_0) return true;
    }
    else if (knots < BAND_1)
    {
        if (abs(dev) < SPEED_DEV_LIMIT_1) return true;
    }
    else
    {
        if (abs(dev) < SPEED_DEV_LIMIT_2) return true;
    }
    return false;
}

void calcWindSpeedAndDir(){
    unsigned long dirPulse_, speedPulse_;
    unsigned long speedTime_;
    unsigned long directionTime_;
    long windDirection = 0l, rps = 0l, knots = 0l;

    static int prevKnots = 0;
    static int prevDir = 0;
    int dev = 0;
    int intermediateDir;
    float intermediateSpd;


    // Get snapshot of data into local variables. Note: an interrupt could trigger here
    noInterrupts();
    dirPulse_ = dirPulse;
    speedPulse_ = speedPulse;
    speedTime_ = speedTime;
    directionTime_ = directionTime;
    interrupts();

    // Make speed zero, if the pulse delay is too long
    if (micros() - speedPulse_ > TIMEOUT) speedTime_ = 0ul;

    // The following converts revolutions per 100 seconds (rps) to knots x 100
    // This calculation follows the Peet Bros. piecemeal calibration data
    if (speedTime_ > 0)
    {
        rps = 100000000/speedTime_;                  //revolutions per 100s

        if (rps < 323)
        {
          knots = (rps * rps * -11)/11507 + (293 * rps)/115 - 12;
        }
        else if (rps < 5436)
        {
          knots = (rps * rps / 2)/11507 + (220 * rps)/115 + 96;
        }
        else
        {
          knots = (rps * rps * 11)/11507 - (957 * rps)/115 + 28664;
        }
        //knots = mph * 0.86897

        if (knots < 0l) knots = 0l;  // Remove the possibility of negative speed
        // Find deviation from previous value
        dev = (int)knots - prevKnots;

        // Only update output if in deviation limit
        if (checkSpeedDev(knots, dev))
        {
          knotsOut = knots;

          // If speed data is ok, then continue with direction data
          if (directionTime_ > speedTime_)
          {
              windDirection = 999;    // For debugging only (not output to knots)
          }
          else
          {
            // Calculate direction from captured pulse times
            windDirection = (((directionTime_ * 360) / speedTime_) + DIRECTION_OFFSET) % 360;
            if (windDirection<0) windDirection +=360;
            Serial.print("Wind direction");
            Serial.println(windDirection);
            
            // Find deviation from previous value
            dev = (int)windDirection - prevDir;

            // Check deviation is in range
            if (checkDirDev(knots, dev))
            {
              int delta = ((int)windDirection - dirOut);
              if (delta < -180)
              {
                delta = delta + 360;    // Take the shortest path when filtering
              }
              else if (delta > +180)
              {
                delta = delta - 360;
              }
              // Perform filtering to smooth the direction output
              dirOut = (dirOut + (int)(round(filterGain * delta))) % 360;
              if (dirOut < 0) dirOut = dirOut + 360;
            }
            prevDir = windDirection;
          }
        }
        else
        {
          ignoreNextReading = true;
        }

        prevKnots = knots;    // Update, even if outside deviation limit, cause it might be valid!?
    }
    else
    {
        knotsOut = 0;
        prevKnots = 0;
    }

    if (debug)
    {    
        Serial.println("DEBUG");
        Serial.print("$WIMWV,");
        Serial.print(dirOut);
        Serial.print(".0,R,");
        Serial.print(knotsOut/100);
        Serial.print(",N,A*");
        Serial.println();
        Serial.print(",");
        Serial.print(knots/100);
        Serial.print(",");
        Serial.println(rps);
    }
    else
    {
      if (millis() - lastUpdate > UPDATE_RATE)
      {
        printWindNmea();
        lastUpdate = millis();
      }
    }
}


/*=== MWV - Wind Speed and Angle ===
 *
 * ------------------------------------------------------------------------------
 *         1   2 3   4 5
 *         |   | |   | |
 *  $--MWV,x.x,a,x.x,a*hh<CR><LF>
 * ------------------------------------------------------------------------------
 *
 * Field Number:
 *
 * 1. Wind Angle, 0 to 360 degrees
 * 2. Reference, R = Relative, T = True
 * 3. Wind Speed
 * 4. Wind Speed Units, K/M/N
 * 5. Status, A = Data Valid
 * 6. Checksum
 *
 */
void printWindNmea(){
    char windSentence[30];
    float spd = knotsOut / 100.0;
    byte cs;
    //Assemble a sentence of the various parts so that we can calculate the proper checksum

     Serial.print("$WIMWV,");
     Serial.print(dirOut);
     Serial.print(".0,R,");
     Serial.print(spd);
     Serial.print(",N,A*");
     Serial.println();
    //calculate the checksum

}





void SDlogger(String dataMessage){

  SerialMon.print("Save data: ");
  SerialMon.println(dataMessage);
  appendFile(SD, "/logger.txt", dataMessage.c_str());

}


void networkConnection(const char* apn, const char* gprsUser,const char* gprsPass,const char* server, const int port){
  SerialMon.print("Waiting for network...");
  appendFile(SD, "/logger.txt", "[INFO] [SERVER] - Waiting for network\r\n");
  modem.restart();
 if (!modem.waitForNetwork()) {
   SerialMon.println("Fail");
   appendFile(SD, "/logger.txt", "[ERROR] [SERVER] - Failed to connect to the network\r\n");
   delay(1000);
   return;
  }

 SerialMon.println("Success");

 if (modem.isNetworkConnected()) {
   SerialMon.println("Network connected");
     appendFile(SD, "/logger.txt", "[INFO] [SERVER] - Connected to the network\r\n");
 }

 SerialMon.print(F("Connecting to "));
 SerialMon.print(apn);
 appendFile(SD, "/logger.txt", "[INFO] [SERVER] - Connecting to the apn ");
 appendFile(SD, "/logger.txt", apn);
 appendFile(SD, "/logger.txt", "\r\n");

 if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
   SerialMon.println(" fail");
   
 appendFile(SD, "/logger.txt", "[ERROR] [SERVER] - Failed to connect to the Access Point Name\r\n");

   delay(1000);
   return;
 }
 SerialMon.println(" success");
 
 appendFile(SD, "/logger.txt", "[INFO] [SERVER] - Successfully connected to the Access Point Name\r\n");

 if (modem.isGprsConnected()) {
   SerialMon.println("GPRS connected");
   
 appendFile(SD, "/logger.txt", "[INFO] [SERVER] - Connected to the GPRS\r\n");
 delay(1000);

 }

 if (!modem.isGprsConnected()) {
   SerialMon.println("GPRS not connected");
   
 appendFile(SD, "/logger.txt", "[ERROR] [SERVER] - Not connected to the GPRS\r\n");
 delay(1000);

 }

 if (!client.connect(server, port)) {

   SerialMon.println(" Failed to connect to the server");
   
 appendFile(SD, "/logger.txt", "[ERROR] [SERVER] - Failed to connect to the server\r\n");
 }
  if (client.connect(server, port)) {

   SerialMon.println(" Successfully connected to the server");
   
 appendFile(SD, "/logger.txt", "[INFO] [SERVER] Successfully connected to the server\r\n");

  }

}

int initiateRHTPsensor(){
  bmestatus = bme.begin(0x76);  
  if (!bmestatus) {
    SerialMon.print("Could not find a valid BME280 sensor, check wiring!");
    SerialMon.println();
      logData("Could not find a valid BME280 sensor", "BME280", 1);
      sensorResponse = getRHTPData();
      logData("BME280 is not integrated", "BME280", 1);
  }

  else{
    SerialMon.println("BME280 found");
     SerialMon.println(); 
          
          logData("BME280 found", "BME280", 0);
         sensorResponse = getRHTPData();
          if (sensorResponse == 200){
        logData("BME280 is integrated", "BME280", 0);
        
      }
          else{
            logData("BME280 is not integrated", "BME280", 1);
          }
  
  }
  return sensorResponse;
}

int initiateRainsensor(){

  SerialRG.begin(9600,SERIAL_8N1,4,2, false);
  SerialMon.println("Initializing Rain sensor\n");
  logData("Initializing Rain sensor", "RAIN", 0);
  sensorResponse = getRainData();
  if (sensorResponse == 200){
    SerialMon.println("Integrated Rain sensor\n");
    logData("Integrated Rain sensor", "RAIN", 0);
  }
  delay(1000);
  return sensorResponse;
}

int initiateWindsensor(){
  pinMode(windSpeedPin, INPUT);
    attachInterrupt(windSpeedPin, readWindSpeed, FALLING);


    pinMode(windDirPin, INPUT);
    attachInterrupt(windDirPin, readWindDir, FALLING);
    SerialMon.println("Initializing Wind Speed and Direction sensor\n");
    logData("Initializing Wind Speed and Direction sensor", "WIND", 0);
    sensorResponse = getWindData();
    if (sensorResponse == 200){
       SerialMon.println("Integrated Wind Speed and Direction sensor\n");
    logData("Integrated Wind Speed and Direction sensor", "WIND", 0);

    }
  interrupts();
  return sensorResponse;
}
int getRHTPData(){

  tempc = bme.readTemperature();
  pres = bme.readPressure();
  hum = bme.readHumidity();

  if(!isnan(tempc) && !isnan(hum) && !isnan(pres)){
    rhtData += "{\"tempc\":\"";
   rhtData += tempc;
   rhtData += "\",\"pres\":";
   rhtData += pres;
   rhtData += ",\"hum\":";
   rhtData += hum;
   rhtData += "}";
   
    SerialMon.println("Got RHTP data\n");
    SerialMon.println("Temp");
    SerialMon.println(tempc);
        SerialMon.println("Hum");
    SerialMon.println(hum);
        SerialMon.println("Pres");
    SerialMon.println(pres);
    logData("Got RHTP data", "BME280", 0);
    logData(rhtData, "BME280", 0);
     rhtData = "";
    return 200;
  }
  else{

    if(isnan(tempc)){
    tempc = floatDataNotFound;
    }
    if(isnan(pres)){
    pres = floatDataNotFound;
    }
    if(isnan(hum)){
    hum = floatDataNotFound;
    }
  
  
   rhtData += "{\"tempc\":\"";
   rhtData += tempc;
   rhtData += "\",\"pres\":";
   rhtData += pres;
   rhtData += ",\"hum\":";
   rhtData += hum;
   rhtData += "}";
   
    SerialMon.println("Didn't get RHTP data\n");
    SerialMon.println("Temp");
    SerialMon.println(tempc);
        SerialMon.println("Hum");
    SerialMon.println(hum);
        SerialMon.println("Pres");
    SerialMon.println(pres);
    logData("Didn't get RHTP data", "BME280", 1);
    logData(rhtData, "BME280", 1); 
    bme.begin(0x76);
     rhtData = "";  
    return 400;
  }

 
}

int getRainData(){

  SerialRG.write('r');
  SerialRG.write('\n');
  String response = SerialRG.readStringUntil('\n');
  SerialMon.println(response);
  
    if (response.length() > 0 && response.charAt(0) == 'A'){
    sscanf(response.c_str(),"%*s %s %[^,] , %*s %s %*s %*s %s %*s %*s %s",
          &acc, &unit, &eventAcc, &totalAcc, &rInt);
  SerialMon.println("Got Rainfall data\n");

  logData("Got rainfall data", "RAIN", 0);
  
  SerialMon.println("Acc");
  SerialMon.println(acc);
  SerialMon.println("eventAcc");
  SerialMon.println(eventAcc);
  SerialMon.println("totalAcc");
  SerialMon.println(totalAcc);
  SerialMon.println("rInt");
  SerialMon.println(rInt);

    rainData = "";
   rainData += "{\"Acc\":\"";
   rainData += acc;
   rainData += "\",\"eventAcc\":";
   rainData += eventAcc;
   rainData += ",\"totalAcc\":";
   rainData += totalAcc;
   rainData += ",\"rInt\":";
   rainData += rInt;
   rainData += "}";

   logData(rainData, "RAIN", 0);
   rainData = "";
   return 200;
  }

  else{
    rainData = "";
    return 400;
  }
  
  
}

int getWindData(){
  int i;
  const unsigned int LOOP_DELAY = 50;
  const unsigned int LOOP_TIME = TIMEOUT / LOOP_DELAY;

  i = 0;
  // If there is new data, process it, otherwise wait for LOOP_TIME to pass
  while ((newData != true) && (i < LOOP_TIME))
  {
    i++;
    delayMicroseconds(LOOP_DELAY);
  }

  calcWindSpeedAndDir();    // Process new data
  Speed = knotsOut/100.0;
  
  
    SerialMon.println("Got Wind speed and direction data\n");
  logData("Got Wind Speed and Direction data", "WIND", 0);
  
  Serial.println("Dir");
  Serial.println(dirOut);
  Serial.println("Speed");
  Serial.println(knotsOut/100.0);
  newData = false;
  
   windData += "{\"windSpd\":\"";
   windData += String(Speed);
   windData += "\",\"windDir\":";
   windData += String(dirOut);
   windData += "}";

  logData(windData, "WIND", 0);
  if (windData.length()>0){
    windData = "";
    return 200;
  }
  else{
    windData = "";
    return 400;
  }
  
}

void collectSensorData(){
    int tempResponse;
    tempResponse = getRHTPData();
    tempResponse = getWindData();
    tempResponse = getRainData();
    delay(500);
  
   postData += "{\"utc\":\"";
   postData += timeString;
   postData += "\",\"tempc\":";
   postData += tempc;
   postData += ",\"pres\":";
   postData += pres;
   postData += ",\"hum\":";
   postData += hum;
   postData += ",\"windDir\":";
   postData += String(dirOut);
   postData += ",\"windSpd\":";
   postData += String(Speed);
   postData += ",\"rainfall\":";
   postData += String(atof(totalAcc),3);
   postData += ",\"rainacc\":";
   postData += String(atof(acc),3);
   postData += ",\"rainflag\":";
   postData += String(rainFlag);
   postData += ",\"iter\":";
   postData += iter;
   postData += "}";
   iter++;
   rainFlag = false;
    logData(postData, "SENSOR", 0);
  
    SerialMon.println("postData");
    SerialMon.println(postData);
}

void getUTCTime(){

 SerialMon.println("Connecting to the time server");
  int err = timec.get(tresource);
  SerialMon.println(err);
 if (err != 0) {
   SerialMon.println(F("Failed to get time"));
  // logData("Successfull at getting time", "TIMESERVER", 0);
  return;
 }

 else{
   SerialMon.println(F("Successfull at getting time"));
    
 }
  SerialMon.println("Getting response");
  String tem = timec.responseBody();
 if (tem.length()>0){
    gotUTC = true;
    SerialMon.println("Response");
    SerialMon.println(tem);
    SerialMon.println("Post response");

    String key = "utc_datetime";
    SerialMon.println("Post key");
    utc = parseTimeJson(tem, key);
    SerialMon.println("Post UTC");
    utc.concat("Z");
    SerialMon.println(utc);
 
    Year = utc.substring(0, 4).toInt();
    Month = utc.substring(5, 7).toInt();
    Day = utc.substring(8, 10).toInt();
    Hour = utc.substring(11, 13).toInt();
    Minute = utc.substring(14, 16).toInt();
    Second = utc.substring(17, 19).toInt();

    
    SerialMon.print("Year: ");
    SerialMon.println(Year);
    SerialMon.print("Month: ");
    SerialMon.println(Month);
    SerialMon.print("Day: ");
    SerialMon.println(Day);
    SerialMon.print("Hour: ");
    SerialMon.println(Hour);
    SerialMon.print("Minute: ");
    SerialMon.println(Minute);
    SerialMon.print("Second: ");
    SerialMon.println(Second);

  }
    
}



void logData(String text, String component, int level){
  SerialMon.println("Entered logData");
  generateTimeString();
  if (level == 0){
       appendFile(SD, "/logger.txt", timeString);
    appendFile(SD, "/logger.txt", " [INFO] [");
    appendFile(SD, "/logger.txt", component.c_str());
            appendFile(SD, "/logger.txt", "] - ");
    appendFile(SD, "/logger.txt", text.c_str());
            appendFile(SD, "/logger.txt", "\r\n"); 
  }

    if (level == 1){

       appendFile(SD, "/logger.txt", timeString);
    appendFile(SD, "/logger.txt", " [ERROR] [");
    appendFile(SD, "/logger.txt", component.c_str());
            appendFile(SD, "/logger.txt", "] - ");
    appendFile(SD, "/logger.txt", text.c_str());
            appendFile(SD, "/logger.txt", "\r\n"); 
  }

    if (level == 2){
       appendFile(SD, "/logger.txt", timeString);
    appendFile(SD, "/logger.txt", " [WARN] [");
    appendFile(SD, "/logger.txt", component.c_str());
            appendFile(SD, "/logger.txt", "] - ");
    appendFile(SD, "/logger.txt", text.c_str());
            appendFile(SD, "/logger.txt", "\r\n"); 
  }

}

void initiateRTC(){

  getUTCTime();
  SerialMon.println("gotUTC");
  SerialMon.println(gotUTC);
  if(gotUTC == true){
    DateTime adjustedTime(Year, Month, Day, Hour, Minute, Second);
    rtc.adjust(adjustedTime);
    now = rtc.now();
    logData("Connected to the time server", "TIMESERVER", 0);
      logData("Got UTC timestamp from Time server", "RTC", 0);
      SerialMon.println("Got UTC timestamp from Time Server");
  }

  if(gotUTC == false && initiatedRTC == false){

  
  rtc.adjust(DateTime(__DATE__, __TIME__));
  initiatedRTC = true;

  now = rtc.now();
  now = now + TimeSpan(offsetDays, offsetHours, offsetMinutes, offsetSeconds); // Apply the offset correction
  
  logData("Couldn't connect to the timeserver", "TIMESERVER", 1);
  logData("Got UTC timestamp from IST", "RTC", 0);
  SerialMon.println("Got UTC timestamp from IST");
  }

    
}

void generateTimeString(){
    
  //  int Year = 0;
  //  Year = now.year();
     now = rtc.now();
     if (gotUTC == false){
     now = now + TimeSpan(offsetDays, offsetHours, offsetMinutes, offsetSeconds); // Apply the offset correction
     }
     Year = now.year();
     Month = now.month();
     Day = now.day();
     Hour = now.hour();
     Minute = now.minute();
     Second = now.second();
  // Serial.println("Year: ");
  // Serial.println(Year);
  // Serial.print("Month: ");
  // Serial.println(Month);
  // Serial.print("Day: ");
  // Serial.println(Day);
  // Serial.print("Hour: ");
  // Serial.println(Hour);
  // Serial.print("Minute: ");
  // Serial.println(Minute);
  // Serial.print("Second: ");
  // Serial.println(Second);

    SerialMon.println("Generating time stamp");

    sprintf(timeString, "%04d-%02d-%02dT%02d:%02d:%02d",Year, Month, Day, Hour, Minute, Second);
    SerialMon.println(timeString);

}



void sendData(){


  String authtext = "Basic ";
  String auth = "admin1:1qazxsdC";
  String authenc = base64::encode(auth);
  authtext += authenc;

  SerialMon.println("Performing client POST request...");
      logData("Performing client POST request...", "SERVER", 0);

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
  logData("Successfull at sending data", "SERVER", 0);
  postData = "";
  SerialMon.println("Data sent successfully");
  }

  if (status != 200){  
    logData(String(status), "SERVER", 1);
  postData = "";
  SerialMon.println("Data could not be sent successfully");
  http.stop();
 SerialMon.println(F("Server disconnected"));
  logData("Server disconnected", "SERVER", 0);
 modem.gprsDisconnect();
 SerialMon.println(F("GPRS disconnected"));
   logData("GPRS Disconnected", "SERVER", 0);
    networkConnection(apn, gprsUser, gprsPass, server, port);
  
  }


}

void ledStatus(int sdStatus){

  if(sdStatus == 1){
    digitalWrite(2, HIGH);
    
  }
  
  else{
    digitalWrite(2, LOW);
    SerialMon.println("SD Card not accessible");
    initiateSD();
  }
}

void setup() {


 pinMode(2, OUTPUT);
  SerialMon.begin(115200);
    initiateSD();
    ledStatus(sdSuccess);
    if (! rtc.begin()) {
      Serial.println("Couldn't find RTC in default location");
    }
      listDir(SD, "/", 0);
      appendFile(SD, "/hello.txt", "World!\n");
       SerialAT.begin(115200,SERIAL_8N1, 16, 17, false);
  SerialMon.println("Initializing modem...");
 
  appendFile(SD, "/logger.txt", "[INFO] [SERVER] - Modem initialized\r\n");

      networkConnection(apn, gprsUser, gprsPass, server, port);
  initiateRTC();
//    initiateRTC();
 
  // appendFile(SD, "/logger.txt", "INFO: SD Card Mounted successfully\r\n");
   logData("SD Card mounted successfully", "SD CARD", 0);
  

  int bmeResponse = initiateRHTPsensor();
  int rainResponse = initiateRainsensor();
  int windResponse = initiateWindsensor();

  if (bmeResponse == 200 && rainResponse == 200 && windResponse == 200){
   logData("All sensors are connected and integrated", "SENSORS", 0);
  }

  
  if (bmeResponse != 200 && rainResponse == 200 && windResponse == 200){
   logData("All sensors except BME280 are connected and integrated", "SENSORS", 2);
  }

  
  if (bmeResponse == 200 && rainResponse != 200 && windResponse == 200){
   logData("All sensors except Rain sensor are connected and integrated", "SENSORS", 2);
  }

  
  if (bmeResponse == 200 && rainResponse == 200 && windResponse != 200){
   logData("All sensors except Wind sensor are connected and integrated", "SENSORS", 2);
  }


ledStatus(sdSuccess);
 

  delay(1000);

}



void loop() {
  ledStatus(sdSuccess);
  if (iter%100 == 0)
  {
    initiateRTC();
  }
  collectSensorData();
  sendData();
}
