#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Данные вашей Wi-Fi сети
const char* ssid = "Vodafone-F50C";
const char* password = "TBPasswdEvr2023";

// Данные вашего сервера
const char* serverHost = "192.168.1.168"; // Замените на IP вашего сервера
const int serverPort = 8000;             // Порт вашего сервера
const char* serverPath = "/upload";      // Путь на сервере

// Пин светодиода на ESP32-CAM
#define LED_PIN 4
// Conversion factor for micro seconds to seconds
#define uS_TO_S_FACTOR 1000000ULL
// Time ESP32 will go to sleep (in seconds)
#define TIME_TO_SLEEP  10

void setup() {
  Serial.begin(115200);

  // Настройка светодиода
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Выключить светодиод по умолчанию

  // Настройка камеры
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA; // 640*480
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Инициализация камеры
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Камера не инициализирована. Ошибка: 0x%x", err);
    Serial.println("Уход в глубокий сон");
    esp_deep_sleep_start();
    return;
  }

  // Настройка яркости
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 1); // Установить яркость (от -2 до 2)
  }

  // Подключение к Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi подключен");
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");
  Serial.flush();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    sendPhotoToServer();
  } else {
    Serial.println("WiFi не подключен");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("Wi-Fi отключен, уход в сон");
  esp_deep_sleep_start();
}

void clearBuffer() {
  camera_fb_t *fb = NULL;
  for (int i = 0; i < 2; i++) { // Очистка 2 раза
    fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
  }
}

void sendPhotoToServer() {
  // Очистка буфера
  clearBuffer();

  // Включение светодиода перед фотографированием
  digitalWrite(LED_PIN, HIGH);
  delay(500); // Задержка для включения светодиода

  fb = esp_camera_fb_get();

  // Выключение светодиода после фотографирования
  digitalWrite(LED_PIN, LOW);

  if (!fb) {
    Serial.println("Не удалось получить изображение");
    return;
  }

  WiFiClient client;

  // Подключение к серверу
  if (!client.connect(serverHost, serverPort)) {
    Serial.println("Ошибка: не удалось подключиться к серверу");
    return;
  }
  Serial.println("Соединение с сервером установлено");

  // Формирование HTTP-запроса
  String boundary = "----ESP32Boundary";
  String bodyStart = "--" + boundary + "\r\n" +
                     "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n" +
                     "device_001\r\n" +
                     "--" + boundary + "\r\n" +
                     "Content-Disposition: form-data; name=\"file\"; filename=\"photo.jpg\"\r\n" +
                     "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  int contentLength = bodyStart.length() + fb->len + bodyEnd.length();

  String request = "POST " + String(serverPath) + " HTTP/1.1\r\n";
  request += "Host: " + String(serverHost) + ":" + String(serverPort) + "\r\n";
  request += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  request += "Content-Length: " + String(contentLength) + "\r\n";
  request += "Connection: close\r\n\r\n";

  // Отправка данных
  client.print(request);
  client.print(bodyStart);
  client.write(fb->buf, fb->len);
  client.print(bodyEnd);

  // Чтение ответа сервера
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break; // Конец заголовков ответа
    Serial.println(line);
  }

  String response = client.readString();
  Serial.println("Ответ сервера:");
  Serial.println(response);

  // Завершение соединения
  client.stop();
  Serial.println("Соединение закрыто");

  // Освобождение буфера
  esp_camera_fb_return(fb);
}
