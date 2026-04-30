/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v7.1.0 - PROFESSIONAL MASTER EDITION
 * 
 * Ecosistema completo de control de acceso industrial.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Hardware_Map_Lite_Vnzla.h"

// --- OBJETOS Y CONFIGURACIÓN ---
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
OneWire ds(PIN_IBUTTON);
Preferences configPrefs;
Preferences aclPrefs;
Preferences logPrefs;

char node_id[32], mqtt_server[64], wifi_ssid[32], wifi_pass[64];
const char* topic_control = "smartaccess/nodos/control";
const char* topic_events = "smartaccess/nodos/eventos";
const char* ntp_server = "pool.ntp.org";

// --- CRONÓMETROS Y ESTADOS ---
unsigned long lastHeartbeat = 0, lastIButton = 0, lastSensor = 0, lastLogSync = 0;
unsigned long relay1Start = 0, relay2Start = 0, ledPatternTime = 0, mqttRetryTime = 0;
bool relay1Active = false, relay2Active = false, lastDoor = HIGH, lastTamper = HIGH;

enum SystemMode { CONFIG_MODE, RUN_MODE } currentMode = CONFIG_MODE;
enum UIState { UI_CONNECTING, UI_READY, UI_GRANTED, UI_DENIED, UI_ALARM, UI_OTA } currentUI = UI_CONNECTING;

// --- PORTAL WEB (HTML/CSS) ---
const char* HTML_BODY = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>SmartAccess v7.1</title><style>
body { font-family: 'Segoe UI', sans-serif; background: #1a1a1a; color: #fff; text-align: center; }
.card { background: #2d2d2d; padding: 30px; border-radius: 15px; display: inline-block; margin-top: 50px; box-shadow: 0 10px 20px rgba(0,0,0,0.5); width: 90%; max-width: 400px; }
h2 { color: #00d2ff; }
input { display: block; width: 100%; margin: 15px 0; padding: 12px; border-radius: 5px; border: 1px solid #444; background: #3d3d3d; color: #fff; box-sizing: border-box; }
button { background: linear-gradient(135deg, #00d2ff 0%, #3a7bd5 100%); color: white; border: none; padding: 12px; width: 100%; border-radius: 5px; cursor: pointer; font-weight: bold; }
p { font-size: 0.8em; color: #888; }
</style></head><body><div class="card"><h2>SmartAccess v7.1</h2><p>Nodo de Borde Industrial</p>
<form action="/save" method="POST">
<input name="s" placeholder="WiFi SSID (Red)">
<input name="p" type="password" placeholder="WiFi Password">
<input name="m" placeholder="MQTT Broker IP/DNS">
<input name="i" placeholder="Node ID (ej. 01)">
<button type="submit">GUARDAR Y REINICIAR</button>
</form></div></body></html>
)rawliteral";

// --- UTILIDADES ---

String getTS() {
    struct tm ti; if (!getLocalTime(&ti)) return "0000-00-00 00:00:00";
    char b[25]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &ti); return String(b);
}

void notify(String type, String msg, String data = "") {
    StaticJsonDocument<512> doc;
    doc["node"] = node_id; doc["type"] = type; doc["msg"] = msg;
    if (data != "") doc["data"] = data;
    doc["ts"] = getTS();
    doc["heap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    
    char b[512]; serializeJson(doc, b);
    if (mqttClient.connected()) mqttClient.publish(topic_events, b);
    else {
        logPrefs.begin("logs", false);
        int c = logPrefs.getInt("c", 0);
        logPrefs.putString(("l"+String(c)).c_str(), b);
        logPrefs.putInt("c", c + 1);
        logPrefs.end();
    }
}

void syncLogs() {
    if (!mqttClient.connected() || (millis() - lastLogSync < 5000)) return;
    lastLogSync = millis();
    logPrefs.begin("logs", false);
    int count = logPrefs.getInt("c", 0);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            String logMsg = logPrefs.getString(("l"+String(i)).c_str(), "");
            if (logMsg != "") {
                StaticJsonDocument<512> doc; deserializeJson(doc, logMsg);
                doc["sync"] = true;
                char buffer[512]; serializeJson(doc, buffer);
                mqttClient.publish(topic_events, buffer);
            }
        }
        logPrefs.clear(); logPrefs.putInt("c", 0);
    }
    logPrefs.end();
}

// --- COMANDOS Y SERVIDOR ---

void handleSave() {
    configPrefs.begin("cfg", false);
    configPrefs.putString("s", server.arg("s"));
    configPrefs.putString("p", server.arg("p"));
    configPrefs.putString("m", server.arg("m"));
    configPrefs.putString("i", server.arg("i"));
    configPrefs.end();
    server.send(200, "text/plain", "OK. Reiniciando...");
    delay(2000); ESP.restart();
}

void handleCommands(String json) {
    StaticJsonDocument<256> doc; if (deserializeJson(doc, json)) return;
    if (doc["id"] != node_id && doc["id"] != "ALL") return;
    String cmd = doc["cmd"] | "";
    if (cmd == "REBOOT") ESP.restart();
    else if (cmd == "ADD") { aclPrefs.begin("acl", false); aclPrefs.putBool(doc["key"], true); aclPrefs.end(); notify("ACL", "ADDED", doc["key"]); }
    else if (cmd == "DEL") { aclPrefs.begin("acl", false); aclPrefs.remove(doc["key"]); aclPrefs.end(); notify("ACL", "REMOVED", doc["key"]); }
}

// --- HARDWARE ---

void handleUI() {
    unsigned long n = millis(); static bool s = false;
    int i = (currentUI == UI_OTA || currentUI == UI_ALARM) ? 50 : (currentUI == UI_CONNECTING) ? 500 : (currentUI == UI_DENIED) ? 100 : 0;
    if (i > 0 && (n - ledPatternTime > i)) { ledPatternTime = n; s = !s; digitalWrite(PIN_LED_STATUS, s); }
    if (currentUI == UI_GRANTED) digitalWrite(PIN_LED_STATUS, HIGH);
    else if (currentUI == UI_READY) digitalWrite(PIN_LED_STATUS, LOW);
}

void handleHardware() {
    unsigned long n = millis();
    if (n - lastIButton > 150) {
        lastIButton = n; byte addr[8];
        if (ds.search(addr)) {
            if (OneWire::crc8(addr, 7) == addr[7]) {
                char ks[17]; sprintf(ks, "%02X%02X%02X%02X%02X%02X%02X%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
                aclPrefs.begin("acl", true); bool auth = aclPrefs.isKey(ks); aclPrefs.end();
                if (auth) {
                    currentUI = UI_GRANTED; ledPatternTime = n;
                    digitalWrite(PIN_RELE_1, HIGH); digitalWrite(PIN_RELE_2, HIGH);
                    relay1Start = relay2Start = n; relay1Active = relay2Active = true;
                    notify("ACCESS", "GRANTED", ks);
                } else {
                    currentUI = UI_DENIED; ledPatternTime = n;
                    notify("ACCESS", "DENIED", ks);
                }
            }
            ds.reset_search();
        }
    }
    if (relay1Active && (n - relay1Start > 1000)) { digitalWrite(PIN_RELE_1, LOW); relay1Active = false; }
    if (relay2Active && (n - relay2Start > 30000)) { digitalWrite(PIN_RELE_2, LOW); relay2Active = false; }
    if (n - lastSensor > 1000) {
        lastSensor = n;
        bool d = digitalRead(PIN_SENSOR_DOOR), t = digitalRead(PIN_TAMPER_SW);
        if (d != lastDoor) { lastDoor = d; notify("SENSOR", d == LOW ? "CLOSED" : "OPENED"); }
        if (t == HIGH && lastTamper == LOW) { lastTamper = HIGH; currentUI = UI_ALARM; notify("ALARM", "TAMPER"); }
        else if (t == LOW && lastTamper == HIGH) { lastTamper = LOW; currentUI = UI_READY; notify("ALARM", "RESTORED"); }
    }
}

// --- SETUP Y LOOP ---

void setup() {
    Serial.begin(115200); esp_task_wdt_init(10, true); esp_task_wdt_add(NULL);
    pinMode(PIN_RELE_1, OUTPUT); pinMode(PIN_RELE_2, OUTPUT); pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP); pinMode(PIN_TAMPER_SW, INPUT_PULLUP);
    
    configPrefs.begin("cfg", true);
    String s = configPrefs.getString("s", ""), p = configPrefs.getString("p", "");
    configPrefs.getString("m", "broker.smartaccess.com.ve").toCharArray(mqtt_server, 64);
    configPrefs.getString("i", "01").toCharArray(node_id, 32);
    configPrefs.end();

    if (s == "") { 
        WiFi.softAP("SmartAccess-Setup");
        server.on("/", [](){ server.send(200, "text/html", HTML_BODY); });
        server.on("/save", HTTP_POST, handleSave);
        server.begin(); currentMode = CONFIG_MODE; 
    } else {
        WiFi.begin(s.c_str(), p.c_str());
        configTime(-14400, 0, ntp_server);
        ArduinoOTA.begin();
        mqttClient.setServer(mqtt_server, 1883);
        mqttClient.setCallback([](char* t, byte* p, unsigned int l){ String m = ""; for(int i=0; i<l; i++) m += (char)p[i]; handleCommands(m); });
        currentMode = RUN_MODE;
    }
}

void loop() {
    esp_task_wdt_reset();
    if (currentMode == CONFIG_MODE) server.handleClient();
    else {
        ArduinoOTA.handle();
        if (WiFi.status() == WL_CONNECTED) {
            if (!mqttClient.connected() && (millis() - mqttRetryTime > 5000)) {
                mqttRetryTime = millis();
                if (mqttClient.connect(node_id)) { mqttClient.subscribe(topic_control); notify("SYS", "ONLINE"); currentUI = UI_READY; }
            }
            mqttClient.loop();
            if (millis() - lastHeartbeat > 30000) { lastHeartbeat = millis(); notify("HB", "OK"); }
            syncLogs();
        } else { currentUI = UI_CONNECTING; }
        handleHardware();
    }
    handleUI();
}
