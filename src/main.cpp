#include <FS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>
#include "Secrets.h"
#include "datalogging.h"
#include "utils.h"
#include "api_interface.h"
#include "VM_501.h"

/* Do not modify below */
SemaphoreHandle_t logMutex;
TaskHandle_t parsingTask; // Task handle for the parsing task

/* Tasks */
void initNTPTask(void *parameter) {
  initNTP();  // Call the initNTP function
  vTaskDelete(NULL);  // Delete the task once initialization is complete
}

void setup() {
  /* Essentials for Remote Access */
  Serial.println("-------------------------------------\nBooting...");
  Serial.begin(115200);
  setupSPIFFS();// Setup SPIFFS -- Flash File System
  SD_initialize();//SD card file system initialization

  connectToWiFi();// Set up WiFi connection
  WiFi.onEvent(WiFiEvent);// Register the WiFi event handler
  startServer();// start Async server with api-interfaces

  /* Logging Capabilities */
  logMutex = xSemaphoreCreateMutex();  // Mutex for current logging file
  void initVM501();
  initDS1307();// Initialize external RTC, MUST BE INITIALIZED BEFORE NTP
  initializeOLED();

  xTaskCreatePinnedToCore(sendCommandVM501, "ParsingTask", 4096, NULL, 1, &parsingTask, 1);
  xTaskCreate(initNTPTask, "InitNTPTask", 4096, NULL, 1, NULL);
  Serial.println("-------------------------------------");
  Serial.println("Data Acquisition Started...");
}

void loop() {


  LogErrorCode result = logData();

  ElegantOTA.loop();

}