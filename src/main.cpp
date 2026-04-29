/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v6.6 - Ultimate Edition
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "Hardware_Map_Lite_Vnzla.h"
#include "secrets.h"

// --- CONFIGURACIÓN ---
const char* node_id = "01";
const char* topic_control = "smartaccess/nodos/01/control";
const char* topic_events = "smartaccess/nodos/01/eventos";
#define WDT_TIMEOUT 8 

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
const int relay1Pulse = 1000;  // 1s (Cerradura)
const int relay2Pulse = 30000; // 30s (Luz de cortesía)

// --- ESTADOS ---
bool relay1Active = false;
bool relay2Active = false;
bool lastDoorState = HIGH;
bool lastTamperState = HIGH;
enum SystemState { CONNECTING, READY, GRANTED, DENIED, ALARM };
SystemState currentState = CONNECTING;

// --- UTILIDADES DE SISTEMA ---

String getUptime() {
    unsigned long sec = millis() / 1000;
    unsigned long min = sec / 60;
    unsigned long hr = min / 60;
    unsigned long days = hr / 24;
    char buffer[20];
    sprintf(buffer, "%d d, %02d:%02d:%02d", (int)days, (int)(hr % 24), (int)(min % 60), (int)(sec % 60));
    return String(buffer);
}

void notifyEvent(String event) {
    if (client.connected()) client.publish(topic_events, event.c_str());
    else {
        logStore.begin("offline_logs", false);
        int count = logStore.getInt("count", 0);
        logStore.putString(("l" + String(count)).c_str(), event);
        logStore.putInt("count", count + 1);
        logStore.end();
    }
}

// --- GESTIÓN DE COMANDOS ---

void handleCommands(String msg) {
    if (msg == "REBOOT") {
        notifyEvent("SYS:REBOOTING_REMOTE");
        delay(500);
        ESP.restart();
    }
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
    String message = "";
    for (int i = 0; i < length; i++) message += (char)payload[i];
    if (String(topic).endsWith("/control")) handleCommands(message);
}

// --- MONITOREO HARDWARE ---

void handlePeripherals() {
    unsigned long now = millis();
    // Gestión Relé 1 (Cerradura)
    if (relay1Active && (now - relay1StartTime > relay1Pulse)) {
        digitalWrite(PIN_RELE_1, RELAY_OFF);
        relay1Active = false;
    }
    // Gestión Relé 2 (Luz de Cortesía)
    if (relay2Active && (now - relay2StartTime > relay2Pulse)) {
        digitalWrite(PIN_RELE_2, RELAY_OFF);
        relay2Active = false;
        Serial.println("Luz de cortesía: OFF");
    }
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
                    digitalWrite(PIN_RELE_1, RELAY_ON);
                    digitalWrite(PIN_RELE_2, RELAY_ON); // Activar luz
                    relay1StartTime = millis(); relay2StartTime = millis();
                    relay1Active = true; relay2Active = true;
                    notifyEvent("ACCESS_GRANTED:" + String(keyStr));
                    Serial.println("Acceso Concedido. Luz activada.");
                } else {
                    currentState = DENIED;
                    notifyEvent("ACCESS_DENIED:" + String(keyStr));
                }
            }
            ds.reset_search();
        }
    }
}

// --- SETUP Y LOOP PRINCIPAL ---

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);
    pinMode(PIN_RELE_1, OUTPUT); pinMode(PIN_RELE_2, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT); pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP);
    pinMode(PIN_TAMPER_SW, INPUT_PULLUP);
    digitalWrite(PIN_RELE_1, RELAY_OFF); digitalWrite(PIN_RELE_2, RELAY_OFF);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(callback);
}

void loop() {
    esp_task_wdt_reset();
    if (WiFi.status() == WL_CONNECTED) {
        if (!client.connected() && (millis() - lastMqttRetry > 5000)) {
            lastMqttRetry = millis();
            if (client.connect("SmartAccess_Node_Lite_01", MQTT_USER, MQTT_PASS)) {
                client.subscribe(topic_control);
                notifyEvent("NODE_ONLINE:UPTIME=" + getUptime());
                currentState = READY;
            }
        }
        client.loop();
        if (client.connected() && (millis() - lastHeartbeat > heartbeatInterval)) {
            lastHeartbeat = millis();
            client.publish(topic_events, ("HEARTBEAT:UPTIME=" + getUptime()).c_str());
        }
    }
    handleIButton();
    handlePeripherals();
    // handleSensors() y handleLED() (Lógica ya integrada)
}
