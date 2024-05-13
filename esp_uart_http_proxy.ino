#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define TERMINATOR "\n\r"

AsyncWebServer server(4498);
SemaphoreHandle_t mux = NULL; 

static void onRequest(AsyncWebServerRequest *request){
  request->send(404);
}

static int printSerial(String data) {
  Serial.print(data);
  Serial.print(TERMINATOR);
}

static int printSerial() {
    Serial.print(TERMINATOR);
}

static int printSerial(uint8_t c) {
  Serial.write(c);
  Serial.print(TERMINATOR);
}

static int writeSerialData(uint8_t *data, size_t len) {
  int tries = 0;

  while(!Serial.availableForWrite()) {
    if (tries > 15) {
      return 0;
    }

    delay(100);
    tries++;
  } 

  uint8_t hash = 0;
  int n = 0;
  for(size_t i=0; i<len; i++,n++){
    if (n == 0) {
      printSerial("PRT");
    }

    Serial.write(data[i]);
    hash ^= data[i];
    if (n == 125) {
      printSerial();
      printSerial(hash);
      hash = 0;
      n = 0;
    }
  }

  printSerial();
  printSerial(hash);
  if (!readSerialBlock().equals("ACK")) {
    return 0;
  }

  return 1;
} 

static void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (xSemaphoreTake(mux, portMAX_DELAY)) {
    if(!index){
      printSerial("BGN");
      printSerial(filename);
    }
    
    int tries = 0;
    while(!writeSerialData(data, len)) {
      if (tries > 5) {
        xSemaphoreGive(mux);
        request->send(500, "application/json", "{\"error\":\"failed to write serial\"}");
        return;
      }
      delay(100);
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
    delay(500);
  }

  return dat;
}

static String readSerial() {
  String dat = Serial.readStringUntil('\n');
  Serial.readStringUntil('\r');

  return dat;
}

static int connWifi() {
  String cmd = readSerialBlock();
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
        Serial.print("ENS\r\n");
        return 0;
      case WL_CONNECT_FAILED:
        Serial.print("ECF\r\n");
        return 0;
      case WL_CONNECTION_LOST:
        Serial.print("ECL\r\n");
        return 0;
      case WL_NO_SHIELD:
        Serial.print("ENS\r\n");
        return 0;
      case WL_CONNECTED:
        return 1;
      default:
        delay(1000);
        break;
    }
  }

  return 1;
}

static String getServerAddr() {
  return WiFi.localIP().toString() + ":4498";
}

void setup() {
  Serial.begin(115200);
  mux = xSemaphoreCreateMutex();
  
  while (!connWifi()) {}

  xTaskCreate(
    loopXTask,     // Function to implement the task
    "loopXTask",   // Name of the task
    2048,      // Stack size in bytes
    NULL,      // Task input parameter
    1,         // Priority of the task
    NULL       // Task handle.
  );
  
  // Print the ESP32's IP address
  printSerial("WIF");
  printSerial(getServerAddr());

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
      delay(200);
    }
  }
}

static void handleTask(String task) {
  if (task.equals("INF")) {
    printSerial("WIF");
    printSerial(getServerAddr());
  } else if (task.equals("PNG")) {
    printSerial("PNG");
  } else {
    printSerial("ERR");
  }
}

void loop() {
  // unused
}