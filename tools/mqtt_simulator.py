import paho.mqtt.client as mqtt
import json
import time
import ssl
from datetime import datetime

# --- CONFIGURACIÓN ---
MQTT_BROKER = "broker.smartaccess.com.ve"
MQTT_PORT = 8883  # Puerto seguro TLS
TOPIC_EVENTS = "smartaccess/nodos/eventos"
TOPIC_CONTROL = "smartaccess/nodos/control"
NODE_ID = "01"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[*] Conectado exitosamente vía TLS (rc: {rc})")
        client.subscribe(TOPIC_EVENTS)
    else:
        print(f"[!] Error de conexión: {rc}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        print(f"\n[{data.get('ts', 'N/A')}] {data.get('node')} ({data.get('type')}): {data.get('msg')}")
        if "data" in data: print(f"    -> Datos: {data['data']}")
    except Exception as e:
        print(f"\n[!] Error: {msg.payload.decode()} | {e}")

def send_json_command(client, cmd, extra_data=None):
    payload = {"id": NODE_ID, "cmd": cmd, "ts": datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
    if extra_data: payload.update(extra_data)
    client.publish(TOPIC_CONTROL, json.dumps(payload))
    print(f"[>] Enviado (TLS): {cmd}")

if __name__ == "__main__":
    client = mqtt.Client("SmartAccess_Secure_Sim")
    
    # --- CONFIGURACIÓN TLS ---
    # client.tls_set(cert_reqs=ssl.CERT_NONE) # Usar si el certificado es auto-firmado
    client.tls_set() # Usa los certificados del sistema por defecto
    
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        print(f"[*] Iniciando Conexión Segura TLS a {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        
        while True:
            print("\n1. Abrir | 2. Reset | 3. Salir")
            opt = input("Seleccione: ")
            if opt == "1": send_json_command(client, "RELAY", {"id": 1, "state": 1})
            elif opt == "2": send_json_command(client, "REBOOT")
            elif opt == "3": break
            time.sleep(1)
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()
