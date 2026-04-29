/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v6.6 - Audit Edition (NTP + Timestamps)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "Hardware_Map_Lite_Vnzla.h"
#include "secrets.h"

// --- CONFIGURACIÓN ---
const char* node_id = "01";
const char* topic_control = "smartaccess/nodos/01/control";
const char* topic_events = "smartaccess/nodos/01/eventos";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -14400; // VZLA (UTC-4)
const int   daylightOffset_sec = 0;

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
unsigned long lastSensorCheck = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastLogSync = 0;
unsigned long relay1StartTime = 0;
unsigned long relay2StartTime = 0;
unsigned long ledPatternTime = 0;

// --- CONFIGURACIÓN DE TIEMPOS ---
const int ibuttonInterval = 150; 
const int sensorInterval = 1000;
const int heartbeatInterval = 30000;
const int logSyncInterval = 5000;
const int relay1Pulse = 1000;
const int relay2Pulse = 30000;

// --- ESTADOS ---
bool relay1Active = false;
bool relay2Active = false;
bool lastDoorState = HIGH;
bool lastTamperState = HIGH;
enum SystemState { CONNECTING, READY, GRANTED, DENIED, ALARM, UPDATING };
SystemState currentState = CONNECTING;

// --- GESTIÓN DE TIEMPO (NTP) ---

String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "NO_TIME";
    char timeStringBuff[25];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStringBuff);
}

void setupNTP() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

// --- GESTIÓN DE LOGS Y EVENTOS ---

void notifyEvent(String event) {
    String timestampedEvent = "[" + getTimestamp() + "] " + event;
    if (client.connected()) {
        client.publish(topic_events, timestampedEvent.c_str());
    } else {
        logStore.begin("offline_logs", false);
        int count = logStore.getInt("count", 0);
        logStore.putString(("l" + String(count)).c_str(), timestampedEvent);
        logStore.putInt("count", count + 1);
        logStore.end();
        Serial.println("Offline Log: " + timestampedEvent);
    }
}

void syncLogs() {
    if (!client.connected() || (millis() - lastLogSync < logSyncInterval)) return;
    lastLogSync = millis();
    logStore.begin("offline_logs", false);
    int count = logStore.getInt("count", 0);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            String log = logStore.getString(("l" + String(i)).c_str(), "");
            if (log != "") client.publish(topic_events, (String("SYNCED:") + log).c_str());
        }
        logStore.clear(); logStore.putInt("count", 0);
    }
    logStore.end();
}

// --- COMANDOS Y MANTENIMIENTO ---

void handleCommands(String msg) {
    if (msg == "GET_TIME") notifyEvent("CURRENT_TIME=" + getTimestamp());
    else if (msg == "REBOOT") { notifyEvent("SYS:REBOOT"); delay(1000); ESP.restart(); }
    else if (msg == "CLEAR_ACL") { preferences.begin("acl", false); preferences.clear(); preferences.end(); notifyEvent("SYS:ACL_CLEARED"); }
    else if (msg.startsWith("ADD:") || msg.startsWith("DEL:")) {
        int sep = msg.indexOf(':');
        String act = msg.substring(0, sep);
        String key = msg.substring(sep + 1);
        key.toUpperCase(); key.trim();
        preferences.begin("acl", false);
        if (act == "ADD") preferences.putBool(key.c_str(), true);
        else if (act == "DEL") preferences.remove(key.c_str());
        preferences.end();
        notifyEvent("SYNC_OK:" + act + ":" + key);
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    if (String(topic).endsWith("/control")) handleCommands(msg);
}

// --- HARDWARE ---

void handleLED() {
    unsigned long now = millis();
    static bool state = false;
    int interval = 0;
    if (currentState == UPDATING || currentState == ALARM) interval = 50;
    else if (currentState == CONNECTING) interval = 500;
    else if (currentState == DENIED) interval = 100;
    
    if (interval > 0 && (now - ledPatternTime > interval)) {
        ledPatternTime = now; state = !state; digitalWrite(PIN_LED_STATUS, state);
    }
    if (currentState == GRANTED) digitalWrite(PIN_LED_STATUS, HIGH);
    else if (currentState == READY) digitalWrite(PIN_LED_STATUS, LOW);
}

void handleIButton() {
    if (millis() - lastIButtonCheck > ibuttonInterval) {
        lastIButtonCheck = millis();
        byte addr[8];
        if (ds.search(addr)) {
            if (OneWire::crc8(addr, 7) == addr[7]) {
                char keyStr[17];
                sprintf(keyStr, "%02X%02X%02X%02X%02X%02X%02X%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
                preferences.begin("acl", true);
                bool auth = preferences.isKey(keyStr);
                preferences.end();
                ledPatternTime = millis();
                if (auth) {
                    currentState = GRANTED;
                    digitalWrite(PIN_RELE_1, RELAY_ON); digitalWrite(PIN_RELE_2, RELAY_ON);
                    relay1StartTime = millis(); relay2StartTime = millis();
                    relay1Active = true; relay2Active = true;
                    notifyEvent("ACCESS_GRANTED:" + String(keyStr));
                } else {
                    currentState = DENIED;
                    notifyEvent("ACCESS_DENIED:" + String(keyStr));
                }
            }
            ds.reset_search();
        }
    }
}

// --- SETUP / LOOP ---

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(WDT_TIMEOUT, true); esp_task_wdt_add(NULL);
    pinMode(PIN_RELE_1, OUTPUT); pinMode(PIN_RELE_2, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT); pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP);
    pinMode(PIN_TAMPER_SW, INPUT_PULLUP);
    digitalWrite(PIN_RELE_1, RELAY_OFF); digitalWrite(PIN_RELE_2, RELAY_OFF);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    setupNTP();
    ArduinoOTA.begin();
    client.setServer(MQTT_SERVER, 1883); client.setCallback(callback);
}

void loop() {
    esp_task_wdt_reset();
    ArduinoOTA.handle();
    if (WiFi.status() == WL_CONNECTED) {
        if (!client.connected() && (millis() - lastMqttRetry > 5000)) {
            lastMqttRetry = millis();
            if (client.connect("SmartAccess_Node_Lite_01", MQTT_USER, MQTT_PASS)) {
                client.subscribe(topic_control);
                notifyEvent("NODE_ONLINE");
                currentState = READY;
            }
        }
        client.loop();
        if (client.connected() && (millis() - lastHeartbeat > heartbeatInterval)) {
            lastHeartbeat = millis();
            client.publish(topic_events, ("HEARTBEAT:TIME=" + getTimestamp()).c_str());
        }
        syncLogs();
    }
    handleIButton();
    handleLED();
    
    // Gestión relés (No bloqueante)
    if (relay1Active && (millis() - relay1StartTime > relay1Pulse)) { digitalWrite(PIN_RELE_1, RELAY_OFF); relay1Active = false; }
    if (relay2Active && (millis() - relay2StartTime > relay2Pulse)) { digitalWrite(PIN_RELE_2, RELAY_OFF); relay2Active = false; }
}
