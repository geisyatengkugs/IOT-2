#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

const int ledPin = D6;

// Data sensor terakhir yang diterima
float suhuTerakhir = 0.0;
int adcTerakhir = 1023;

bool dataSuhuMasuk = false;
bool dataCahayaMasuk = false;
bool statusLED = false;

Scheduler userScheduler;
painlessMesh mesh;

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.println("Gagal membaca JSON!");
    return;
  }

  const char* tipe = doc["tipe"] | "";

  // Perbarui data suhu dari Node 1
  if (strcmp(tipe, "suhu_node") == 0) {
    suhuTerakhir = doc["suhu"] | 0.0;
    dataSuhuMasuk = true;

    Serial.printf(
      "Suhu: %.2f C | Kelembapan: %.2f %%\n",
      suhuTerakhir,
      (float)(doc["kelembapan"] | 0.0)
    );
  }

  // Perbarui data cahaya dari Node 2
  else if (strcmp(tipe, "cahaya_node") == 0) {
    adcTerakhir = doc["adc"] | 1023;
    dataCahayaMasuk = true;

    Serial.printf("Nilai ADC LDR: %d\n", adcTerakhir);
  }

  else {
    return;
  }

  // Tunggu sampai kedua sensor mengirimkan data
  if (!dataSuhuMasuk || !dataCahayaMasuk) {
    Serial.println("Menunggu data kedua sensor...");
    return;
  }

  // Rule engine: suhu tinggi ATAU kondisi gelap
  bool kondisiKritis =
    (suhuTerakhir > 31.0) ||
    (adcTerakhir < 300);

  if (kondisiKritis) {
    statusLED = true;
  } else {
    statusLED = false;
  }

  digitalWrite(
    ledPin,
    statusLED ? HIGH : LOW
  );

  Serial.println("----- STATUS HUB -----");
  Serial.printf("Suhu terakhir : %.2f C\n", suhuTerakhir);
  Serial.printf("ADC terakhir  : %d\n", adcTerakhir);
  Serial.printf(
    "LED           : %s\n",
    statusLED ? "MENYALA" : "MATI"
  );
  Serial.println("----------------------");
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("Node terhubung: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("Topologi mesh berubah");
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  mesh.setDebugMsgTypes(ERROR | STARTUP);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    MESH_PORT
  );

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  // Node 3 hanya menerima data, tanpa task pengiriman.
  Serial.println("Node 3 Hub LED siap");
}

void loop() {
  mesh.update();
}
