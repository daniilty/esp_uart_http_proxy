#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>

#define TERMINATOR "e\r\n"
#define B_PIN 4
#define G_PIN 5
#define R_PIN 6

AsyncWebServer server(4498);
SemaphoreHandle_t mux = NULL; 

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
  const size_t max_part_size = 127;
  uint8_t hash = 0;
  int n = 0;
  for(size_t i=0; i<len; i++){
    if (n == 0) {
      printSerial("PRT");
      size_t part_len = len-i;
      if (part_len >= max_part_size) part_len = max_part_size;
  
      printSerial(part_len);
    }

    printSerial(data[i]);
    n++;
    hash ^= data[i];

    if (n == max_part_size) {
      printSerial(hash);
      hash = 0;
      n = 0;
    }
  }

  if (n != 0) {
    printSerial(hash);
  }

  return 1;
} 

static void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (xSemaphoreTake(mux, portMAX_DELAY)) {
    if(!index){
      printSerial("BGN");
      filename.trim();
      printSerial(strlen(filename.c_str()));
      printSerial(filename);
      setBlue();
    } else {
      setPurple();
    }
    
    if(!writeSerialData(data, len)) {
      xSemaphoreGive(mux);
      request->send(500, "application/json", "{\"error\":\"failed to write serial\"}");
      return;
    }
  
    if(final){
      printSerial("FIN");
      setGreen();
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
  setBlue();
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
  Serial.begin(76800);
  // Serial.begin(115200);
  mux = xSemaphoreCreateMutex();

  pinMode(B_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(R_PIN, OUTPUT);
  setWhite();
  
  while (!connWifi()) {
    setRed();
  }

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
      delay(1000);
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