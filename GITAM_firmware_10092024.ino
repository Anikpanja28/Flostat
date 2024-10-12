// Last Firmware updated - 19/04/2024 14:31:00

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Ping.h>
#include <ArduinoJson.h>
#include <ESP32httpUpdate.h>

// Sensor pins
#define SENSOR1_PIN 21
#define SENSOR2_PIN 19
#define SENSOR3_PIN 18
#define SENSOR4_PIN 4

// LED pins
#define SENSOR1_LED_PIN 27
#define SENSOR2_LED_PIN 26
#define SENSOR3_LED_PIN 25
#define SENSOR4_LED_PIN 33
#define SERVER_LED_PIN 14
#define WIFI_LED_PIN 13
#define POWER_LED_PIN 32

const char* ssid = "GITAM";
const char* password = "Gitam$$123";
const char* remote_host = "www.google.com";

const char* Firmware = "";
String versionNumber = "1.2";
String prevVersion = "1.1";
String orgName = "GITAM";
String firmwarePath = "https://raw.githubusercontent.com/Anikpanja28/ritu_firmware/main/";
unsigned long lastTime = 0;

const char* host = "172.17.18.65";  // Replace with your server IP
const uint16_t port = 5555;  

// Timer set to 10 minutes (600000)
// unsigned long timerDelay = 600000;
// Set timer to 30 seconds (30000)
unsigned long timerDelay = 30000;
const unsigned long hourlyInterval = 3600000; // 1 hour in milliseconds
unsigned long lastHourlyUpdate = 0; // Variable to track the last time an hourly update was sent

unsigned long Uptime;

// Variable to hold the current water level state
int cur_state = -1; // Initialize to an impossible state for initial comparison
int stable_state = -1; // State that is stable
unsigned long lastChangeTime = 0; // Time when last change was detected
const unsigned long stableInterval = 5000; // Interval for stability in milliseconds
int wifi_retries = 30;
String serverName = "https://lgcdq22pe3.execute-api.ap-south-1.amazonaws.com/First/?org_id=1&service=update&machine=watertank&mode=auto"; // API Gateway deployment
String tankid = "1";
int count = 0;

WiFiClient client;

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  pinMode(SERVER_LED_PIN, OUTPUT);
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(POWER_LED_PIN, OUTPUT);
  pinMode(SENSOR1_LED_PIN, OUTPUT);
  pinMode(SENSOR2_LED_PIN, OUTPUT);
  pinMode(SENSOR3_LED_PIN, OUTPUT);
  pinMode(SENSOR4_LED_PIN, OUTPUT);

  digitalWrite(SERVER_LED_PIN, HIGH);
  digitalWrite(WIFI_LED_PIN, HIGH);
  digitalWrite(POWER_LED_PIN, HIGH);
  digitalWrite(SENSOR1_LED_PIN, HIGH);
  digitalWrite(SENSOR2_LED_PIN, HIGH);
  digitalWrite(SENSOR3_LED_PIN, HIGH);
  digitalWrite(SENSOR4_LED_PIN, HIGH);

  delay(3000);

  digitalWrite(SERVER_LED_PIN, LOW);
  digitalWrite(WIFI_LED_PIN, LOW);
  digitalWrite(POWER_LED_PIN, LOW);
  digitalWrite(SENSOR1_LED_PIN, LOW);
  digitalWrite(SENSOR2_LED_PIN, LOW);
  digitalWrite(SENSOR3_LED_PIN, LOW);
  digitalWrite(SENSOR4_LED_PIN, LOW);

  delay(1000);

  digitalWrite(POWER_LED_PIN, HIGH);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    wifi_retries--;
    if(wifi_retries == 0){
      Serial.println("Restarting....");
      ESP.restart();
    }
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  pinMode(SENSOR1_PIN, INPUT_PULLUP);
  pinMode(SENSOR2_PIN, INPUT_PULLUP);
  pinMode(SENSOR3_PIN, INPUT_PULLUP);
  pinMode(SENSOR4_PIN, INPUT_PULLUP);

  digitalWrite(SERVER_LED_PIN, LOW);
}

void loop() {
  int State1 = digitalRead(SENSOR1_PIN);
  int State2 = digitalRead(SENSOR2_PIN);
  int State3 = digitalRead(SENSOR3_PIN);
  int State4 = digitalRead(SENSOR4_PIN);
  int new_state = determineWaterLevel(State1, State2, State3, State4);

  Serial.println(new_state);
  count++;
  if (new_state == -1){
    String faultyLevel = String(new_state) + String(State1) + String(State2) + String(State3) + String(State4); 
    if(count == 60){
      count = 0;
      sendWaterLevelUpdate(faultyLevel);
    }
    updateLEDs(0);
  }
  else{
    updateLEDs(new_state);
    String Level = getWaterLevelString(new_state);
    if(count == 60){
      count = 0;
      Serial.println("Local");
      sendWaterLevelUpdate(Level);
    }
  }

  delay(1000); // Delay to avoid spamming (adjust as needed)

  unsigned long currentTime = millis();
  if (new_state != cur_state) {
    Serial.println(new_state);
    cur_state = new_state;
    lastChangeTime = currentTime; // Update time of change
    updateLEDs(cur_state);
  } else if (currentTime - lastChangeTime >= stableInterval && stable_state != cur_state) {
    stable_state = cur_state; // Update stable state
    String Level = getWaterLevelStringCloud(stable_state);
    sendWaterLevelUpdateCloud(Level);
  }

  // Send water level every hour, irrespective of changes
  if (currentTime - lastHourlyUpdate >= hourlyInterval) {
    Serial.println("Sending hourly water level update.");
    sendWaterLevelUpdateCloud(getWaterLevelStringCloud(cur_state)); // Send the current water level
    lastHourlyUpdate = currentTime; // Reset the lastHourlyUpdate time
  }

  delay(1000); // Delay to avoid spamming (adjust as needed)
}

int determineWaterLevel(int State1, int State2, int State3, int State4) {
  if (State1 == LOW && State2 == HIGH && State3 == HIGH && State4 == HIGH) return 1;
  if (State1 == LOW && State2 == LOW && State3 == HIGH && State4 == HIGH) return 2;
  if (State1 == LOW && State2 == LOW && State3 == LOW && State4 == HIGH) return 3;
  if (State1 == LOW && State2 == LOW && State3 == LOW && State4 == LOW) return 4;
  if (State1 == HIGH && State2 == HIGH && State3 == HIGH && State4 == HIGH) return 0;
  return -1; // Undefined state
}

void updateLEDs(int level) {
  digitalWrite(SENSOR1_LED_PIN, LOW);
  digitalWrite(SENSOR2_LED_PIN, LOW);
  digitalWrite(SENSOR3_LED_PIN, LOW);
  digitalWrite(SENSOR4_LED_PIN, LOW);

  if (level >= 1) digitalWrite(SENSOR1_LED_PIN, HIGH);
  if (level >= 2) digitalWrite(SENSOR2_LED_PIN, HIGH);
  if (level >= 3) digitalWrite(SENSOR3_LED_PIN, HIGH);
  if (level == 4) digitalWrite(SENSOR4_LED_PIN, HIGH);
}

String getWaterLevelString(int level) {
  switch(level) {
    case 0: return "0";
    case 1: return "1";
    case 2: return "2";
    case 3: return "3";
    case 4: return "4";
    default: return "-1";
  }
}

String getWaterLevelStringCloud(int level) {
  switch(level) {
    case 0: return "0";
    case 1: return "25";
    case 2: return "50";
    case 3: return "75";
    case 4: return "100";
    default: return "-1";
  }
}

void sendWaterLevelUpdate(String waterLevel) {
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("ENtered");
    if (client.connect(host, port)) {
      digitalWrite(SERVER_LED_PIN, HIGH);
      Serial.println("Connected to server");

      // Send data to the server
      String message = tankid + "|" + waterLevel;
      client.println(message);
      Serial.println("Data sent to server: " + message);
    } else {
      Serial.println("Connection to server failed");
      digitalWrite(SERVER_LED_PIN, LOW);
      // Removed ESP.restart(); continue execution
    }
  } else {
    digitalWrite(SERVER_LED_PIN, LOW);
    digitalWrite(WIFI_LED_PIN, LOW);
    digitalWrite(SERVER_LED_PIN, LOW);
    // Removed ESP.restart(); continue execution
  }
}

void sendWaterLevelUpdateCloud(String waterLevel) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String serverPath = serverName + "&id=" + tankid + "&state=" + waterLevel;
    http.begin(serverPath); // Specify the URL
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("Server response: " + response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
      if(!Ping.ping(remote_host)) {
        Serial.println("No internet, restarting");
        delay(2000);
        ESP.restart();
      }
    }

    http.end(); // Close connection
  } else {
    if(!Ping.ping(remote_host)) {
      Serial.println("No internet, restarting");
      delay(2000);
      ESP.restart();
    }
  }
}
