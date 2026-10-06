#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// ===============================
// WIFI
// ===============================
const char* ssid = "Tengku";
const char* password = "tengkuirwansyah.";

// ===============================
// PIN
// ===============================
#define DHTPIN D4
#define DHTTYPE DHT22

#define LED_PIN D6
#define BUTTON_PIN D2

// ===============================
// DHT22
// ===============================
DHT dht(DHTPIN, DHTTYPE);

// ===============================
// SERVER
// ===============================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ===============================
// VARIABLE
// ===============================
bool ledState = false;

String currentTemp = "--";
String currentHum = "--";

unsigned long previousMillis = 0;
const unsigned long interval = 3000;

// ===============================
// HTML DASHBOARD
// ===============================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

  <meta name="viewport" content="width=device-width, initial-scale=1">

  <title>ESP8266 Dashboard</title>

  <style>

    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background: #f2f2f2;
      margin-top: 40px;
    }

    h1 {
      font-size: 40px;
    }

    .card {
      background: white;
      width: 300px;
      margin: 20px auto;
      padding: 25px;
      border-radius: 15px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.2);
    }

    .value {
      font-size: 35px;
      font-weight: bold;
    }

    button {
      padding: 12px 25px;
      font-size: 18px;
      cursor: pointer;
    }

  </style>

</head>

<body>

  <h1>ESP8266 Dashboard</h1>

  <!-- TEMPERATURE -->

  <div class="card">

    <h2>Temperature</h2>

    <div class="value">

      <span id="tempValue">--</span> °C

    </div>

  </div>


  <!-- HUMIDITY -->

  <div class="card">

    <h2>Humidity</h2>

    <div class="value">

      <span id="humValue">--</span> %

    </div>

  </div>


  <!-- LED -->

  <div class="card">

    <h2>LED Status</h2>

    <div class="value" id="ledStatus">

      OFF

    </div>

    <br>

    <button onclick="toggleLED()">

      Toggle LED

    </button>

  </div>


<script>

  var websocket;


  // ===============================
  // WEBSOCKET
  // ===============================

  function initWebSocket() {

    websocket = new WebSocket(
      "ws://" + window.location.hostname + "/ws"
    );


    websocket.onopen = function() {

      console.log("WebSocket Connected");

    };


    websocket.onclose = function() {

      console.log("WebSocket Disconnected");

      setTimeout(initWebSocket, 2000);

    };


    websocket.onmessage = function(event) {

      var data = JSON.parse(event.data);


      // Temperature

      document.getElementById("tempValue").innerHTML =
        data.temp;


      // Humidity

      document.getElementById("humValue").innerHTML =
        data.hum;


      // LED

      document.getElementById("ledStatus").innerHTML =
        data.led;

    };

  }


  // ===============================
  // TOGGLE LED
  // ===============================

  function toggleLED() {

    if (websocket.readyState === WebSocket.OPEN) {

      websocket.send("toggle");

    }

  }


  // Jalankan WebSocket

  window.onload = function() {

    initWebSocket();

  };

</script>

</body>

</html>
)rawliteral";


// ===============================
// KIRIM DATA KE BROWSER
// ===============================
void notifyClients() {

  String message = "{";

  message += "\"temp\":\"";
  message += currentTemp;
  message += "\",";

  message += "\"hum\":\"";
  message += currentHum;
  message += "\",";

  message += "\"led\":\"";

  if (ledState) {

    message += "ON";

  } else {

    message += "OFF";

  }

  message += "\"";

  message += "}";


  ws.textAll(message);

}


// ===============================
// WEBSOCKET EVENT
// ===============================
void onWebSocketEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {

  // CLIENT CONNECT

  if (type == WS_EVT_CONNECT) {

    Serial.println("WebSocket Client Connected");

    String message = "{";

    message += "\"temp\":\"";
    message += currentTemp;
    message += "\",";

    message += "\"hum\":\"";
    message += currentHum;
    message += "\",";

    message += "\"led\":\"";

    if (ledState) {

      message += "ON";

    } else {

      message += "OFF";

    }

    message += "\"";

    message += "}";

    client->text(message);

  }


  // CLIENT DISCONNECT

  else if (type == WS_EVT_DISCONNECT) {

    Serial.println("WebSocket Client Disconnected");

  }


  // DATA FROM BROWSER

  else if (type == WS_EVT_DATA) {

    String message = "";

    for (size_t i = 0; i < len; i++) {

      message += (char)data[i];

    }


    if (message == "toggle") {

      ledState = !ledState;

      digitalWrite(
        LED_PIN,
        ledState ? HIGH : LOW
      );

      notifyClients();

    }

  }

}


// ===============================
// SETUP
// ===============================
void setup() {

  // SERIAL

  Serial.begin(115200);

  delay(500);


  // ===============================
  // PIN LED
  // ===============================

  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);


  // ===============================
  // PIN BUTTON
  // ===============================

  pinMode(BUTTON_PIN, INPUT);


  // ===============================
  // START DHT22
  // ===============================

  dht.begin();

  // DHT22 membutuhkan waktu setelah mulai

  delay(2000);


  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP8266 DASHBOARD");
  Serial.println("DHT22 + WEBSOCKET");
  Serial.println("==============================");


  // ===============================
  // WIFI
  // ===============================

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);


  Serial.print("Menghubungkan ke WiFi");


  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");

  }


  Serial.println();

  Serial.println("WiFi Terhubung!");


  Serial.print("IP Address: ");

  Serial.println(WiFi.localIP());


  // ===============================
  // WEBSOCKET
  // ===============================

  ws.onEvent(onWebSocketEvent);

  server.addHandler(&ws);


  // ===============================
  // WEB SERVER
  // ===============================

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {

    request->send_P(
      200,
      "text/html",
      index_html
    );

  });


  server.begin();


  Serial.println("Web Server aktif!");

  Serial.println("Buka IP Address di browser.");

}


// ===============================
// LOOP
// ===============================
void loop() {


  // ===============================
  // PUSH BUTTON
  // ===============================

  static bool lastButtonState = LOW;

  bool buttonState = digitalRead(BUTTON_PIN);


  if (
    buttonState == HIGH &&
    lastButtonState == LOW
  ) {

    ledState = !ledState;


    digitalWrite(
      LED_PIN,
      ledState ? HIGH : LOW
    );


    notifyClients();


    delay(50);

  }


  lastButtonState = buttonState;


  // ===============================
  // BACA DHT22
  // ===============================

  unsigned long currentMillis = millis();


  if (
    currentMillis - previousMillis >= interval
  ) {

    previousMillis = currentMillis;


    // BACA HUMIDITY

    float humidity = dht.readHumidity();


    // BACA TEMPERATURE

    float temperature = dht.readTemperature();


    // ===============================
    // CEK HASIL PEMBACAAN
    // ===============================

    if (
      isnan(humidity) ||
      isnan(temperature)
    ) {

      Serial.println(
        "Gagal membaca DHT22!"
      );

    }


    else {

      // SIMPAN TEMPERATURE

      currentTemp = String(
        temperature,
        1
      );


      // SIMPAN HUMIDITY

      currentHum = String(
        humidity,
        1
      );


      // ===============================
      // SERIAL MONITOR
      // ===============================

      Serial.print("Temperature: ");

      Serial.print(currentTemp);

      Serial.print(" °C");


      Serial.print(" | Humidity: ");

      Serial.print(currentHum);

      Serial.println(" %");


      // ===============================
      // KIRIM KE DASHBOARD
      // ===============================

      notifyClients();

    }

  }

}