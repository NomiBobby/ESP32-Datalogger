#include "utils.h"

const int CS = 5; // SD Card chip select
HardwareSerial VM(1); // UART port 1 on ESP32

void spiffs_init(){
  // Mount SPIFFS filesystem
  Serial.printf("Mounting SPIFFS filesystem - ");
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS allocation failed");
    return;
  }
  Serial.println("OK");
}

/******************************************************************
 *                                                                *
 *                             WiFi                               *
 *                                                                *
 ******************************************************************/

void wifi_setting_reset(){
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); //load the flash-saved configs
  esp_wifi_init(&cfg); //initiate and allocate wifi resources (does not matter if connection fails)
  delay(2000); //wait a bit
  if(esp_wifi_restore()!=ESP_OK){
    Serial.println("WiFi is not initialized by esp_wifi_init ");
  }
  else{
    Serial.println("Clear WiFi Configurations - OK");
  }
}

// Function to connect to WiFi
void wifi_init(){
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println(WIFI_SSID);
    Serial.println(WIFI_PASSWORD);

    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      i++;
        delay(1000);
        Serial.print(".");
        if(i > 10){
          break;
        }
    }

    // Print local IP address
    Serial.println();
    Serial.print("Connected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());
}

String get_public_ip() {

  HTTPClient http;
  http.begin("https://api.ipify.org");  // Make a GET request to api.ipify.org to get the public IP
  int httpCode = http.GET();  // Send the request

  if (httpCode == HTTP_CODE_OK) {  // Check for a successful response
    String publicIP = http.getString();
    return publicIP;
  } else {
    Serial.printf("HTTP request failed with error code %d\n", httpCode);
    return "Error";
  }

  http.end();  // End the request
}



/******************************************************************
 *                                                                *
 *                             Time                               *
 *                                                                *
 ******************************************************************/

/* Time */
const char *ntpServers[] = {
  "pool.ntp.org",
  "time.google.com",
  "time.windows.com",
  "time.nist.gov",  // Add more NTP servers as needed
};

const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);
int daylightOffset_sec = 3600;
RTC_DS1307 rtc;

DateTime tmToDateTime(struct tm timeinfo) {
  return DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void external_rtc_init(){

  if (! rtc.begin()) {
    Serial.println("RTC module is NOT found");
    Serial.flush();
    return;
  }

  DateTime now = rtc.now();
  Serial.print("RTC time: ");
  Serial.println(get_current_time());
}

void external_rtc_sync_ntp(){
  // Synchronize DS1307 RTC with NTP time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    DateTime ntpTime = tmToDateTime(timeinfo);
    rtc.adjust(ntpTime);
    Serial.println("\nDS1307 RTC synchronized with NTP time.");
    DateTime now = rtc.now();
    Serial.print("RTC time: ");
    Serial.println(get_current_time());    return; // Exit the function if synchronization is successful
  } else {
    Serial.printf("Failed to get NTP Time for DS1307\n");
    return;
  }
}

void ntp_sync() {

  long gmtOffset_sec = utcOffset * 60 * 60;

  // Attempt synchronization with each NTP server in the list
  for (int i = 0; i < numNtpServers; i++) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServers[i]);
    struct tm timeinfo;
    // Serial.printf("Syncing with NTP server %s...\n", ntpServers[i]);
    if (getLocalTime(&timeinfo)) {
      // Serial.printf("Time synchronized successfully with NTP server %s\n", ntpServers[i]);
      // Serial.printf("NTP Time: %s\n", get_current_time().c_str());
      external_rtc_sync_ntp();
      return; // Exit the function if synchronization is successful
    } else {
      Serial.printf("Failed to synchronize with NTP server %s\n", ntpServers[i]);
    }
  }
  // If synchronization fails with all servers
  Serial.println("Failed to synchronize with any NTP server.");
}

String get_current_time(bool getFilename) {
  struct tm timeinfo;
  DateTime now = rtc.now();
  
    char buffer[20]; // Adjust the buffer size based on your format
    if(!getFilename){
      snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %02d:%02d:%02d", 
              now.year(), now.month(), now.day(), now.hour(), now.minute(),now.second());
    }
    else{
      // Round minutes to the nearest 15-minute interval
      int roundedMinutes = (now.minute() / 15) * 15;
      snprintf(buffer, sizeof(buffer), "%04d_%02d_%02d_%02d_%02d", 
              now.year(), now.month(), now.day(), now.hour(), roundedMinutes);
    }
    return buffer;
  return ""; // Return an empty string in case of failure
}

/******************************************************************
 *                                                                *
 *                            SD Card                             *
 *                                                                *
 ******************************************************************/
SPIClass *sdSpi = NULL; // SPI object for SD card

void sd_init(){
  Serial.printf("Initializing SD card - ");

  // Initialize SD card SPI bus
  sdSpi = new SPIClass(VSPI);
  sdSpi->begin(18, 19, 23, 5); // VSPI pins for SD card: miso, mosi, sck, ss

  // SPI.begin(18, 19, 23, 5); //SCK, MISO, MOSI,SS
  // if (!SD.begin(CS, SPI)) {
  if (!SD.begin(CS, *sdSpi)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("OK");
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
  // Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      // Serial.println("Message appended");
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

/******************************************************************
 *                                                                *
 *                            OLED                                *
 *                                                                *
 ******************************************************************/

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define CHAR_HEIGHT 8  // Each character row is 8 pixels high
#define OLED_ADDR 0x3C // Define the I2C address (0x3C for most Adafruit OLEDs)
#define NUM_ROWS (SCREEN_HEIGHT / CHAR_HEIGHT) // Number of rows that fit on the screen
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);// Create the display object
char screenBuffer[NUM_ROWS][21];  // 20 characters + null terminator

void oled_init() {

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Initialize the screen buffer with empty strings
  for (int i = 0; i < NUM_ROWS; i++) {
    screenBuffer[i][0] = '\0';
  }

  display.setCursor(0, 0);  // Set cursor to top-left corner
  display.println(F("booted"));
  display.display();

}

void oled_print(const char* text) {
  static int readingIndex = 0; // Keep track of the current reading index

  // Scroll up if the screen is full
  if (readingIndex >= NUM_ROWS) {
    // Shift all rows up by copying the content from the next row
    for (int i = 0; i < NUM_ROWS - 1; i++) {
      strcpy(screenBuffer[i], screenBuffer[i + 1]);
    }
    readingIndex = NUM_ROWS - 1;
  }

  // Add the new reading to the buffer
  strncpy(screenBuffer[readingIndex], text, sizeof(screenBuffer[readingIndex]));
  screenBuffer[readingIndex][sizeof(screenBuffer[readingIndex]) - 1] = '\0';  // Ensure null termination

  // Clear the display
  display.clearDisplay();

  // Redraw all the buffer content
  for (int i = 0; i <= readingIndex; i++) {
    display.setCursor(0, i * CHAR_HEIGHT);
    display.print(screenBuffer[i]);
  }
  display.display();

  // Update the reading index
  readingIndex++;
}

// Overloaded function for uint8_t data type
void oled_print(uint8_t value) {
  char buffer[8];
  snprintf(buffer, 8, "%u", value);
  oled_print(buffer);
}

// Function to generate a random number using ESP32's hardware RNG
uint32_t generateRandomNumber() {
  // Seed the random number generator with a value from the hardware RNG
  uint32_t seed = esp_random();
  randomSeed(seed);
  
  // Generate a random number between 0 and 4294967294
  uint32_t randomNumber = random(0, 4294967295);
  
  return randomNumber;
}