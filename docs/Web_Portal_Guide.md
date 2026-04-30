# 🌐 Guía de Configuración: SmartAccess Web Portal (v7.0.0)

Esta guía explica cómo configurar tu nodo de control de acceso sin tocar una sola línea de código.

## 1. Activación del Modo Configuración
Si el dispositivo no tiene credenciales de WiFi guardadas (o no logra conectarse), entrará automáticamente en **Modo Configuración**:
- El LED de estado parpadeará lentamente (cada 500ms).
- El ESP32 creará una red WiFi propia llamada: `SmartAccess-Setup`.

## 2. Acceso al Portal
1. Desde tu smartphone o laptop, conéctate a la red WiFi `SmartAccess-Setup`.
2. Abre tu navegador web (Chrome, Safari, etc.).
3. Si no se abre automáticamente, navega a la dirección IP: `192.168.4.1`.

## 3. Configuración de Parámetros
En el formulario, completa los siguientes campos:
- **SSID:** Nombre de tu red WiFi doméstica o de oficina.
- **PASS:** Contraseña de tu red WiFi.
- **MQTT:** Dirección del broker (ej: `broker.smartaccess.com.ve`).
- **ID:** Identificador único del nodo (ej: `01`).

## 4. Guardado y Reinicio
Haz clic en **"Guardar y Reiniciar"**. El dispositivo guardará los datos en su memoria Flash interna, se desconectará del modo AP y se unirá a tu red WiFi local.

---
**Nota:** Para forzar el regreso al modo configuración, puedes enviar el comando `CLEAR_CFG` vía MQTT o borrar la memoria Flash desde el puerto serial.
