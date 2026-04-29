import paho.mqtt.client as mqtt
import time

# --- CONFIGURACIÓN ---
MQTT_BROKER = "broker.smartaccess.com.ve"
MQTT_PORT = 1883
TOPIC_EVENTS = "smartaccess/nodos/+/eventos"
TOPIC_CONTROL = "smartaccess/nodos/01/control"

def on_connect(client, userdata, flags, rc):
    print(f"Conectado al Simulador de Centro de Control (rc: {rc})")
    client.subscribe(TOPIC_EVENTS)
    print(f"Escuchando eventos en: {TOPIC_EVENTS}")

def on_message(client, userdata, msg):
    print(f"\n[EVENTO RECIBIDO] {msg.topic} -> {msg.payload.decode()}")

def send_command(client):
    while True:
        print("\n--- COMANDOS DE SIMULACIÓN ---")
        print("1. Agregar Llave (ADD:A1B2C3D4E5F6G7H8)")
        print("2. Eliminar Llave (DEL:A1B2C3D4E5F6G7H8)")
        print("3. Salir")
        choice = input("Seleccione una opción: ")
        
        if choice == "1":
            key = input("Ingrese ID de llave (HEX): ")
            client.publish(TOPIC_CONTROL, f"ADD:{key}")
            print(f"Comando enviado: ADD:{key}")
        elif choice == "2":
            key = input("Ingrese ID de llave (HEX): ")
            client.publish(TOPIC_CONTROL, f"DEL:{key}")
            print(f"Comando enviado: DEL:{key}")
        elif choice == "3":
            break

if __name__ == "__main__":
    client = mqtt.Client("SmartAccess_Admin_Sim")
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        send_command(client)
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()
