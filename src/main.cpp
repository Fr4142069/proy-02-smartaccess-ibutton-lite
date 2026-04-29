/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v6.6 - Fleet Edition (OTA + Maint)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>
#include "Hardware_Map_Lite_Vnzla.h"
#include "secrets.h"

// --- CONFIGURACIÓN ---
const char* node_id = "01";
const char* topic_control = "smartaccess/nodos/01/control";
const char* topic_events = "smartaccess/nodos/01/eventos";
#define WDT_TIMEOUT 10 // Aumentado ligeramente para OTA

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

// --- GESTIÓN OTA (Over-The-Air) ---

void setupOTA() {
    ArduinoOTA.setHostname("SmartAccess-Node-01");
    ArduinoOTA.setPassword("admin-smart-lite"); // Seguridad para updates

    ArduinoOTA.onStart([]() {
        currentState = UPDATING;
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("Iniciando actualización OTA: " + type);
    });
    
    ArduinoOTA.onEnd([]() { Serial.println("\nActualización completada."); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error [%u]: ", error);
        currentState = READY;
    });
    
    ArduinoOTA.begin();
}

// --- UTILIDADES DE SISTEMA ---

String getUptime() {
    unsigned long sec = millis() / 1000;
    unsigned long min = sec / 60;
    char buffer[32];
    sprintf(buffer, "%d d, %02d:%02d:%02d", (int)(sec/86400), (int)((sec%86400)/3600), (int)(min % 60), (int)(sec % 60));
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
        delay(1000); ESP.restart();
    }
    else if (msg == "CLEAR_ACL") {
        preferences.begin("acl", false);
        preferences.clear();
        preferences.end();
        notifyEvent("SYS:ACL_CLEARED");
    }
    else if (msg == "GET_INFO") {
        String info = "INFO:UPTIME=" + getUptime() + "|IP=" + WiFi.localIP().toString();
        notifyEvent(info);
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

void handleLED() {
    unsigned long now = millis();
    static bool ledState = false;
    int interval = 0;
    
    if (currentState == UPDATING) interval = 50; // Parpadeo súper rápido
    else if (currentState == CONNECTING) interval = 500;
    else if (currentState == DENIED) interval = 100;
    else if (currentState == ALARM) interval = 50;
    
    if (interval > 0 && (now - ledPatternTime > interval)) {
        ledPatternTime = now;
        ledState = !ledState;
        digitalWrite(PIN_LED_STATUS, ledState);
        if (currentState == GRANTED && (now - ledPatternTime > 1000)) currentState = READY;
    }
    if (currentState == GRANTED) digitalWrite(PIN_LED_STATUS, HIGH);
}

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);
    pinMode(PIN_RELE_1, OUTPUT); pinMode(PIN_RELE_2, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT); pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP);
    pinMode(PIN_TAMPER_SW, INPUT_PULLUP);
    digitalWrite(PIN_RELE_1, RELAY_OFF); digitalWrite(PIN_RELE_2, RELAY_OFF);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    setupOTA();
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(callback);
}

void loop() {
    esp_task_wdt_reset();
    ArduinoOTA.handle();
    
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
    
    if (currentState != UPDATING) {
        // Ejecutar lógica solo si no estamos actualizando
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
                    if (auth) {
                        currentState = GRANTED; ledPatternTime = millis();
                        digitalWrite(PIN_RELE_1, RELAY_ON); digitalWrite(PIN_RELE_2, RELAY_ON);
                        relay1StartTime = millis(); relay2StartTime = millis();
                        relay1Active = true; relay2Active = true;
                        notifyEvent("ACCESS_GRANTED:" + String(keyStr));
                    } else {
                        currentState = DENIED; ledPatternTime = millis();
                        notifyEvent("ACCESS_DENIED:" + String(keyStr));
                    }
                }
                ds.reset_search();
            }
        }
        
        // Gestión de Relés No Bloqueante
        unsigned long now = millis();
        if (relay1Active && (now - relay1StartTime > relay1Pulse)) { digitalWrite(PIN_RELE_1, RELAY_OFF); relay1Active = false; }
        if (relay2Active && (now - relay2StartTime > relay2Pulse)) { digitalWrite(PIN_RELE_2, RELAY_OFF); relay2Active = false; }
    }
    handleLED();
}
