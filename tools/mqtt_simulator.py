import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

# --- CONFIGURACIÓN ---
MQTT_BROKER = "broker.smartaccess.com.ve"
MQTT_PORT = 1883
TOPIC_EVENTS = "smartaccess/nodos/eventos"
TOPIC_CONTROL = "smartaccess/nodos/control"
NODE_ID = "01"  # ID del nodo a controlar

def on_connect(client, userdata, flags, rc):
    print(f"[*] Conectado al Centro de Control JSON (rc: {rc})")
    client.subscribe(TOPIC_EVENTS)
    print(f"[*] Escuchando telemetría en: {TOPIC_EVENTS}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        ts = data.get("ts", "N/A")
        node = data.get("node", "Unknown")
        type_ = data.get("type", "Info")
        message = data.get("msg", "")
        
        print(f"\n[{ts}] {node} ({type_}): {message}")
        
        # Mostrar telemetría adicional si es un Heartbeat
        if type_ == "HB":
            heap = data.get("heap", 0)
            rssi = data.get("rssi", 0)
            print(f"    -> Salud: Heap: {heap} bytes | Señal: {rssi} dBm")
        elif "data" in data:
            print(f"    -> Datos: {data['data']}")
            
    except Exception as e:
        print(f"\n[!] Error procesando mensaje: {msg.payload.decode()} | {e}")

def send_json_command(client, cmd, extra_data=None):
    payload = {
        "id": NODE_ID,
        "cmd": cmd,
        "ts": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }
    if extra_data:
        payload.update(extra_data)
        
    client.publish(TOPIC_CONTROL, json.dumps(payload))
    print(f"[>] Comando enviado: {cmd}")

def menu():
    print("\n--- SMARTACCESS CONTROL CENTER (JSON) ---")
    print("1. Abrir Puerta (Relé 1)")
    print("2. Activar Luz (Relé 2)")
    print("3. Agregar Llave ACL")
    print("4. Eliminar Llave ACL")
    print("5. Reiniciar Nodo")
    print("6. Salir")
    return input("Seleccione opción: ")

if __name__ == "__main__":
    client = mqtt.Client("SmartAccess_Cloud_Sim")
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        
        while True:
            opt = menu()
            if opt == "1": send_json_command(client, "RELAY", {"id": 1, "state": 1})
            elif opt == "2": send_json_command(client, "RELAY", {"id": 2, "state": 1})
            elif opt == "3":
                key = input("ID de Llave (HEX): ")
                send_json_command(client, "ADD", {"key": key})
            elif opt == "4":
                key = input("ID de Llave (HEX): ")
                send_json_command(client, "DEL", {"key": key})
            elif opt == "5": send_json_command(client, "REBOOT")
            elif opt == "6": break
            time.sleep(1)
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()
