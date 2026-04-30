# 🧠 SMARTACCESS CONTEXT PACKAGE (v7.2.0) - GROUNDING FOR NOTEBOOKLM
**Proyecto:** SmartAccess iButton Lite | **ID:** proy-02-smartaccess-ibutton-lite
**Filosofía:** Zero Pérdida de Contexto | **Arquitectura:** Edge Resilient

## 1. RESUMEN EJECUTIVO
Sistema de control de acceso de borde para ESP32, optimizado para resiliencia ante fallas eléctricas y de red. Utiliza tecnología iButton para validación y MQTT para gestión en la nube, operando bajo un esquema "Offline-First".

## 2. ARQUITECTURA TÉCNICA
- **Core:** ESP32 (Framework Arduino C++).
- **Conectividad:** WiFi 2.4GHz con mDNS (smart-node-01.local) y WiFi Scan en portal.
- **Protocolo Cloud:** MQTT con Payloads JSON estructurados y LWT (Last Will and Testament).
- **Resiliencia:** Watchdog Timer (8s), Registro de logs offline con sincronización automática, y Diagnóstico de reinicio (Reset Reason).
- **Seguridad Física:** Sensor Anti-Tamper con estados de alarma visual (LED) y remota (MQTT).

## 3. MAPA DE HARDWARE (PINOUT)
- GPIO 4: OneWire (iButton)
- GPIO 12: Relé 1 (Cerradura)
- GPIO 13: Relé 2 (Luz de Cortesía/Portón)
- GPIO 14: Sensor Reed (Puerta)
- GPIO 15: Tamper Switch (Gabinete)
- GPIO 2: LED de Estado (UX visual con patrones)

## 4. LÓGICA DE OPERACIÓN
- **Boot:** Si no hay WiFi, lanza un Captive Portal (Web Setup). Si hay WiFi, sincroniza hora vía NTP.
- **Acceso:** Validación contra ACL en Flash (Preferences). Si se concede: Relé 1 (1s) + Relé 2 (30s).
- **Fallas:** Si MQTT cae, guarda logs en Flash. Al reconectar, vacía la cola de logs automáticamente.
- **Seguridad:** El Watchdog reinicia el sistema si el loop se bloquea por más de 10s.

## 5. ESTADO DEL DESARROLLO (V7.2.0)
- ✅ Infraestructura Híbrida (Drive/Local) validada.
- ✅ Código 100% No Bloqueante.
- ✅ Gestión de Flotas (OTA/Comandos Remotos).
- ✅ Telemetría de Salud (Heap/RSSI).

## 6. GUÍA DE MANTENIMIENTO
- Comandos MQTT: `REBOOT`, `GET_INFO`, `GET_HEALTH`, `CLEAR_ACL`, `ADD:{key}`, `DEL:{key}`.
- Portal Web: Accesible vía IP o mDNS para configuración sin código.

---
**Documento generado por Gemini CLI para SmartAccess PRO Security.**
