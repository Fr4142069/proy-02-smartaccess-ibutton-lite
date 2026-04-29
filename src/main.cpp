/**
 * @file main.cpp
 * @brief SmartAccess iButton Lite v6.6 - 100% Non-Blocking & Heartbeat
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include "Hardware_Map_Lite_Vnzla.h"
#include "secrets.h"

// --- CONFIGURACIÓN ---
const char* node_id = "01";
const char* topic_control = "smartaccess/nodos/01/control";
const char* topic_events = "smartaccess/nodos/01/eventos";

// --- OBJETOS ---
WiFiClient espClient;
PubSubClient client(espClient);
OneWire ds(PIN_IBUTTON);
Preferences preferences;

// --- CRONÓMETROS (millis) ---
unsigned long lastMqttRetry = 0;
unsigned long lastIButtonCheck = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastHeartbeat = 0;
unsigned long relay1StartTime = 0;

// --- CONFIGURACIÓN DE TIEMPOS ---
const int ibuttonInterval = 150; 
const int sensorInterval = 1000;
const int heartbeatInterval = 30000; // 30s
const int relayPulseDuration = 1000;  // 1s

// --- ESTADOS ---
bool relay1Active = false;
bool lastDoorState = HIGH;

// --- GESTIÓN DE ACL ---
void manageACL(String command) {
    int separatorIndex = command.indexOf(':');
    if (separatorIndex == -1) return;
    String action = command.substring(0, separatorIndex);
    String key = command.substring(separatorIndex + 1);
    key.toUpperCase();
    key.trim();
    preferences.begin("acl", false);
    if (action == "ADD") preferences.putBool(key.c_str(), true);
    else if (action == "DEL") preferences.remove(key.c_str());
    preferences.end();
}

// --- COMUNICACIONES ---
void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (int i = 0; i < length; i++) message += (char)payload[i];
    if (String(topic).endsWith("/control")) manageACL(message);
}

void reconnectMQTT() {
    if (!client.connected() && (millis() - lastMqttRetry > 5000)) {
        lastMqttRetry = millis();
        if (client.connect("SmartAccess_Node_Lite_01", MQTT_USER, MQTT_PASS)) {
            client.subscribe(topic_control);
            client.publish(topic_events, "NODE_ONLINE");
        }
    }
}

void sendHeartbeat() {
    if (client.connected() && (millis() - lastHeartbeat > heartbeatInterval)) {
        lastHeartbeat = millis();
        client.publish(topic_events, "HEARTBEAT:OK");
    }
}

// --- MONITOREO DE HARDWARE ---

void handleRelays() {
    if (relay1Active && (millis() - relay1StartTime > relayPulseDuration)) {
        digitalWrite(PIN_RELE_1, RELAY_OFF);
        digitalWrite(PIN_LED_STATUS, LOW);
        relay1Active = false;
        Serial.println("Relé 1: Cerrado (Fin de pulso)");
    }
}

void handleSensors() {
    if (millis() - lastSensorCheck > sensorInterval) {
        lastSensorCheck = millis();
        bool currentDoorState = digitalRead(PIN_SENSOR_DOOR);
        if (currentDoorState != lastDoorState) {
            lastDoorState = currentDoorState;
            String status = (currentDoorState == LOW) ? "DOOR_CLOSED" : "DOOR_OPENED";
            if (client.connected()) client.publish(topic_events, status.c_str());
            Serial.println("Sensor: " + status);
        }
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
                
                if (auth) {
                    Serial.printf("Acceso Concedido: %s\n", keyStr);
                    digitalWrite(PIN_RELE_1, RELAY_ON);
                    digitalWrite(PIN_LED_STATUS, HIGH);
                    relay1StartTime = millis();
                    relay1Active = true;
                    if (client.connected()) client.publish(topic_events, (String("ACCESS_GRANTED:") + keyStr).c_str());
                } else {
                    Serial.printf("Acceso Denegado: %s\n", keyStr);
                    if (client.connected()) client.publish(topic_events, (String("ACCESS_DENIED:") + keyStr).c_str());
                }
            }
            ds.reset_search();
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_RELE_1, OUTPUT);
    pinMode(PIN_RELE_2, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP);
    
    digitalWrite(PIN_RELE_1, RELAY_OFF);
    digitalWrite(PIN_RELE_2, RELAY_OFF);
    digitalWrite(PIN_LED_STATUS, LOW);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(callback);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        reconnectMQTT();
        client.loop();
        sendHeartbeat();
    }
    handleIButton();
    handleSensors();
    handleRelays(); // Gestión no bloqueante del pulso del relé
}
