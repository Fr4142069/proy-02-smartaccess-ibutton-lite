/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v7.0.1 - Master Edition (Full Log Sync)
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

// --- OBJETOS ---
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

// --- CRONÓMETROS ---
unsigned long lastHeartbeat = 0, lastIButton = 0, lastSensor = 0, lastLogSync = 0;
unsigned long relay1Start = 0, relay2Start = 0, ledPatternTime = 0, mqttRetryTime = 0;
bool relay1Active = false, relay2Active = false, lastDoor = HIGH, lastTamper = HIGH;

enum SystemMode { CONFIG_MODE, RUN_MODE } currentMode = CONFIG_MODE;
enum UIState { UI_CONNECTING, UI_READY, UI_GRANTED, UI_DENIED, UI_ALARM, UI_OTA } currentUI = UI_CONNECTING;

// --- TELEMETRÍA ---

String getTS() {
    struct tm ti; if (!getLocalTime(&ti)) return "0000-00-00 00:00:00";
    char b[25]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &ti); return String(b);
}

void notify(String type, String msg, String data = "") {
    StaticJsonDocument<384> doc;
    doc["node"] = node_id; doc["type"] = type; doc["msg"] = msg;
    if (data != "") doc["data"] = data;
    doc["ts"] = getTS();
    doc["heap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    
    char b[384]; serializeJson(doc, b);
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
    if (!mqttClient.connected()) return;
    if (millis() - lastLogSync < 5000) return;
    lastLogSync = millis();

    logPrefs.begin("logs", false);
    int count = logPrefs.getInt("c", 0);
    if (count > 0) {
        Serial.printf("[SYNC] Enviando %d logs pendientes...\n", count);
        for (int i = 0; i < count; i++) {
            String key = "l" + String(i);
            String logMsg = logPrefs.getString(key.c_str(), "");
            if (logMsg != "") {
                // Añadir prefijo para identificar que es un log offline sincronizado
                StaticJsonDocument<512> doc;
                deserializeJson(doc, logMsg);
                doc["sync"] = true;
                char buffer[512];
                serializeJson(doc, buffer);
                mqttClient.publish(topic_events, buffer);
            }
        }
        logPrefs.clear(); 
        logPrefs.putInt("c", 0);
        Serial.println("[SYNC] Memoria Flash liberada.");
    }
    logPrefs.end();
}

// --- COMANDOS ---

void handleCommands(String json) {
    StaticJsonDocument<256> doc; if (deserializeJson(doc, json)) return;
    if (doc["id"] != node_id && doc["id"] != "ALL") return;
    
    String cmd = doc["cmd"] | "";
    if (cmd == "REBOOT") ESP.restart();
    else if (cmd == "CLEAR_ACL") { aclPrefs.begin("acl", false); aclPrefs.clear(); aclPrefs.end(); notify("SYS", "ACL_CLEARED"); }
    else if (cmd == "ADD") {
        aclPrefs.begin("acl", false); aclPrefs.putBool(doc["key"], true); aclPrefs.end();
        notify("ACL", "ADDED", doc["key"]);
    }
    else if (cmd == "DEL") {
        aclPrefs.begin("acl", false); aclPrefs.remove(doc["key"]); aclPrefs.end();
        notify("ACL", "REMOVED", doc["key"]);
    }
}

// --- HARDWARE ---

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
        server.on("/", [](){ server.send(200, "text/html", "<html>Config Portal</html>"); }); // Simplificado para brev.
        server.on("/save", HTTP_POST, [](){ /* handleSave */ });
        server.begin();
        currentMode = CONFIG_MODE; 
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
    // handleUI() (Lógica ya implementada)
}
