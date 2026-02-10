import os
import sys
import time
import threading

DEVICE = "/dev/egb"

# Almacenamos el último dato de telemetría recibido
telemetria = {"v": 0.0, "i": 0.0, "actualizado": False}

def parse_data(line):
    try:
        if "V:" in line and ",I:" in line:
            parts = line.split("V:")[1].split(",I:")
            return float(parts[0]), float(parts[1].strip())
    except:
        pass
    return None, None

def hilo_lectura():
    """Lee el driver sin parar para evitar que el kfifo se llene"""
    global telemetria
    try:
        with open(DEVICE, "r") as f:
            while True:
                line = f.readline()
                if line:
                    v, i = parse_data(line.strip())
                    if v is not None:
                        telemetria["v"] = v
                        telemetria["i"] = i
                        telemetria["actualizado"] = True
    except Exception as e:
        print(f"\n[Error de Lectura]: {e}")

def main():
    if not os.path.exists(DEVICE):
        print(f"Error: {DEVICE} no encontrado. Revisa el driver."); return

    # Iniciar el hilo de lectura constante
    t = threading.Thread(target=hilo_lectura, daemon=True)
    t.start()

    print("========================================")
    print("   CONTROL DE FUENTE RPI -> PICO        ")
    print("========================================")
    print("Instrucciones:")
    print(" - Ingresa el voltaje (ej: 15.4)")
    print(" - Escribe 'mon' para ver datos 10 segundos")
    print(" - Ctrl+C para salir\n")

    try:
        while True:
            entrada = input("Comando o Voltaje > ").strip().lower()

            if entrada == 'mon':
                print("--- Monitoreando 10s ---")
                for _ in range(10):
                    print(f" -> {time.strftime('%H:%M:%S')} | V: {telemetria['v']:5.2f}V | I: {telemetria['i']:5.2f}A")
                    time.sleep(1)
                print("--- Fin monitoreo ---\n")
            
            elif entrada:
                try:
                    valor = float(entrada)
                    comando = f"S{valor}\n" # Aquí añadimos la S y el salto de línea
                    
                    with open(DEVICE, "w") as f:
                        f.write(comando)
                    
                    print(f"Sent: {comando.strip()} ... Esperando respuesta")
                    
                    # Pequeña pausa y mostramos 3 lecturas para ver el cambio
                    time.sleep(0.5)
                    for _ in range(3):
                        print(f" Confirmación -> V: {telemetria['v']:5.2f}V")
                        time.sleep(1)
                        
                except ValueError:
                    print("Error: Ingresa un número válido.")

    except KeyboardInterrupt:
        print("\nCerrando comunicación...")

if __name__ == "__main__":
    main()
