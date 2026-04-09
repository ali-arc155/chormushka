// Импорт библиотек для работы с камерой ESP32-CAM
#include "esp_camera.h"
// Библиотека для подключения устройства к Wi‑Fi сети
#include <WiFi.h>
// Библиотека для создания веб‑сервера на ESP32 — позволяет управлять устройством через браузер
#include <WebServer.h>
// Библиотека для выполнения HTTP‑запросов (GET, POST и т. д.) — нужна для загрузки фото на Яндекс Диск
#include <HTTPClient.h>

// Указатель на буфер кадра камеры. Изначально не инициализирован (nullptr)
camera_fb_t* frame = nullptr;


// Токен авторизации для доступа к Яндекс Диску (замените на ваш реальный токен)
const char* yandexToken = "";

// Путь к папке на Яндекс Диске, куда будут загружаться фото
const String folderPath = "/ESP-CAM";

// Данные для подключения к Wi‑Fi сети
const char* ssid = "";      // Имя Wi‑Fi сети (замените на ваше)
const char* password = "";   // Пароль Wi‑Fi (замените на ваш)

// Конфигурация камеры для модуля AI‑Thinker ESP32‑CAM
camera_config_t config = {
  .pin_pwdn  = 32,        // Пин для управления питанием камеры (power down)
  .pin_reset = -1,       // Пин сброса камеры (-1 — не используется)
  .pin_xclk  = 0,       // Пин тактового сигнала XCLK
  .pin_sscb_sda = 26,  // Пин данных I2C (SDA) для управления сенсором
  .pin_sscb_scl = 27,  // Пин тактирования I2C (SCL) для управления сенсором
  .pin_d7    = 35,      // Пины данных D7–D0 для передачи видеоданных
  .pin_d6    = 34,
  .pin_d5    = 39,
  .pin_d4    = 36,
  .pin_d3    = 21,
  .pin_d2    = 19,
  .pin_d1    = 18,
  .pin_d0    = 5,
  .pin_vsync = 25,      // Пин вертикальной синхронизации
  .pin_href  = 23,    // Пин горизонтальной синхронизации
  .pin_pclk  = 22,   // Пин пиксельного тактового сигнала
  .xclk_freq_hz = 20000000,  // Частота тактового сигнала (20 МГц)
  .ledc_timer = LEDC_TIMER_0,  // Таймер для управления светодиодом
  .ledc_channel = LEDC_CHANNEL_0, // Канал LEDC для светодиода
  .pixel_format = PIXFORMAT_JPEG,  // Формат пикселей: JPEG (сжатое изображение)
  .frame_size = FRAMESIZE_VGA,   // Разрешение кадра: VGA (640×480)
  .jpeg_quality = 5,     // Качество JPEG (1–63, чем меньше — тем ниже качество)
  .fb_count = 1         // Количество буферов кадра (1 — экономия памяти)
};

// Создаём объект веб‑сервера, работающий на порту 80
WebServer server(80);

// HTML‑страница для веб‑интерфейса управления
const char HTML_PAGE[] = R"raw(
<html>
<head>
  <meta charset="UTF-8">
  <title>Птичья кормушка</title>
  <style>
    body { text-align:center; font-family:sans-serif; margin:20px; }
    button { padding:12px 24px; font-size:16px; border:none; border-radius:4px; cursor:pointer; margin:0 10px; }
    #feed-result { margin-top:15px; font-style:italic; color:#555; }
  </style>
</head>
<body>
  <h2>Управление птичьей кормушкой</h2>
  <br><br>
  <button onclick="window.location.href='/stream'" style="background:#4CAF50;color:white;">
    посмотреть птичек
  </button>
  <button onclick=fetch('/food')" style="background:#4CAF50;color:white;">
    покормить птичек
  </button>
  <button onclick=fetch('/show')" style="background:#4CAF50;color:white;">
    отправить птичек
  </button>
  <div id="feed-result"></div>
</body>
</html>)raw";

void handleStream() {
  String resp = "HTTP/1.1 200 OK\r\n";
  resp += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.client().print(resp);  // Отправляем HTTP‑заголовок для MJPG‑стрима
  unsigned long int start_time_stream = millis() + 60000;  // Время работы стрима — 60 секунд
  while ((millis() < start_time_stream) && (server.client().connected())) {
    frame = esp_camera_fb_get();  // Получаем кадр с камеры
    if (!frame) {
      Serial.println("Camera capture failed");  // Если кадр не получен — выводим ошибку
      break;
    }
    // Отправляем заголовок с указанием типа и длины данных
    server.client().printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", frame->len);
    // Отправляем сами данные кадра
    server.client().write(frame->buf, frame->len);
    server.client().print("\r\n");
    esp_camera_fb_return(frame);  // Освобождаем буфер кадра

    if (digitalRead(2)) {
      Serial.println("1qqqqq");  // Отладочный вывод (можно удалить после отладки)
      send_to_drive();  // Если кнопка нажата (пин 2), отправляем фото на Яндекс Диск
    }
  }
}

// Обработка главной страницы — отдаёт HTML‑страницу
void handleRoot() {
  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html", HTML_PAGE);
}

void setup() {
  #if defined(RTC_CNTL_BROWN_OUT_REG)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Отключаем защиту от пониженного напряжения (brownout detection) — предотвращает сброс при кратковременных просадках питания
  #endif

  Serial.begin(115200);  // Инициализируем последовательный порт для отладки (скорость 115200 бод)

  // Подключение к Wi‑Fi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к Wi‑Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");  // Выводим точки в мониторе порта, пока идёт подключение
  }
  Serial.println("");
  Serial.println("Wi‑Fi подключён!");
  Serial.print("IP‑адрес: http://");
  Serial.println(WiFi.localIP());  // Выводим IP‑адрес устройства — по нему можно открыть веб‑интерфейс

  // Инициализация камеры с заданной конфигурацией
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: 0x%x", err);  // Если инициализация не удалась, выводим код ошибки
    return;
  }

  // Получаем указатель на сенсор камеры для дальнейшей настройки
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);  // Включаем вертикальный переворот изображения (vflip) — чтобы картинка не была вверх ногами

  // Регистрируем обработчики HTTP‑запросов для разных URL
  server.on("/", HTTP_GET, handleRoot);      // Главная страница (/)
  server.on("/stream", HTTP_GET, handleStream);  // Потоковая передача видео (/stream)
  server.on("/food", HTTP_GET, feed);        // Команда «покормить птичек» (/food)
  server.on("/show", HTTP_GET, show);        // Команда «отправить фото птичек» (/show)

  server.begin();  // Запускаем веб‑сервер

  Serial.println("Веб‑сервер запущен");

  // Делаем тестовый снимок и сразу отправляем его на Яндекс Диск
  camera_fb_t* fb = esp_camera_fb_get();  // Получаем кадр с камеры
  esp_camera_fb_return(fb);  // Освобождаем буфер кадра
  send_to_drive();  // Отправляем фото на Яндекс Диск

  pinMode(2, INPUT_PULLDOWN);  // Настраиваем пин 2 как вход с подтяжкой вниз (для кнопки)
}

// Основной цикл программы — выполняется постоянно
void loop() {
  server.handleClient();  // Обрабатываем входящие HTTP‑запросы от клиентов (браузеров и т. д.)

  // Проверяем состояние пина 2: если на нём высокий уровень (нажата кнопка), отправляем фото на Яндекс Диск
  if (digitalRead(2)) {
    send_to_drive();  // Вызываем функцию отправки фото на Яндекс Диск
  }
}

// Обработчик команды «покормить птичек»
void feed() {
  server.send(200);  // Отправляем HTTP‑ответ 200 OK (успех) — браузер получает подтверждение
  Serial.println("Корм подан");  // Выводим сообщение в монитор порта для отладки
}

// Обработчик команды «отправить фото птичек»
void show() {
  server.send(200);  // Отправляем HTTP‑ответ 200 OK
  Serial.println("Отправка фото");  // Выводим сообщение в монитор порта
  send_to_drive();  // Вызываем функцию отправки фото на Яндекс Диск
}

// Функция отправки фото с камеры на Яндекс Диск
void send_to_drive() {
  // Получаем кадр с камеры
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed or FB-OVF");  // Если кадр не получен — выводим ошибку
    delay(100);
    return;
  }

  // Копируем данные кадра в выделенную память, чтобы освободить буфер камеры как можно быстрее
  uint8_t* image_data = (uint8_t*)malloc(fb->len);
  size_t image_len = 0;

  if (image_data) {
    memcpy(image_data, fb->buf, fb->len);  // Копируем данные из буфера камеры в выделенную память
    image_len = fb->len;  // Сохраняем длину данных для последующей отправки
  } else {
    Serial.println("Memory allocation failed");  // Если не удалось выделить память — выводим ошибку
    esp_camera_fb_return(fb);
    return;
  }
  esp_camera_fb_return(fb);  // Освобождаем буфер камеры — важно сделать это как можно раньше

  // ЭТАП 1: Получаем URL для загрузки файла на Яндекс Диск
  {
    String fileName = "ESP32_" + String(millis()) + ".jpg";  // Формируем имя файла с меткой времени (чтобы избежать дубликатов)
    String getUploadUrl = "https://cloud-api.yandex.net/v1/disk/resources/upload?path=" + folderPath + "/" + fileName + "&overwrite=true";

    HTTPClient http;
    http.setTimeout(10000);  // Устанавливаем таймаут 10 секунд для HTTP‑запроса

    // Отправляем запрос для получения URL загрузки
    http.begin(getUploadUrl);
    http.addHeader("Authorization", "OAuth " + String(yandexToken));  // Добавляем заголовок с токеном авторизации
    http.addHeader("Accept", "application/json");  // Указываем, что ожидаем JSON в ответе

    int httpCode = http.GET();  // Выполняем GET‑запрос
    if (httpCode == 200) {
      String response = http.getString();  // Получаем ответ от сервера

      // Парсим JSON‑ответ (вручную ищем URL загрузки)
      int start = response.indexOf("\"href\":\"") + 8;  // Находим начало URL в JSON
      int end = response.indexOf("\"", start);  // Находим конец URL
      if (start >= 8 && end > start) {
        String uploadUrl = response.substring(start, end);  // Извлекаем URL для загрузки
        http.end();  // Закрываем первое соединение

        // ЭТАП 2: Отправляем файл на Яндекс Диск методом PUT
        http.begin(uploadUrl);
        http.addHeader("Content-Type", "application/octet-stream");  // Указываем тип данных
        httpCode = http.PUT(image_data, image_len);  // Выполняем PUT‑запрос с данными изображения

        if (httpCode > 0) {
          Serial.println("File uploaded to Yandex Disk successfully");  // Успешная загрузка
        } else {
          Serial.println("Upload to Yandex Disk failed: " + String(httpCode));  // Ошибка загрузки
        }
      } else {
        Serial.println("Failed to parse upload URL from Yandex response");  // Не удалось извлечь URL из ответа
      }
    } else {
      Serial.println("Failed to get upload URL from Yandex: " + String(httpCode));  // Ошибка при получении URL
    }
    http.end();  // Завершаем HTTP‑сессию
  }

  // Освобождаем выделенную память для данных изображения
  free(image_data);
}
