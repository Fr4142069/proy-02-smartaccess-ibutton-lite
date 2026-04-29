# 📘 SMARTACCESS MASTER DOCUMENTATION (v6.6.0) - PROY-02
**Proyecto:** SmartAccess iButton Lite | **Versión:** v6.6.0 (Vnzla Edition)

## 0. Metadatos de Gestión
*   **ID del Proyecto:** proy-02-smartaccess-ibutton-lite
*   **Lead Engineer:** Francisco
*   **Arquitectura:** Edge Node (ESP32)
*   **Estado:** Inicialización
*   **Repositorio GitHub:** https://github.com/Fr4142069/proy-02-smartaccess-ibutton-lite

## 1. Identidad del Proyecto
*   **Nombre Oficial:** SmartAccess iButton Lite v6.6
*   **Filosofía:** "Offline-First & Resilience". Nodo de control de acceso optimizado para entornos con fallas eléctricas y de internet.

## 2. Arquitectura del Sistema
*   **Core:** ESP32 (WROOM/C3).
*   **Input:** iButton (OneWire) GPIO 4.
*   **Output:** Dual Relay (Peatonal/Portón) GPIO 12/13.
*   **Storage:** Flash local (Preferences.h) para ACL.

## 3. Lógica de Negocio y Seguridad
*   **Offline-First:** Las llaves autorizadas se validan contra la Flash local si el servidor no está disponible.
*   **No Bloqueante:** Uso de `millis()` para lectura de iButton y comunicación MQTT.

## 4. Hardware Map (Resumen)
*   GPIO 4: OneWire (Pull-up)
*   GPIO 12: Relé 1 (Cerradura)
*   GPIO 13: Relé 2 (Portón/Luz)
*   GPIO 14: Sensor de Puerta
*   GPIO 2: LED de Estado

## 7. Hoja de Roadmap
*   **Hito 1:** Inicialización de arquitectura híbrida. -> **[COMPLETADO]**
*   **Hito 2:** Implementación de código base no bloqueante. -> **[EN PROCESO]**
*   **Hito 3:** Sincronización SQL vía MQTT. -> **https://github.com/Fr4142069/proy-02-smartaccess-ibutton-lite**

---
**SmartAccess PRO Security | v14.9.11**

## 12. Pruebas y Simulación
Para probar la lógica del nodo sin hardware físico:
1. Instala las dependencias: `pip install paho-mqtt`
2. Ejecuta el simulador: `python tools/mqtt_simulator.py`
3. Usa el simulador para enviar comandos `ADD` o `DEL` y observa la respuesta `SYNC_OK` del nodo.
