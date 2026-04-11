#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

camera_fb_t* frame = nullptr;
SemaphoreHandle_t cameraMutex = nullptr;

const char* yandexToken = "*";
IPAddress clientIP;
IPAddress loginip;
const String folderPath = "/ESP-CAM";
bool direction = true;
const char* ssid = "*";
const char* password = "*";
const char* adminUser = "dafsdg76@mail.ru";
const char* adminPassword = "1qaaa2qweasdq31";
const int servoPin = 13;

camera_config_t config = {
  .pin_pwdn = 32,
  .pin_reset = -1,
  .pin_xclk = 0,
  .pin_sscb_sda = 26,
  .pin_sscb_scl = 27,
  .pin_d7 = 35,
  .pin_d6 = 34,
  .pin_d5 = 39,
  .pin_d4 = 36,
  .pin_d3 = 21,
  .pin_d2 = 19,
  .pin_d1 = 18,
  .pin_d0 = 5,
  .pin_vsync = 25,
  .pin_href = 23,
  .pin_pclk = 22,
  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_QVGA,
  .jpeg_quality = 10,
  .fb_count = 1
};

WebServer server(80);

const char HTML_PAGE[] = R"raw(<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Птичья кормушка</title><style>body { text-align:center; font-family:sans-serif; margin:20px; }button { padding:12px 24px; font-size:16px; border:none; border-radius:4px; cursor:pointer; margin:0 10px; }#feed-result { margin-top:15px; font-style:italic; color:#555; }.video-container { margin: 20px 0; }</style></head><body><h2>Управление птичьей кормушкой</h2><button onclick="window.location.href='/stream'" style="background:#4CAF50;color:white;">посмотреть птичек</button><button onclick="fetch('/food')" style="background:#4CAF50;color:white;">покормить птичек</button><button onclick="fetch('/show')" style="background:#4CAF50;color:white;">отправить птичек</button><button onclick="window.location.href='/forAdmins'" style="background:#4CAF50;color:white;">полное управление</button><div id="feed-result"></div></body></html>)raw";

const char loginPage[] = R"raw(<!DOCTYPE HTML><html><head><meta name="viewport" content="width=device-width, initial-scale=1", charset="UTF-8"><style>body { font-family: Arial; text-align: center; margin: 50px; background: #f0f0f0; }.container { max-width: 400px; margin: auto; padding: 30px; background: white; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }input { width: 100%; padding: 12px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }button:hover { background: #45a049; }h2 { margin-top: 0; }.error { color: red; }</style></head><body><div class="container"><h2>Вход в панель управления</h2><form action="/login" method="post"><input type="text" name="username" placeholder="Логин" required><input type="password" name="password" placeholder="Пароль" required><button type="submit">Войти</button></form></div></body></html>)raw";

const char adminPage[] = R"rawliteral(<!DOCTYPE HTML><html><head><meta name="viewport" content="width=device-width, initial-scale=1", charset="UTF-8"><style>body { font-family: Arial; text-align: center; margin: 50px; }.container { max-width: 600px; margin: auto; }.btn { padding: 15px 30px; margin: 10px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }.info { background-color: #2196F3; color: white; }.logout { background-color: #808080; color: white; }</style></head><body><div class="container"><h1>Панель управления ESP32</h1><div style="margin-top: 30px;"><h3>Информация</h3><button class="btn info" onclick="getInfo()">Получить статус</button><button class="btn info" onclick="fetch('/restart')">Перезагрузить ESP</button><div id="status"></div></div><div style="margin-top: 30px;"><button class="btn logout" onclick="logout()">Выйти</button></div></div><script>function getInfo() {fetch('/info').then(response => {if (!response.ok) throw new Error('Unauthorized');return response.json();}).then(data => {document.getElementById('status').innerHTML = `<p>IP: ${data.ip}</p><p>Память: ${data.memory} байт свободно</p><p>Температура: ${data.temp}°C</p>`;}).catch(error => {alert('Ошибка доступа. Пожалуйста, войдите снова.');window.location.href = '/logout';});}function logout() {window.location.href = '/logout';}</script></body></html>)rawliteral";


void handleStream() {
  String resp = "HTTP/1.1 200 OK\r\n";
  resp += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.client().print(resp);
  unsigned long int start_time_stream = millis() + 60000;
  while ((millis() < start_time_stream) && server.client().connected()) {
    frame = esp_camera_fb_get();
    if (!frame) {
      Serial.println("Camera capture failed in stream");
      break;
    }
    server.client().printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", frame->len);
    server.client().write(frame->buf, frame->len);
    server.client().print("\r\n");
    esp_camera_fb_return(frame);
    frame = nullptr;
  }
}

void handleRoot() {
  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html", HTML_PAGE);
}

void feed() {
  server.send(200);
  if (direction) {
    for (int i = 32; i < 149; i += 2) {
      setServoAngleManual(i);
      delay(10);
    }
    direction = false;
  } else {
    for (int i = 148; i > 31; i -= 2) {
      setServoAngleManual(i);
      delay(10);
    }
    direction = true;
  }
}

void handleRestart() {
  clientIP = server.client().remoteIP();
  if (clientIP != loginip) { return; }
  server.send(200);
  delay(100);
  ESP.restart();
}

void setServoAngleManual(int angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  int pulseWidth = map(angle, 0, 180, 544, 2400);
  digitalWrite(servoPin, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(servoPin, LOW);
  int delayTime = 20 - (pulseWidth / 1000);
  if (delayTime > 0) delay(delayTime);
}

void show() {
  server.send(200);
  send_to_drive();
}

void handleLogout() {
  clientIP = server.client().remoteIP();
  if (clientIP != loginip) { return; }
  loginip = IPAddress(192, 168, 1, 1);
  server.send(200, "text/html", loginPage);
}

void admin() {
  clientIP = server.client().remoteIP();
  if (clientIP == loginip) {
    server.send(200, "text/html", adminPage);
  } else {
    server.send(200, "text/html", loginPage);
  }
}

void handleLogin() {
  String username = server.arg("username");
  String password = server.arg("password");
  if (username == adminUser && password == adminPassword) {
    loginip = server.client().remoteIP();
    server.send(200, "text/html", adminPage);
  } else {
    server.send(200, "text/html", loginPage);
  }
}

void send_to_drive() {
  if (xSemaphoreTake(cameraMutex, portMAX_DELAY) != pdTRUE) {
    Serial.println("Camera mutex timeout");
    return;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    xSemaphoreGive(cameraMutex);
    return;
  }
  uint8_t* image_data = (uint8_t*)malloc(fb->len);
  if (!image_data) {
    Serial.println("Memory allocation failed");
    esp_camera_fb_return(fb);
    xSemaphoreGive(cameraMutex);
    return;
  }
  memcpy(image_data, fb->buf, fb->len);
  size_t image_len = fb->len;
  esp_camera_fb_return(fb);
  String fileName = "ESP32_" + String(millis()) + ".jpg";
  String getUploadUrl = "https://cloud-api.yandex.net/v1/disk/resources/upload?path=" + folderPath + "/" + fileName + "&overwrite=true";
  HTTPClient http;
  http.begin(getUploadUrl);
  http.addHeader("Authorization", "OAuth " + String(yandexToken));
  http.addHeader("Accept", "application/json");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    int start = response.indexOf("\"href\":\"") + 8;
    int end = response.indexOf("\"", start);
    if (start != -1 && end != -1) {
      String uploadUrl = response.substring(start, end);
      http.end();
      http.begin(uploadUrl);
      http.addHeader("Content-Type", "application/octet-stream");
      httpCode = http.PUT(image_data, image_len);
      if (httpCode > 0) {
        Serial.println("File uploaded to Yandex Disk successfully");
      } else {
        Serial.println("Upload to Yandex Disk failed: " + String(httpCode));
      }
    } else {
      Serial.println("Failed to parse upload URL from Yandex response");
    }
  } else {
    Serial.println("Failed to get upload URL from Yandex: " + String(httpCode));
  }
  http.end();
  free(image_data);
  xSemaphoreGive(cameraMutex);
}

void handleInfo() {
  clientIP = server.client().remoteIP();
  if (clientIP != loginip) { return; }
  StaticJsonDocument<200> doc;
  doc["ip"] = WiFi.localIP().toString();
  doc["memory"] = ESP.getFreeHeap();
  doc["temp"] = temperatureRead();
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

void setup() {
#if defined(RTC_CNTL_BROWN_OUT_REG)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi подключён!");
  Serial.print("IP-адрес: http://");
  Serial.println(WiFi.localIP());
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: 0x%x", err);
    return;
  }
  sensor_t* s = esp_camera_sensor_get();
  cameraMutex = xSemaphoreCreateMutex();
  if (cameraMutex == nullptr) {
    Serial.println("Failed to create camera mutex");
    ESP.restart();
  }
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/food", HTTP_GET, feed);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/show", HTTP_GET, show);
  server.on("/forAdmins", HTTP_GET, admin);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/restart", HTTP_GET, handleRestart);
  server.on("/logout", HTTP_GET, handleLogout);
  server.begin();
  pinMode(16, INPUT_PULLDOWN);
  pinMode(servoPin, OUTPUT);
}

void loop() {
  server.handleClient();
}
