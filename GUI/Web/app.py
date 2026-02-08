import eventlet
eventlet.monkey_patch()  # Optimiza el manejo de hilos para SocketIO

import serial
from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import time
import threading

app = Flask(__name__)
# Permitimos CORS para evitar bloqueos del navegador
socketio = SocketIO(app, cors_allowed_origins="*")

# --- Configuración del Puerto Serie ---
# Usamos /dev/ttyS0 que es el puerto UART por defecto en los pines GPIO 14 y 15 de la RPi 4
try:
    ser = serial.Serial('/dev/ttyS0', 115200, timeout=0.1)
    print(">>> Puerto serie /dev/ttyS0 abierto correctamente.")
except Exception as e:
    print(f">>> ERROR: No se pudo abrir el puerto serie: {e}")
    ser = None

@app.route('/')
def index():
    return render_template('index.html')

# EVENTO: Recibir tensión desde el slider de la web
@socketio.on('set_voltage')
def handle_voltage(data):
    try:
        v_target = float(data['value'])

        # Validación de seguridad
        if 12.0 <= v_target <= 24.0:
            print(f"Enviando Setpoint a Pico 2: {v_target}V")

            if ser and ser.is_open:
                # Formato: SXX.XX\n (ejemplo: S19.50\n)
                comando = f"S{v_target:.2f}\n"
                ser.write(comando.encode('utf-8'))
        else:
            print(f"Valor fuera de rango: {v_target}")

    except ValueError:
        print("Error: El valor recibido no es numérico.")

# FUNCIÓN: Lectura REAL de la Pico 2
def read_pico_real():
    print(">>> Hilo de lectura serie iniciado...")
    while True:
        if ser and ser.is_open:
            try:
                # Lee una línea completa terminada en \n
                line = ser.readline().decode('utf-8').strip()

                # Esperamos el formato: "V:XX.XX,I:X.XX"
                if line.startswith("V:"):
                    parts = line.split(',')
                    v_val = parts[0].split(':')[1]
                    i_val = parts[1].split(':')[1]

                    # Enviamos los datos reales a la interfaz web
                    socketio.emit('update_metrics', {
                        'voltage': v_val,
                        'current': i_val
                    })
            except Exception as e:
                # Evitamos que un error de lectura rompa el hilo
                pass

        # Pequeña pausa para no saturar la CPU
        time.sleep(0.1)

if __name__ == '__main__':
    # Lanzamos el hilo de lectura en segundo plano
    t = threading.Thread(target=read_pico_real)
    t.daemon = True
    t.start()

    # Iniciamos el servidor Flask
    # host='0.0.0.0' es vital para acceder desde tu PC (alan.local)
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)