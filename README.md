# 📘 SMARTACCESS MASTER DOCUMENTATION (v7.2.0) - PROY-02
**Proyecto:** SmartAccess iButton Lite | **Versión:** v7.2.0 (Ultimate Fleet Edition)

## 0. Metadatos de Gestión (Hoja de Registro Inmutable)
*   **ID del Proyecto:** proy-02-smartaccess-ibutton-lite
*   **Lead Engineer:** Francisco
*   **Arquitectura:** SmartAccess Hybrid (Container + Drive Symlink)
*   **Estado:** ENTREGADO - PRODUCCIÓN READY
*   **Repositorio GitHub:** [https://github.com/Fr4142069/proy-02-smartaccess-ibutton-lite](https://github.com/Fr4142069/proy-02-smartaccess-ibutton-lite)
*   **Proyecto GitHub (Tablero):** [https://github.com/users/Fr4142069/projects/7](https://github.com/users/Fr4142069/projects/7)

## 1. Identidad del Proyecto
*   **Nombre Oficial:** SmartAccess iButton Lite v7.2.0
*   **Filosofía:** "Zero Pérdida de Contexto". Nodo de control de acceso de borde ultra-resiliente, diseñado para operar en condiciones extremas de red y energía.

## 2. Arquitectura del Sistema
*   **Core:** ESP32 (WROOM/C3) - Framework Arduino/C++.
*   **Comunicaciones:** WiFi (2.4GHz) + mDNS (smart-node-01.local) + MQTT JSON.
*   **Seguridad:** Watchdog Timer (8s), Anti-Tamper Switch, MQTT LWT (Last Will).
*   **UX:** Patrones de LED no bloqueantes y Portal Web de Configuración (Captive Portal).

## 3. Lógica de Negocio y Seguridad
*   **Offline-First:** Validación ACL en Flash local. Registro de logs offline con auto-sincronización histórica.
*   **Telemetría:** Monitoreo de Free Heap, RSSI y Uptime en cada Heartbeat.
*   **Mantenimiento:** Soporte OTA (Over-The-Air) y comandos remotos JSON (Reboot, Clear ACL, etc.).

## 4. Hardware Map (Pinout ESP32)
*   **GPIO 4:** Bus OneWire (iButton Dallas Key).
*   **GPIO 12:** Relé 1 (Cerradura Peatonal).
*   **GPIO 13:** Relé 2 (Luz de Cortesía / Portón).
*   **GPIO 14:** Sensor Reed (Estado de Puerta).
*   **GPIO 15:** Tamper Switch (Sabotaje de Gabinete).
*   **GPIO 2:** LED de Estado.

## 6. Guía de Instalación y Despliegue (Estándar SmartAccess)
1. **Contenedor Local:** Crear `C:\Users\Franc\SmartAccess-Workspaces\proy-02-smartaccess-ibutton-lite`.
2. **Symlink:** Ejecutar como Admin: `New-Item -ItemType SymbolicLink -Path "...\code" -Target "...\Drive_Path"`.
3. **Firmware:** Usar PlatformIO. Configurar `include/secrets.h` (Ver `secrets.h.example`).
4. **Setup:** Conectar al WiFi `SmartAccess-Setup` para configurar parámetros vía Web.

## 7. Hoja de Roadmap (Hitos Finalizados)
*   **M1 [Infraestructura]:** Contenedor Local, Symlink y Aislamiento Git. -> **[COMPLETADO]**
*   **M2 [Core]:** Lógica No Bloqueante, ACL Local y Offline Logging. -> **[COMPLETADO]**
*   **M3 [Cloud]:** Arquitectura JSON, Telemetría y Fleet Management (OTA). -> **[COMPLETADO]**
*   **M4 [Net]:** mDNS, LWT, WiFi Scan y Web Portal Profesional. -> **[COMPLETADO]**
*   **M5 [Intelligence]:** Grounding NotebookLM (Context Package). -> **[COMPLETADO]**

## 11. Ecosistema de Pruebas y Auditoría
*   **Simulador:** `tools/mqtt_simulator.py` (Control JSON y Monitoreo).
*   **Reportes:** `tools/report_generator.py` (Generación de CSV de Auditoría).

---
**SmartAccess PRO Security | v14.9.11**
