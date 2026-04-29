/**
 * @file Hardware_Map_Lite_Vnzla.h
 * @brief Mapeo de Hardware para Nodo de Borde iButton Lite v6.6
 */

#ifndef HARDWARE_MAP_LITE_VNZLA_H
#define HARDWARE_MAP_LITE_VNZLA_H

#include <Arduino.h>

// Lector iButton (Dallas Key)
#define PIN_IBUTTON      4   

// Salidas de Control (Relés)
#define PIN_RELE_1       12  // Cerradura Peatonal
#define PIN_RELE_2       13  // Portón / Luz

// Sensores
#define PIN_SENSOR_DOOR  14  // Sensor Reed (Estado Puerta)
#define PIN_TAMPER_SW    15  // Sensor Anti-Sabotaje (Gabinete)

// Interfaz Visual
#define PIN_LED_STATUS   2   

#define RELAY_ON         HIGH
#define RELAY_OFF        LOW

#endif
