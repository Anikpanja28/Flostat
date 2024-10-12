
#include "FS.h"
#include "SD.h"
#include "SPI.h"


// Define the software serial pins
#define SerialMon Serial
HardwareSerial SerialAT(2);

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
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);
  String dataFin;
  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
   
    
  }
  file.close();
 
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
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
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}


void SDlogger(float temperature, float pressure, float humidity, String wind, String timeStamp, char* acc, char* eventAcc, char* totalAcc, char* rInt, bool rainFlag) {
  
String  dataMessage = "Time " + String(timeStamp) + "," + "Temperature " + String(temperature) + "," +"Pressure " + String(pressure) + "," + 
                "Humidity " + String(humidity) + "," +  "Accumulation " + String(acc) + "," +  "Event Acc " + String(eventAcc) + "," + 
                "Total Acc " + String(totalAcc) + "," + wind + "rainFlag " + String(rainFlag) + "\r\n";

  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/logger.txt", dataMessage.c_str());

}

void setup() {
  
  // Start the software serial communication
  SerialAT.begin(115200,SERIAL_8N1, 16, 17, false);
  SerialMon.begin(115200);
  delay(2000);
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
     Serial.println("Restarting device");
    delay(5000);
     
  ESP.restart();
  }
  uint8_t cardType = SD.cardType();
  uint32_t fileSize;
  String chunk;
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    Serial.println("Restarting device");
    delay(5000);
  ESP.restart();
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  listDir(SD, "/", 0);
  for(int i=0; i<5; i++){
    if(i!=4){
  appendFile(SD, "/ftp2.txt", "Appending test\r\n");
    }
    else{
      appendFile(SD, "/ftp2.txt", "Appending test\r\t");
    }
  delay(1000);
  }
  File dataFile = SD.open("/ftp2.txt");
  if (dataFile) {
    // Get the file size
    fileSize = dataFile.size();

    Serial.print("File Size: ");
    Serial.print(fileSize);
    Serial.println(" bytes");
  }

    while (dataFile.available()) {
    chunk = dataFile.readStringUntil('\t');
    }
    SerialMon.println(chunk);
  SerialMon.println("Started");
  // Initialize the SIM7600 module
  delay(3000);
    sendATCommand("AT"); // Send AT to check if the module is responsive
  delay(3000);
  sendATCommand("AT+CSQ"); // Send AT to check if the module is responsive
  delay(3000);
    sendATCommand("AT+CREG?"); // Send AT to check if the module is responsive
  delay(3000);
    sendATCommand("AT+CGREG?"); // Send AT to check if the module is responsive
  delay(3000);
  sendATCommand("AT+CPSI?"); // Send AT to check if the module is responsive
  delay(3000);
  // Set up the APN and other necessary settings
  sendATCommand("AT+CGATT=1");  // Attach to GPRS
   delay(3000);
  sendATCommand("AT+CGDCONT=1,\"IP\",\"airtelgprs.com\""); // Set your APN
  delay(3000);
  // Open a GPRS context
  sendATCommand("AT+CGACT=1,1");
  delay(3000);
  sendATCommand("AT+HTTPINIT");
  delay(3000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"http://34.201.127.182/data\"");

  delay(3000);
   sendATCommand("AT+HTTPPARA=\"CID\",1");
  delay(1000);
        sendATCommand("AT+HTTPDATA=18,1000");
  delay(3000);
//  SerialAT.println("MESSAGE=HELLOWORLD");
          sendATCommand("MESSAGE=HELLOWORLD");
  delay(3000);
    sendATCommand("AT+HTTPACTION=1");
  delay(3000);
      sendATCommand("AT+HTTPHEAD");
  delay(3000);
        sendATCommand("AT+HTTPTERM");
  delay(3000);
  
  

}

void loop() {
  // Your main loop code here
}

void sendATCommand(const char* command) {
  SerialAT.println(command);
  delay(100);
  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }
}
