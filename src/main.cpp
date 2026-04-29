/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v6.6 - Cloud-Ready (JSON Edition)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <ArduinoJson.h>
#include "Hardware_Map_Lite_Vnzla.h"
#include "secrets.h"

// --- CONFIGURACIÓN ---
const char* node_id = "smart-node-01";
const char* topic_control = "smartaccess/nodos/01/control";
const char* topic_events = "smartaccess/nodos/01/eventos";
const char* ntpServer = "pool.ntp.org";
#define WDT_TIMEOUT 10 

// --- OBJETOS ---
WiFiClient espClient;
PubSubClient client(espClient);
OneWire ds(PIN_IBUTTON);
Preferences preferences;
Preferences logStore;

// --- CRONÓMETROS ---
unsigned long lastMqttRetry = 0;
unsigned long lastIButtonCheck = 0;
unsigned long lastHeartbeat = 0;
unsigned long relay1StartTime = 0;
unsigned long relay2StartTime = 0;
unsigned long ledPatternTime = 0;

// --- ESTADOS ---
bool relay1Active = false;
bool relay2Active = false;
enum SystemState { CONNECTING, READY, GRANTED, DENIED, ALARM, UPDATING };
SystemState currentState = CONNECTING;

// --- COMUNICACIÓN JSON ---

void publishStatus(String type, String message, String data = "") {
    StaticJsonDocument<256> doc;
    doc["node"] = node_id;
    doc["type"] = type;
    doc["msg"] = message;
    if (data != "") doc["data"] = data;
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buff[25]; strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", &timeinfo);
        doc["ts"] = buff;
    }
    
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(topic_events, buffer);
}

void sendHeartbeat() {
    StaticJsonDocument<512> doc;
    doc["node"] = node_id;
    doc["type"] = "HEARTBEAT";
    doc["uptime"] = millis() / 1000;
    doc["heap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    doc["state"] = (int)currentState;
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buff[25]; strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", &timeinfo);
        doc["ts"] = buff;
    }
    
    char buffer[512];
    serializeJson(doc, buffer);
    client.publish(topic_events, buffer);
}

// --- GESTIÓN DE COMANDOS JSON ---

void handleCommands(String msg) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (error) return;

    String cmd = doc["cmd"] | "";
    if (cmd == "REBOOT") { publishStatus("SYS", "REBOOTING"); delay(1000); ESP.restart(); }
    else if (cmd == "RELAY") {
        int id = doc["id"] | 1;
        int state = doc["state"] | 0;
        if (id == 1) digitalWrite(PIN_RELE_1, state);
        else digitalWrite(PIN_RELE_2, state);
        publishStatus("ACTUATOR", "RELAY_CMD_OK");
    }
    else if (cmd == "ACL_ADD") {
        String key = doc["key"] | "";
        if (key != "") {
            preferences.begin("acl", false); preferences.putBool(key.c_str(), true); preferences.end();
            publishStatus("ACL", "KEY_ADDED", key);
        }
    }
}

// --- LÓGICA DE CONTROL ---

void callback(char* topic, byte* payload, unsigned int length) {
    String msg = ""; for (int i = 0; i < length; i++) msg += (char)payload[i];
    handleCommands(msg);
}

void handleIButton() {
    if (millis() - lastIButtonCheck > 150) {
        lastIButtonCheck = millis();
        byte addr[8];
        if (ds.search(addr)) {
            if (OneWire::crc8(addr, 7) == addr[7]) {
                char keyStr[17]; sprintf(keyStr, "%02X%02X%02X%02X%02X%02X%02X%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
                preferences.begin("acl", true); bool auth = preferences.isKey(keyStr); preferences.end();
                if (auth) {
                    currentState = GRANTED; ledPatternTime = millis();
                    digitalWrite(PIN_RELE_1, HIGH); digitalWrite(PIN_RELE_2, HIGH);
                    relay1StartTime = relay2StartTime = millis(); relay1Active = relay2Active = true;
                    publishStatus("ACCESS", "GRANTED", keyStr);
                } else {
                    currentState = DENIED; ledPatternTime = millis();
                    publishStatus("ACCESS", "DENIED", keyStr);
                }
            }
            ds.reset_search();
        }
    }
}

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(WDT_TIMEOUT, true); esp_task_wdt_add(NULL);
    pinMode(PIN_RELE_1, OUTPUT); pinMode(PIN_RELE_2, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT); pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP);
    pinMode(PIN_TAMPER_SW, INPUT_PULLUP);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    configTime(-14400, 0, ntpServer);
    ArduinoOTA.begin();
    client.setServer(MQTT_SERVER, 1883); client.setCallback(callback);
}

void loop() {
    esp_task_wdt_reset(); ArduinoOTA.handle();
    if (WiFi.status() == WL_CONNECTED) {
        if (!client.connected() && (millis() - lastMqttRetry > 5000)) {
            lastMqttRetry = millis();
            if (client.connect(node_id, MQTT_USER, MQTT_PASS)) {
                client.subscribe(topic_control);
                publishStatus("SYS", "ONLINE");
                currentState = READY;
            }
        }
        client.loop();
        if (client.connected() && (millis() - lastHeartbeat > heartbeatInterval)) {
            lastHeartbeat = millis(); sendHeartbeat();
        }
    }
    handleIButton();
    if (relay1Active && (millis() - relay1StartTime > 1000)) { digitalWrite(PIN_RELE_1, LOW); relay1Active = false; }
    if (relay2Active && (millis() - relay2StartTime > 30000)) { digitalWrite(PIN_RELE_2, LOW); relay2Active = false; }
    
    // handleLED() (Lógica simplificada para brevedad)
    if (currentState == GRANTED) digitalWrite(PIN_LED_STATUS, HIGH);
    else if (currentState == READY) digitalWrite(PIN_LED_STATUS, LOW);
}
