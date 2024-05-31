#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>

#define B_PIN 4
#define G_PIN 5
#define R_PIN 6

typedef enum {
  START,
  TRANSFER_BEGIN,
  TRANSFER_END,
  FILE_PART,
  ACK,
  PONG,
  WIFI_INFO,
  WIFI_SETUP,
  E_WL_NO_SSID_AVAIL,
  E_WL_CONNECT_FAILED,
  E_WL_CONNECTION_LOST,
  E_WL_NO_SHIELD,
} CMD;

AsyncWebServer server(4498);
// SemaphoreHandle_t mux = NULL; 

static void setWhite() {
  digitalWrite(B_PIN, LOW);
  digitalWrite(G_PIN, LOW);
  digitalWrite(R_PIN, LOW);
}

static void setPurple() {
  digitalWrite(B_PIN, LOW);
  digitalWrite(G_PIN, HIGH);
  digitalWrite(R_PIN, LOW);
}

static void setRed() {
  digitalWrite(B_PIN, HIGH);
  digitalWrite(G_PIN, HIGH);
  digitalWrite(R_PIN, LOW);
}

static void setBlue() {
  digitalWrite(B_PIN, LOW);
  digitalWrite(G_PIN, HIGH);
  digitalWrite(R_PIN, HIGH);
}

static void setGreen() {
  digitalWrite(B_PIN, HIGH);
  digitalWrite(G_PIN, LOW);
  digitalWrite(R_PIN, HIGH);
}

static void onRequest(AsyncWebServerRequest *request){
  request->send(404);
}

static void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  // if (xSemaphoreTake(mux, portMAX_DELAY)) {
  if(!index){
    Serial.write(TRANSFER_BEGIN);
    filename.trim();
    Serial.print(filename);
    Serial.write('\0');
    setBlue();
  } else {
    setPurple();
  }

  Serial.write(data, len);

  if(final){
    setGreen();
    Serial.write('\0');
  }
    
  //   xSemaphoreGive(mux);
  // }
}

static String readSerialBlock() {
  String dat = readSerial();
  while (dat.isEmpty()) {
    dat = readSerial();
    delay(10);
  }

  return dat;
}

uint8_t readSerialByte() {
  while (!Serial.available())
    delay(10);

  return Serial.read();
}

static String readSerial() {
  String dat = Serial.readStringUntil('\n');

  return dat;
}

static int connWifi() {
  String ssidPassword = readSerialBlock();
  int pos = ssidPassword.indexOf(":");
  if (pos == -1) {
    return 0;
  }

  String ssid = ssidPassword.substring(0, pos);
  String password = ssidPassword.substring(pos+1, ssidPassword.length());
  WiFi.begin(ssid, password); 
  setBlue();
  while (1) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.write(E_WL_NO_SSID_AVAIL);
        return 0;
      case WL_CONNECT_FAILED:
        Serial.write(E_WL_CONNECT_FAILED);
        return 0;
      case WL_CONNECTION_LOST:
        Serial.write(E_WL_CONNECTION_LOST);
        return 0;
      case WL_NO_SHIELD:
        Serial.write(E_WL_NO_SHIELD);
        return 0;
      case WL_CONNECTED:
        setGreen();
        return 1;
      default:
        delay(100);
        break;
    }
  }

  return 0;
}

static String getServerAddr() {
  return WiFi.localIP().toString() + ":4498";
}

static void printWifiInfo() {
  Serial.write(WIFI_INFO);
  Serial.print(getServerAddr());
  Serial.write('\0');
}

void setup() {
  esp_task_wdt_init(600, false);
  Serial.begin(76800);
  //Serial.begin(115200);
  Serial.setTxBufferSize(0);
  // mux = xSemaphoreCreateMutex();

  pinMode(B_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(R_PIN, OUTPUT);
  setWhite();
  
  while (!connWifi()) {
    setRed();
  }

  // xTaskCreate(
  //   loopXTask,     // Function to implement the task
  //   "loopXTask",   // Name of the task
  //   2048,      // Stack size in bytes
  //   NULL,      // Task input parameter
  //   tskIDLE_PRIORITY, // Priority of the task
  //   NULL       // Task handle.
  // );
  
  // Print the ESP32's IP address
  printWifiInfo();

  // Define a route to serve the HTML page
  server.on("/is_esp", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"board\":\"flipperdevboard\"}");
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{}");
  }, onUpload);

  server.on("/add_folder", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, onUpload);

  server.onFileUpload(onUpload);
  server.onNotFound(onRequest);

  // Start the server
  server.begin();
}

// static void loopXTask (void* pvParameters) {
//   while (1) {
//     if (xSemaphoreTake(mux, portMAX_DELAY)) {
//       handleTask(readSerialByte());
      
//       xSemaphoreGive(mux);
//       delay(1000);
//     }
//   }
// }

// static void handleTask(uint8_t task) {
//   switch (task) {
//     case WIFI_INFO:
//     case WIFI_SETUP:
//       printWifiInfo();
//       break;
//     default:
//       Serial.write(PONG);
//       break;
//   }
// }

void loop() {
  // unused
}