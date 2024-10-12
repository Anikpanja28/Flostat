
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Ping.h>

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
//const char* host = "192.168.1.7";  // Replace with your server IP
//const uint16_t port = 5544; 
int count = 0; 
const char* host = "172.17.18.65";  // Replace with your server IP
const uint16_t port = 5555;  

const char* remote_host = "www.google.com";

// Variable to hold the current water level state
int cur_state = -1; // Initialize to an impossible state for initial comparison
int stable_state = -1; // State that is stable
unsigned long lastChangeTime = 0; // Time when last change was detected
const unsigned long stableInterval = 45000; // Interval for stability in milliseconds (30 seconds)
int wifi_retries = 30;
//String serverName = "http://192.168.212.40:5544"; // API Gateway deployment
String tankid = "8";

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
  pinMode(SERVER_LED_PIN, OUTPUT);

  digitalWrite(SERVER_LED_PIN, HIGH);
  digitalWrite(WIFI_LED_PIN, HIGH);
  digitalWrite(POWER_LED_PIN, HIGH);
  digitalWrite(SENSOR1_LED_PIN, HIGH);
  digitalWrite(SENSOR2_LED_PIN, HIGH);
  digitalWrite(SENSOR3_LED_PIN, HIGH);
  digitalWrite(SENSOR4_LED_PIN, HIGH);
  digitalWrite(SERVER_LED_PIN, HIGH);

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
    digitalWrite(WIFI_LED_PIN, HIGH);
  Serial.println("Connected to WiFi");

  pinMode(SENSOR1_PIN, INPUT_PULLUP);
  pinMode(SENSOR2_PIN, INPUT_PULLUP);
  pinMode(SENSOR3_PIN, INPUT_PULLUP);
  pinMode(SENSOR4_PIN, INPUT_PULLUP);
  

  
  digitalWrite(SENSOR1_LED_PIN, LOW);
  digitalWrite(SENSOR2_LED_PIN, LOW);
  digitalWrite(SENSOR3_LED_PIN, LOW);
  digitalWrite(SENSOR4_LED_PIN, LOW);
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
  if(count==60){
    count = 0;
  sendWaterLevelUpdate(Level);
  }
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

void sendWaterLevelUpdate(String waterLevel) {
  if(WiFi.status() == WL_CONNECTED) {

    
    if (!client.connect(host, port)) {
    Serial.println("Connection to server failed");
      digitalWrite(SERVER_LED_PIN, LOW);
      delay(2000);
      ESP.restart();
    }   

  if (client.connect(host, port)) {

      digitalWrite(SERVER_LED_PIN, HIGH);
     Serial.println("Connected to server");

    // Send data to the server
     String message = tankid + "|" + waterLevel;
 
    client.println(message);
    Serial.println("Data sent to server: " + message);
    }
  }

  
  else{
    digitalWrite(SERVER_LED_PIN, LOW);
      digitalWrite(WIFI_LED_PIN, LOW);
      digitalWrite(SERVER_LED_PIN, LOW);
      delay(2000);
      ESP.restart();
    }
  }
   
