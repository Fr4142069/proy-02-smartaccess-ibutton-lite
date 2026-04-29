/**
 * @file Hardware_Map_Lite_Vnzla.h
 * @author Francisco (SmartAccess)
 * @brief Mapeo de Hardware para Nodo de Borde iButton Lite v6.6
 * @version 6.6
 * @date 2026-04-29
 */

#ifndef HARDWARE_MAP_LITE_VNZLA_H
#define HARDWARE_MAP_LITE_VNZLA_H

#include <Arduino.h>

// --- CONFIGURACIÓN DE PINES (ESP32) ---

// Lector iButton (Dallas Key)
#define PIN_IBUTTON      4   // OneWire Bus (Requiere Pull-up de 4.7k)

// Salidas de Control (Relés)
#define PIN_RELE_1       12  // Cerradura Peatonal
#define PIN_RELE_2       13  // Portón / Luz de Cortesía

// Sensores
#define PIN_SENSOR_DOOR  14  // Sensor Reed (Puerta Abierta/Cerrada)

// Interfaz Visual
#define PIN_LED_STATUS   2   // LED de estado del sistema (Onboard)

// --- MACROS DE CONTROL ---
#define RELAY_ON         HIGH
#define RELAY_OFF        LOW

#endif // HARDWARE_MAP_LITE_VNZLA_H
