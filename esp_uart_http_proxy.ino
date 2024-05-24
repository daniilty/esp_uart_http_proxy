#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>

#define TERMINATOR "e\r\n"

AsyncWebServer server(4498);
SemaphoreHandle_t mux = NULL; 

static void onRequest(AsyncWebServerRequest *request){
  request->send(404);
}

static void printSerial(String data) {
  while (Serial.availableForWrite()<data.length())
    delay(10);
  Serial.print(data);
}

static void printSerial() {
}

static void printSerial(unsigned int c) {
  while (!Serial.availableForWrite())
    delay(10);
  Serial.write(c);
}

static void printSerial(uint8_t c) {
  while (!Serial.availableForWrite())
    delay(10);
  Serial.write(c);
}

static int writeSerialData(uint8_t *data, size_t len) {
  uint8_t hash = 0;
  int n = 0;
  for(size_t i=0; i<len; i++,n++){
    if (n == 0) {
      printSerial("PRT");
      Serial.write(len);
    }

    printSerial(data[i]);
    hash ^= data[i];
    if (n == 127) {
      printSerial(hash);
      hash = 0;
      n = 0;
      if (!readSerialBlock().equals("ACK")) {
        i = 0;
      }
    }
  }

  if (n != 0) {
    printSerial(hash);
    if (!readSerialBlock().equals("ACK")) {
      return 0;
    }
  }

  return 1;
} 

static void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (xSemaphoreTake(mux, portMAX_DELAY)) {
    if(!index){
      printSerial("BGN");
      printSerial(filename.length());
      printSerial(filename);
    }
    
    int tries = 0;
    while(!writeSerialData(data, len)) {
      if (tries > 5) {
        xSemaphoreGive(mux);
        request->send(500, "application/json", "{\"error\":\"failed to write serial\"}");
        return;
      }
      delay(10);
      tries++;
    }
  
    if(final){
      printSerial("FIN");
    }
    
    xSemaphoreGive(mux);
  }
}

static String readSerialBlock() {
  String dat = readSerial();
  while (dat.isEmpty()) {
    dat = readSerial();
    delay(10);
  }

  return dat;
}

static String readSerial() {
  String dat = Serial.readStringUntil('\n');

  return dat;
}

static int connWifi() {
  String cmd = readSerialBlock();

  if(cmd.equals("INF")) {
    printSerial("NIT");
    return 0;
  }

  if(!cmd.equals("WSP")) {
    return 0;
  }
  if (strcmp(cmd.c_str(), "WSP") != 0) {
    return 0;
  }

  String ssidPassword = readSerialBlock();
  int pos = ssidPassword.indexOf(":");
  if (pos == -1) {
    return 0;
  }

  String ssid = ssidPassword.substring(0, pos);
  String password = ssidPassword.substring(pos+1, ssidPassword.length());
  WiFi.begin(ssid, password); 

  while (1) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        printSerial("ENS");
        return 0;
      case WL_CONNECT_FAILED:
        printSerial("ECF");
        return 0;
      case WL_CONNECTION_LOST:
        printSerial("ECL");
        return 0;
      case WL_NO_SHIELD:
        printSerial("ESH");
        return 0;
      case WL_CONNECTED:
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
  printSerial("WIF");
  const char *inf = getServerAddr().c_str();
  size_t l = strlen(inf);
  while (Serial.availableForWrite()<l+1)
    delay(10);
  Serial.write(l);
  Serial.write(inf);
}

void setup() {
  esp_task_wdt_init(50, false);
  //Serial.begin(76800);
  Serial.begin(115200);
  mux = xSemaphoreCreateMutex();
  
  while (!connWifi()) {}

  xTaskCreate(
    loopXTask,     // Function to implement the task
    "loopXTask",   // Name of the task
    2048,      // Stack size in bytes
    NULL,      // Task input parameter
    tskIDLE_PRIORITY, // Priority of the task
    NULL       // Task handle.
  );
  
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

static void loopXTask (void* pvParameters) {
  while (1) {
    if (xSemaphoreTake(mux, portMAX_DELAY)) {
      String cmd = readSerial();
      if (!cmd.isEmpty()) {
        handleTask(cmd);
      }
      
      xSemaphoreGive(mux);
      delay(2000);
    }
  }
}

static void handleTask(String task) {
  if (task.equals("INF") || task.equals("WSP")) {
    printWifiInfo();
  } else if (task.equals("PNG")) {
    printSerial("PNG");
  }
}

void loop() {
  // unused
}