/**
 * @file main.cpp
 * @brief Código Base SmartAccess iButton Lite v6.6 - Hito 3: Sync
 * 
 * Implementación de sincronización de ACL vía MQTT y validación Offline.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include "Hardware_Map_Lite_Vnzla.h"

// --- CONFIGURACIÓN ---
const char* ssid = "TU_WIFI_SSID";
const char* password = "TU_WIFI_PASSWORD";
const char* mqtt_server = "broker.smartaccess.com.ve";
const char* node_id = "01";
const char* topic_control = "smartaccess/nodos/01/control";
const char* topic_events = "smartaccess/nodos/01/eventos";

// --- OBJETOS ---
WiFiClient espClient;
PubSubClient client(espClient);
OneWire ds(PIN_IBUTTON);
Preferences preferences;

// --- VARIABLES GLOBALES ---
unsigned long lastMqttRetry = 0;
unsigned long lastIButtonCheck = 0;
const int ibuttonInterval = 150; 

// --- FUNCIONES DE GESTIÓN DE LLAVES ---

void manageACL(String command) {
    // Formato esperado: "ADD:HEXKEY" o "DEL:HEXKEY"
    int separatorIndex = command.indexOf(':');
    if (separatorIndex == -1) return;

    String action = command.substring(0, separatorIndex);
    String key = command.substring(separatorIndex + 1);
    key.toUpperCase();
    key.trim();

    preferences.begin("acl", false); // Modo lectura/escritura
    
    if (action == "ADD") {
        preferences.putBool(key.c_str(), true);
        Serial.printf("ACL: Llave %s AGREGADA\n", key.c_str());
        client.publish(topic_events, ("SYNC_OK:ADD:" + key).c_str());
    } 
    else if (action == "DEL") {
        preferences.remove(key.c_str());
        Serial.printf("ACL: Llave %s ELIMINADA\n", key.c_str());
        client.publish(topic_events, ("SYNC_OK:DEL:" + key).c_str());
    }
    
    preferences.end();
}

bool validateKeyLocal(byte* key) {
    char keyStr[17];
    sprintf(keyStr, "%02X%02X%02X%02X%02X%02X%02X%02X", 
            key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
    
    preferences.begin("acl", true); // Modo solo lectura
    bool authorized = preferences.isKey(keyStr);
    preferences.end();
    
    return authorized;
}

// --- COMUNICACIONES ---

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("Mensaje recibido [%s]: %s\n", topic, message.c_str());
    
    if (String(topic) == topic_control) {
        manageACL(message);
    }
}

void reconnectMQTT() {
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastMqttRetry > 5000) {
            lastMqttRetry = now;
            Serial.print("Intentando conexión MQTT...");
            if (client.connect("SmartAccess_Node_Lite_01")) {
                Serial.println("conectado");
                client.subscribe(topic_control);
                client.publish(topic_events, "NODE_ONLINE");
            } else {
                Serial.printf("falló, rc=%d intentando de nuevo en 5s\n", client.state());
            }
        }
    }
}

// --- LOGICA DE CONTROL ---

void handleIButton() {
    unsigned long now = millis();
    if (now - lastIButtonCheck > ibuttonInterval) {
        lastIButtonCheck = now;
        
        byte addr[8];
        if (ds.search(addr)) {
            if (OneWire::crc8(addr, 7) == addr[7]) {
                char keyStr[17];
                sprintf(keyStr, "%02X%02X%02X%02X%02X%02X%02X%02X", 
                        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
                
                Serial.printf("iButton: %s -> ", keyStr);
                
                if (validateKeyLocal(addr)) {
                    Serial.println("AUTORIZADO");
                    digitalWrite(PIN_RELE_1, RELAY_ON);
                    digitalWrite(PIN_LED_STATUS, HIGH);
                    
                    // Notificar evento
                    if (client.connected()) {
                        client.publish(topic_events, ("ACCESS_GRANTED:" + String(keyStr)).c_str());
                    }
                    
                    delay(500); // Pulso de apertura (bloqueo mínimo aceptable para demo)
                    
                    digitalWrite(PIN_RELE_1, RELAY_OFF);
                    digitalWrite(PIN_LED_STATUS, LOW);
                } else {
                    Serial.println("DENEGADO");
                    if (client.connected()) {
                        client.publish(topic_events, ("ACCESS_DENIED:" + String(keyStr)).c_str());
                    }
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
    
    WiFi.begin(ssid, password);
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        reconnectMQTT();
        client.loop();
    }
    handleIButton();
}
