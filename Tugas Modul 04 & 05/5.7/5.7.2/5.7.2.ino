
#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

const int ldrPin = A0;

Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();

Task taskSendMessage(
  TASK_SECOND * 3,
  TASK_FOREVER,
  &sendMessage
);

void sendMessage() {
  int adc = analogRead(ldrPin);

  StaticJsonDocument<200> doc;
  doc["tipe"] = "cahaya_node";
  doc["adc"] = adc;

  String msg;
  serializeJson(doc, msg);

  mesh.sendBroadcast(msg);

  Serial.print("[KIRIM LDR] ");
  Serial.println(msg);
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[PESAN DARI %u] %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("Node terhubung: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("Topologi mesh berubah");
}

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes(ERROR | STARTUP);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    &userScheduler,
    MESH_PORT
  );

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.println("Node 2 Sensor LDR siap");
}

void loop() {
  mesh.update();
}
