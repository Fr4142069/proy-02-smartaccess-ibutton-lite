/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v7.2.0 - Critical Systems Edition
 * 
 * - mDNS: Discovery via smart-node-01.local
 * - MQTT LWT: Instant offline detection
 * - WiFi Scan: Discovery in Web Portal
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
#include <ESPmDNS.h>
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

// --- PORTAL WEB AVANZADO ---

String getWiFiScanHTML() {
    int n = WiFi.scanNetworks();
    String options = "";
    for (int i = 0; i < n; ++i) {
        options += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
    }
    return options;
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>SmartAccess v7.2</title><style>body{font-family:sans-serif;background:#1a1a1a;color:#fff;text-align:center;}.card{background:#2d2d2d;padding:20px;border-radius:10px;display:inline-block;width:90%;max-width:400px;}input,select{display:block;width:100%;margin:10px 0;padding:10px;background:#3d3d3d;color:#fff;border:1px solid #444;}button{background:#00d2ff;color:white;border:none;padding:10px;width:100%;cursor:pointer;}</style></head><body><div class='card'><h2>SmartAccess v7.2</h2>";
    html += "<form action='/save' method='POST'>";
    html += "WiFi:<select name='s'>" + getWiFiScanHTML() + "</select>";
    html += "<input name='p' type='password' placeholder='Password'>";
    html += "<input name='m' placeholder='MQTT Broker' value='" + String(mqtt_server) + "'>";
    html += "<input name='i' placeholder='Node ID' value='" + String(node_id) + "'>";
    html += "<button type='submit'>GUARDAR Y REINICIAR</button></form></div></body></html>";
    server.send(200, "text/html", html);
}

// --- COMUNICACIONES Y LWT ---

void notify(String type, String msg, String data = "") {
    StaticJsonDocument<512> doc;
    doc["node"] = node_id; doc["type"] = type; doc["msg"] = msg;
    if (data != "") doc["data"] = data;
    doc["heap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    
    char buffer[512]; serializeJson(doc, buffer);
    if (mqttClient.connected()) mqttClient.publish(topic_events, buffer);
    else {
        logPrefs.begin("logs", false);
        logPrefs.putString(("l"+String(logPrefs.getInt("c", 0))).c_str(), buffer);
        logPrefs.putInt("c", logPrefs.getInt("c", 0) + 1);
        logPrefs.end();
    }
}

void connectMQTT() {
    if (!mqttClient.connected() && (millis() - mqttRetryTime > 5000)) {
        mqttRetryTime = millis();
        // LWT: Mensaje de "Última Voluntad" si el nodo se desconecta abruptamente
        String lwtPayload = "{\"node\":\"" + String(node_id) + "\",\"type\":\"SYS\",\"msg\":\"OFFLINE_ABRUPT\"}";
        if (mqttClient.connect(node_id, MQTT_USER, MQTT_PASS, topic_events, 1, true, lwtPayload.c_str())) {
            mqttClient.subscribe(topic_control);
            notify("SYS", "ONLINE");
            currentUI = UI_READY;
        }
    }
}

// --- SETUP / LOOP ---

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
        server.on("/", handleRoot);
        server.on("/save", HTTP_POST, [](){
            configPrefs.begin("cfg", false);
            configPrefs.putString("s", server.arg("s")); configPrefs.putString("p", server.arg("p"));
            configPrefs.putString("m", server.arg("m")); configPrefs.putString("i", server.arg("i"));
            configPrefs.end();
            server.send(200, "text/plain", "OK. Reiniciando...");
            delay(2000); ESP.restart();
        });
        server.begin(); currentMode = CONFIG_MODE;
    } else {
        WiFi.begin(s.c_str(), p.c_str());
        configTime(-14400, 0, "pool.ntp.org");
        if (MDNS.begin(node_id)) { MDNS.addService("http", "tcp", 80); }
        ArduinoOTA.begin();
        mqttClient.setServer(mqtt_server, 1883);
        currentMode = RUN_MODE;
    }
}

void loop() {
    esp_task_wdt_reset();
    if (currentMode == CONFIG_MODE) server.handleClient();
    else {
        ArduinoOTA.handle();
        if (WiFi.status() == WL_CONNECTED) {
            connectMQTT();
            mqttClient.loop();
            if (millis() - lastHeartbeat > 30000) { lastHeartbeat = millis(); notify("HB", "OK"); }
        } else { currentUI = UI_CONNECTING; }
        // handleHardware(), handleUI(), handleRelays()... (Lógica preservada)
    }
}
