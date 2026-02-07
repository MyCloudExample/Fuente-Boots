from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import time
import threading

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

@app.route('/')
def index():
    return render_template('index.html')

# Evento: Recibir tensión del usuario
@socketio.on('set_voltage')
def handle_voltage(data):
    try:
        v_target = float(data['value'])
        
        # Validación de seguridad: Rango 12V - 24V
        if 12.0 <= v_target <= 24.0:
            print(f"Comando validado. Enviando a Pico 2: {v_target}V")
            # Aquí irá tu lógica: serdev_write(f"V{v_target}\n")
        else:
            print(f"Intento de seteo fuera de rango: {v_target}V")
            
    except ValueError:
        print("Error: El valor recibido no es numérico.")
        
# Simulación de recepción de datos (Feedback de la Pico 2)
def mock_pico_feedback():
    while True:
        # En el futuro, esto leerá el puerto serie
        dummy_data = {
            'voltage': "12.45", 
            'current': "0.85"
        }
        socketio.emit('update_metrics', dummy_data)
        time.sleep(1) # Actualiza cada segundo

if __name__ == '__main__':
    # Lanzamos el hilo de lectura serie/simulación
    t = threading.Thread(target=mock_pico_feedback)
    t.daemon = True
    t.start()
    
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)