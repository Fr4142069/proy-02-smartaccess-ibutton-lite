# SmartAccess iButton Lite - Project Instructions

## Reglas de Arquitectura
- **Stack:** ESP32 C++/Arduino.
- **Protocolo de Comunicación:** WiFi + MQTT (smartaccess/nodos/+/eventos).
- **Prioridad:** Operación Offline-First (ACL en Flash local).
- **Concurrencia:** Prohibido el uso de `delay()`. Todo el manejo de tiempos debe ser vía `millis()`.
- **Hardware:** Exclusivamente iButton. No incluir soporte para Wiegand o RS485.

## Estándares de Código
- Los pines deben estar definidos en `include/Hardware_Map_Lite_Vnzla.h`.
- El estado inicial de los relés debe ser siempre LOW.
- La lectura OneWire debe ser asíncrona/no bloqueante en el loop principal.
