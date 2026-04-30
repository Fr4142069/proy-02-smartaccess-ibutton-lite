import paho.mqtt.client as mqtt
import json
import csv
import os
from datetime import datetime

# --- CONFIGURACIÓN ---
MQTT_BROKER = "broker.smartaccess.com.ve"
TOPIC_EVENTS = "smartaccess/nodos/eventos"
LOG_FILE = "audit_report.csv"

def on_connect(client, userdata, flags, rc):
    print(f"[*] Servidor de Auditoría Conectado (rc: {rc})")
    client.subscribe(TOPIC_EVENTS)
    
    # Inicializar CSV si no existe
    if not os.path.exists(LOG_FILE):
        with open(LOG_FILE, mode='w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(["Timestamp", "Nodo", "Tipo", "Evento", "Datos", "Heap", "RSSI", "Sincronizado"])

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        ts = data.get("ts", "N/A")
        node = data.get("node", "Unknown")
        type_ = data.get("type", "Info")
        message = data.get("msg", "")
        extra_data = data.get("data", "")
        heap = data.get("heap", 0)
        rssi = data.get("rssi", 0)
        is_sync = data.get("sync", False)

        # Guardar en CSV
        with open(LOG_FILE, mode='a', newline='') as file:
            writer = csv.writer(file)
            writer.writerow([ts, node, type_, message, extra_data, heap, rssi, is_sync])
        
        print(f"[{ts}] Log Guardado: {node} - {message} ({'Sincronizado' if is_sync else 'Vivo'})")
            
    except Exception as e:
        print(f"[!] Error: {e}")

if __name__ == "__main__":
    client = mqtt.Client("SmartAccess_Audit_Server")
    client.on_connect = on_connect
    client.on_message = on_message
    
    print(f"[*] Iniciando Servidor de Auditoría... Guardando en {LOG_FILE}")
    client.connect(MQTT_BROKER, 1883, 60)
    client.loop_forever()
