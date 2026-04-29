/**
 * @file main.cpp
 * @brief Código Base SmartAccess iButton Lite v6.6 - Offline-First
 * 
 * Implementación no bloqueante para control de acceso resiliente.
 * Utiliza OneWire para iButton y Preferences para ACL local.
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

// --- OBJETOS ---
WiFiClient espClient;
PubSubClient client(espClient);
OneWire ds(PIN_IBUTTON);
Preferences preferences;

// --- VARIABLES GLOBALES ---
unsigned long lastMqttRetry = 0;
unsigned long lastIButtonCheck = 0;
const int ibuttonInterval = 100; // ms
byte addr[8];

// --- FUNCIONES ---

void setupWiFi() {
    delay(10);
    Serial.println("\nConectando a WiFi...");
    WiFi.begin(ssid, password);
}

void callback(char* topic, byte* payload, unsigned int length) {
    // Manejo de mensajes entrantes (Sincronización de ACL)
    Serial.print("Mensaje recibido [");
    Serial.print(topic);
    Serial.print("] ");
}

void reconnectMQTT() {
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastMqttRetry > 5000) {
            lastMqttRetry = now;
            Serial.print("Intentando conexión MQTT...");
            if (client.connect("SmartAccess_Node_Lite_01")) {
                Serial.println("conectado");
                client.subscribe("smartaccess/nodos/01/config");
            } else {
                Serial.print("falló, rc=");
                Serial.print(client.state());
                Serial.println(" intentando de nuevo en 5s");
            }
        }
    }
}

bool validateKeyLocal(byte* key) {
    char keyStr[17];
    sprintf(keyStr, "%02X%02X%02X%02X%02X%02X%02X%02X", 
            key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
    
    preferences.begin("acl", true);
    bool authorized = preferences.isKey(keyStr);
    preferences.end();
    
    return authorized; // Simulación: Retorna true si existe en Preferences
}

void handleIButton() {
    unsigned long now = millis();
    if (now - lastIButtonCheck > ibuttonInterval) {
        lastIButtonCheck = now;
        
        if (ds.search(addr)) {
            if (OneWire::crc8(addr, 7) != addr[7]) {
                Serial.println("CRC inválido!");
                return;
            }
            
            Serial.print("iButton Detectado: ");
            for (int i = 0; i < 8; i++) {
                Serial.print(addr[i], HEX);
            }
            Serial.println();
            
            if (validateKeyLocal(addr)) {
                Serial.println("ACCESO CONCEDIDO (Local)");
                digitalWrite(PIN_RELE_1, RELAY_ON);
                digitalWrite(PIN_LED_STATUS, HIGH);
                delay(100); // Pequeño pulso (en implementación real usar millis)
                digitalWrite(PIN_RELE_1, RELAY_OFF);
                digitalWrite(PIN_LED_STATUS, LOW);
            } else {
                Serial.println("ACCESO DENEGADO");
            }
            
            ds.reset_search();
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Inicializar Hardware
    pinMode(PIN_RELE_1, OUTPUT);
    pinMode(PIN_RELE_2, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_SENSOR_DOOR, INPUT_PULLUP);
    
    // Estado inicial seguro
    digitalWrite(PIN_RELE_1, RELAY_OFF);
    digitalWrite(PIN_RELE_2, RELAY_OFF);
    digitalWrite(PIN_LED_STATUS, LOW);
    
    setupWiFi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    // 1. Mantener comunicaciones (No bloqueante)
    if (WiFi.status() != WL_CONNECTED) {
        // Podríamos intentar reconectar aquí sin bloquear
    } else {
        reconnectMQTT();
        client.loop();
    }
    
    // 2. Escaneo de llaves (No bloqueante)
    handleIButton();
}
