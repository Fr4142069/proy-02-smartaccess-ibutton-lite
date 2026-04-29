/**
 * @file main.cpp
 * @brief Código Base SmartAccess iButton Lite v6.6 - Hito 3.1: Security & Sensors
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include "Hardware_Map_Lite_Vnzla.h"
#include "secrets.h"

// --- OBJETOS ---
WiFiClient espClient;
PubSubClient client(espClient);
OneWire ds(PIN_IBUTTON);
Preferences preferences;

// --- VARIABLES GLOBALES ---
unsigned long lastMqttRetry = 0;
unsigned long lastIButtonCheck = 0;
unsigned long lastSensorCheck = 0;
const int ibuttonInterval = 150; 
const int sensorInterval = 1000;
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
            client.subscribe("smartaccess/nodos/01/control");
            client.publish("smartaccess/nodos/01/eventos", "NODE_ONLINE");
        }
    }
}

// --- MONITOREO DE SENSORES ---
void handleSensors() {
    if (millis() - lastSensorCheck > sensorInterval) {
        lastSensorCheck = millis();
        bool currentDoorState = digitalRead(PIN_SENSOR_DOOR);
        if (currentDoorState != lastDoorState) {
            lastDoorState = currentDoorState;
            String status = (currentDoorState == LOW) ? "DOOR_CLOSED" : "DOOR_OPENED";
            if (client.connected()) {
                client.publish("smartaccess/nodos/01/eventos", status.c_str());
            }
            Serial.println("Sensor: " + status);
        }
    }
}

// --- CONTROL DE ACCESO ---
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
                    digitalWrite(PIN_RELE_1, RELAY_ON);
                    digitalWrite(PIN_LED_STATUS, HIGH);
                    if (client.connected()) client.publish("smartaccess/nodos/01/eventos", (String("ACCESS_GRANTED:") + keyStr).c_str());
                    delay(500); // Apertura
                    digitalWrite(PIN_RELE_1, RELAY_OFF);
                    digitalWrite(PIN_LED_STATUS, LOW);
                } else {
                    if (client.connected()) client.publish("smartaccess/nodos/01/eventos", (String("ACCESS_DENIED:") + keyStr).c_str());
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
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(callback);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        reconnectMQTT();
        client.loop();
    }
    handleIButton();
    handleSensors();
}
