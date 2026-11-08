#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// WiFi
const char* ssid = "Tengku2";
const char* password = "tengkuirwansyah.";

// Pin
#define DHTPIN D4
#define DHTTYPE DHT22

const int ledPin = D6;
const int buttonPin = D2;
const int pwmPin = D1;

// Objek
DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Status
bool ledState = false;
bool lastButtonState = LOW;
bool buttonState = LOW;

int pwmValue = 0;

unsigned long lastDebounceTime = 0;
unsigned long lastReadTime = 0;

const unsigned long debounceDelay = 50;

String currentTemp = "--";
String currentHum = "--";

// Mengirim data ke dashboard
void notifyClients() {
  String json = "{";
  json += "\"led\":\"" + String(ledState ? "ON" : "OFF") + "\",";
  json += "\"suhu\":\"" + currentTemp + "\",";
  json += "\"hum\":\"" + currentHum + "\",";
  json += "\"pwm\":" + String(pwmValue);
  json += "}";

  ws.textAll(json);
}

// Menerima perintah dari dashboard
void handleWebSocketMessage(
  void *arg,
  uint8_t *data,
  size_t len
) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 &&
      info->len == len && info->opcode == WS_TEXT) {

    String message = "";

    for (size_t i = 0; i < len; i++) {
      message += (char)data[i];
    }

    if (message == "toggle") {
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
      notifyClients();
    }

    // Mengatur kecerahan LED menggunakan PWM
    if (message.startsWith("pwm,")) {
      pwmValue = message.substring(4).toInt();
      pwmValue = constrain(pwmValue, 0, 1023);

      analogWrite(pwmPin, pwmValue);
      notifyClients();
    }
  }
}

// Event WebSocket
void onEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }

  if (type == WS_EVT_CONNECT) {
    notifyClients();
  }
}

// Dashboard web
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Monitoring IoT PWM</title>

  <style>
    body {
      font-family: Arial;
      text-align: center;
      background: #f1f5f9;
      padding: 20px;
    }

    .card {
      background: white;
      padding: 20px;
      margin: 15px auto;
      border-radius: 12px;
      max-width: 350px;
      box-shadow: 0 2px 8px #ccc;
    }

    .value {
      font-size: 28px;
      font-weight: bold;
      color: #2563eb;
    }

    button {
      padding: 12px 20px;
      font-size: 16px;
      border: none;
      border-radius: 8px;
      background: #2563eb;
      color: white;
    }

    input[type=range] {
      width: 90%;
    }
  </style>
</head>

<body>
  <h2>Monitoring IoT</h2>

  <div class="card">
    <h3>Suhu</h3>
    <div class="value">
      <span id="tempValue">--</span> &deg;C
    </div>
  </div>

  <div class="card">
    <h3>Kelembapan</h3>
    <div class="value">
      <span id="humValue">--</span> %
    </div>
  </div>

  <div class="card">
    <h3>LED 1</h3>
    <div class="value" id="ledStatus">OFF</div>
    <br>
    <button onclick="toggleLED()">Nyalakan / Matikan LED 1</button>
  </div>

  <div class="card">
    <h3>Kecerahan LED 2 (PWM)</h3>
    <input
      type="range"
      min="0"
      max="1023"
      value="0"
      id="pwmSlider"
      oninput="sendPWM(this.value)"
    >

    <p>Nilai PWM: <span id="pwmValue">0</span></p>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    var pwmTimer;

    function initWebSocket() {
      websocket = new WebSocket(gateway);

      websocket.onmessage = function(event) {
        var data = JSON.parse(event.data);

        document.getElementById("tempValue").innerText = data.suhu;
        document.getElementById("humValue").innerText = data.hum;
        document.getElementById("ledStatus").innerText = data.led;
        document.getElementById("pwmValue").innerText = data.pwm;

        document.getElementById("pwmSlider").value = data.pwm;
      };

      websocket.onclose = function() {
        setTimeout(initWebSocket, 2000);
      };
    }

    function toggleLED() {
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("toggle");
      }
    }

    function sendPWM(value) {
      document.getElementById("pwmValue").innerText = value;

      clearTimeout(pwmTimer);

      pwmTimer = setTimeout(function() {
        if (websocket && websocket.readyState === WebSocket.OPEN) {
          websocket.send("pwm," + value);
        }
      }, 50);
    }

    window.onload = initWebSocket;
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(pwmPin, OUTPUT);

  digitalWrite(ledPin, LOW);
  analogWriteRange(1023);
  analogWrite(pwmPin, 0);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi gagal terhubung!");
    return;
  }

  Serial.println("\nWiFi terhubung!");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("Web server berjalan.");
}

void loop() {
  // Membaca push button untuk LED 1
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState ? HIGH : LOW);
        notifyClients();
      }
    }
  }

  lastButtonState = reading;

  // Membaca suhu dan kelembapan setiap 3 detik
  if (millis() - lastReadTime >= 3000) {
    lastReadTime = millis();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature)) {
      currentTemp = String(temperature, 1);
    }

    if (!isnan(humidity)) {
      currentHum = String(humidity, 1);
    }

    notifyClients();

    Serial.print("Suhu: ");
    Serial.print(currentTemp);
    Serial.print(" C | Kelembapan: ");
    Serial.print(currentHum);
    Serial.println(" %");
  }

  ws.cleanupClients();
}