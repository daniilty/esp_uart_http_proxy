#include <WiFi.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(4498);

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
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
  for(size_t i=0; i<len; i++){
    Serial.write(data[i]);
    hash ^= data[i];
  }
  Serial.println("SUM");
  Serial.write(hash);
  Serial.println();

  if (strcmp(readSerial().c_str(), "ACK") != 0) {
    return 0;
  }

  return 1;
} 

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!index){
    Serial.write("BGN");
  }

  int tries = 0;
  while(!writeSerialData(data, len)) {
    if (tries > 5) {
      request->send(500, "application/json", "{\"error\":\"failed to write serial\"}");
      return;
    }
    delay(100);
    tries++;
  }
 
  if(final){
    Serial.write("FIN");
  }
}

static String readSerial() {
  String dat = Serial.readStringUntil('\n');
  while (dat.isEmpty()) {
    dat = Serial.readStringUntil('\n');
    delay(500);
  }
  return dat;
}

static int connWifi() {
  String cmd = readSerial();
  if (strcmp(cmd.c_str(), "WSP") != 0) {
    Serial.println(cmd);
    return 0;
  }

  String ssidPassword = readSerial();
  int pos = ssidPassword.indexOf(":");
  if (pos == -1) {
    return 0;
  }

  String ssid = ssidPassword.substring(0, pos);
  String password = ssidPassword.substring(pos+1, ssidPassword.length());

  Serial.println(ssid);
  Serial.println(password);
  WiFi.begin(ssid, password);  

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (tries > 5) {
      return 0;
    }

    delay(1000);
    tries++;
  }

  return 1;
}

void setup() {
  Serial.begin(115200);
  
  while (!connWifi()) {
    Serial.print("WFL");
  }
  
  // Print the ESP32's IP address
  Serial.print("ESP32 Web Server's IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println(":4498");

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

void loop() {
  // Your code here
}