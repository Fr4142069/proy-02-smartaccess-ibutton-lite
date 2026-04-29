# 📘 SMARTACCESS MASTER DOCUMENTATION (v6.6.0) - PROY-02
**Proyecto:** SmartAccess iButton Lite | **Versión:** v6.6.0 (Final Gold)

## 0. Metadatos de Gestión (Hoja de Registro Inmutable)
*   **ID del Proyecto:** proy-02-smartaccess-ibutton-lite
*   **Lead Engineer:** Francisco
*   **Arquitectura:** Edge Node (ESP32) - Offline-First
*   **Estado:** Entregado (Producción-Piloto)
*   **Repositorio GitHub:** [https://github.com/Fr4142069/proy-02-smartaccess-ibutton-lite](https://github.com/Fr4142069/proy-02-smartaccess-ibutton-lite)
*   **Proyecto GitHub (Tablero):** [https://github.com/users/Fr4142069/projects/7](https://github.com/users/Fr4142069/projects/7)

## 1. Identidad del Proyecto
*   **Nombre Oficial:** SmartAccess iButton Lite v6.6
*   **Filosofía:** "Zero Pérdida de Contexto". Nodo de control de acceso resiliente diseñado para el mercado venezolano, optimizado para fallas eléctricas y de red.

## 2. Arquitectura del Sistema
*   **Core:** ESP32 (WROOM/C3) - Framework Arduino/C++.
*   **Comunicaciones:** WiFi (2.4GHz) + MQTT (PubSub).
*   **Almacenamiento:** Flash local (Preferences) para ACL y Logs Offline.
*   **Seguridad:** Watchdog Timer (WDT) + Anti-Tamper Switch.

## 3. Lógica de Negocio y Seguridad
*   **Offline-First:** Validación local de llaves iButton y almacenamiento de eventos si falla la red.
*   **No Bloqueante:** Implementación 100% basada en `millis()` y máquinas de estados.
*   **Sincronización:** Auto-reconexión MQTT y vaciado automático de logs offline al recuperar conexión.

## 4. Hardware Map (Pinout ESP32)
*   **GPIO 4:** Bus OneWire (iButton Dallas Key).
*   **GPIO 12:** Relé 1 (Cerradura Peatonal).
*   **GPIO 13:** Relé 2 (Portón/Luz de cortesía).
*   **GPIO 14:** Sensor Reed (Estado de Puerta).
*   **GPIO 15:** Tamper Switch (Sabotaje de Gabinete).
*   **GPIO 2:** LED de Estado (UX visual).

## 5. Protocolos de Comunicación
*   **Topic Control:** `smartaccess/nodos/01/control` (Comandos `ADD:KEY` / `DEL:KEY`).
*   **Topic Events:** `smartaccess/nodos/01/eventos` (Eventos `ACCESS_GRANTED`, `DOOR_OPENED`, `ALARM`, etc.).

## 6. Guía de Instalación y Despliegue
1. Clonar el repositorio.
2. Renombrar `include/secrets.h.example` a `include/secrets.h` y completar credenciales.
3. Usar VS Code + PlatformIO para compilar y subir al ESP32.
4. Ejecutar `tools/setup_environment.ps1` como Admin para configurar el entorno local.

## 7. Hoja de Roadmap (Hitos)
*   **M1.RC1:** Inicialización de arquitectura híbrida y motor Git. -> **[COMPLETADO]**
*   **M2.RC1:** Sincronización ACL vía MQTT y lógica Offline-First. -> **[COMPLETADO]**
*   **M2.RC2:** Optimización No Bloqueante y Heartbeat. -> **[COMPLETADO]**
*   **M2.RC4:** UX Avanzada (LED Patterns) y Watchdog Timer. -> **[COMPLETADO]**
*   **M2.RC5:** Diagnósticos de Reinicio y Anti-Tamper. -> **[COMPLETADO]**
*   **M2.RC6:** Offline Logging (Resiliencia de Datos). -> **[COMPLETADO]**

## 8. Mantenimiento y Diagnóstico
*   **Reset Reasons:** El nodo reporta por qué se reinició (`POWER_ON`, `TASK_WDT`, etc.).
*   **Heartbeat:** Latido cada 30s para verificar estado "vivo" del dispositivo.

## 9. Seguridad Física y Lógica
*   **Tamper:** Alarma visual y remota inmediata si se abre el gabinete.
*   **WDT:** Reinicio automático en 8s ante bloqueos de software.

## 10. Estándares de Código y Git
*   **Commit Format:** `<tipo>(M[X].RC[Y]): <descripción>`
*   **Branches:** Trabajo directo en `master` para este piloto.

## 11. Ecosistema de Pruebas
*   **Simulador:** `tools/mqtt_simulator.py` (Requiere `paho-mqtt`).
*   **Comandos:** Simula el Centro de Control y recibe eventos del nodo.

---
**SmartAccess PRO Security | v14.9.11**
