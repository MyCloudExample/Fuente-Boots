import eventlet
eventlet.monkey_patch()

import serial
import subprocess  # <--- Agregamos esto
from flask import Flask, render_template
from flask_socketio import SocketIO
import time

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

# --- Función para "despertar" el puerto como lo hace cat ---
def inicializar_puerto_sistema():
    try:
        # Esto hace lo mismo que el comando stty que usamos antes
        subprocess.run(["stty", "-F", "/dev/ttyS0", "115200", "raw", "-echo"], check=True)
        print(">>> Configuración de sistema aplicada al puerto.")
    except Exception as e:
        print(f">>> Error configurando puerto: {e}")

# Llamamos a la configuración antes de abrir el puerto en Python
inicializar_puerto_sistema()

try:
    # Abrimos el puerto. 
    # Añadimos rtscts=False y dsrdtr=False para evitar bloqueos de hardware
    ser = serial.Serial('/dev/ttyS0', 115200, timeout=0.1, rtscts=False, dsrdtr=False)
    ser.reset_input_buffer() 
    print(">>> Puerto serie abierto y listo.")
except Exception as e:
    print(f">>> ERROR Serie: {e}")
    ser = None

# ... (resto de tus rutas y funciones de set_voltage iguales) ...

def read_pico_real():
    print(">>> Iniciando lectura...")
    while True:
        if ser and ser.is_open:
            try:
                # Si el puerto parece "dormido", forzamos una lectura de lo que haya
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line.startswith("V:"):
                        parts = line.split(',')
                        v_val = parts[0].split(':')[1]
                        i_val = parts[1].split(':')[1]
                        socketio.emit('update_metrics', {'voltage': v_val, 'current': i_val})
            except Exception:
                pass
        socketio.sleep(0.01) # Pausa mínima para no saturar

if __name__ == '__main__':
    socketio.start_background_task(read_pico_real)
    socketio.run(app, host='0.0.0.0', port=5000)
import eventlet
eventlet.monkey_patch()

import serial
import subprocess
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

# --- Función para forzar la configuración del sistema (El truco del CAT) ---
def configurar_puerto_linux():
    try:
        # Forzamos al sistema operativo a poner el puerto en modo crudo
        subprocess.run(["stty", "-F", "/dev/ttyS0", "115200", "raw", "-echo", "-echoe", "-echok"], check=True)
        print(">>> Sistema: Puerto configurado en modo RAW.")
    except Exception as e:
        print(f">>> Advertencia: No se pudo ejecutar stty: {e}")

# Ejecutamos la configuración antes de cualquier otra cosa
configurar_puerto_linux()

# Global para el puerto
ser = None

def conectar_serie():
    global ser
    try:
        # Abrimos el puerto con parámetros que evitan bloqueos
        ser = serial.Serial('/dev/ttyS0', 115200, timeout=0.1, rtscts=False, dsrdtr=False)
        ser.reset_input_buffer()
        print(">>> Serie: Conexión establecida.")
    except Exception as e:
        print(f">>> ERROR Serie: {e}")

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('set_voltage')
def handle_voltage(data):
    if ser and ser.is_open:
        try:
            v_target = float(data['value'])
            comando = f"S{v_target:.2f}\n"
            ser.write(comando.encode('utf-8'))
        except Exception as e:
            print(f"Error enviando: {e}")

def read_pico_real():
    print(">>> Iniciando hilo de telemetría...")
    while True:
        if ser and ser.is_open:
            try:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line.startswith("V:"):
                        parts = line.split(',')
                        v_val = parts[0].split(':')[1]
                        i_val = parts[1].split(':')[1]
                        socketio.emit('update_metrics', {'voltage': v_val, 'current': i_val})
            except Exception:
                pass
        # IMPORTANTE: socketio.sleep es necesario con eventlet
        socketio.sleep(0.01)

if __name__ == '__main__':
    conectar_serie()
    # Usamos la tarea de fondo de socketio que es más segura
    socketio.start_background_task(read_pico_real)
    # Cambiamos debug a False para evitar reinicios dobles que bloquean el puerto
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
