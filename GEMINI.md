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

## 📘 ESTÁNDAR SMARTACCESS: METODOLOGÍA DE TRABAJO HÍBRIDO (Vnzla Edition)

### 1. Arquitectura de "Contenedor de Proyecto"
Todo proyecto debe residir en un contenedor local que separe la operativa del código fuente:
- **Ruta Local:** `C:\Users\Franc\SmartAccess-Workspaces\<nombre-proyecto>`
- **Sub-carpetas Obligatorias:**
  - `\git`: Almacenamiento exclusivo del motor de versiones (.git).
  - `\docs`: Documentación local y diagramas.
  - `\code`: Enlace virtual (Symlink) hacia Google Drive.

### 2. Implementación de Enlaces Virtuales (Symlinks)
El código fuente vive en la nube (Google Drive) para redundancia, pero se opera desde el contenedor local:
- **Comando:** `cmd /c mklink /D "<Ruta-Local>\code" "<Ruta-Drive>"`
- **Regla Crítica:** La carpeta de destino (`\code`) **NO** debe existir antes de ejecutar el comando.
- **Privilegios:** Requiere ejecución como **Administrador**.

### 3. Aislamiento del Control de Versiones (Git)
Para evitar "ruido" en la nube y conflictos de sincronización:
- **Comando:** `git init --separate-git-dir="<Ruta-Local>\git"`
- El archivo `.git` en la carpeta de código será solo un puntero al motor local.

### 4. Protocolo de Seguridad Windows (Desbloqueo)
Si Windows bloquea scripts ejecutados vía Symlink:
- Ir a la ruta original en Google Drive -> Propiedades -> **Desbloquear**.
