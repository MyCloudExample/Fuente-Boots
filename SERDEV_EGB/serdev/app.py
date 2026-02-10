from flask import Flask, render_template, jsonify, request
import threading
import time

app = Flask(__name__)

# Estado del sistema
estado_sistema = {"status": "Iniciando...", "ultimo_seteo": "Ninguno"}

def leer_pico():
    global estado_sistema
    while True:
        try:
            with open("/dev/egb", "r") as f:
                linea = f.readline().strip()
                if linea:
                    # Si el driver dice "Dato valido", actualizamos el estado
                    estado_sistema["status"] = "Conectado (Recibiendo Datos)"
                    print(f"Driver dice: {linea}")
        except Exception as e:
            estado_sistema["status"] = f"Error: {e}"
        time.sleep(1)

threading.Thread(target=leer_pico, daemon=True).start()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    return jsonify(estado_sistema)

@app.route('/enviar', methods=['POST'])
def enviar():
    global estado_sistema
    nuevo_valor = request.form.get('valor')
    try:
        # Formateamos el comando según el protocolo: S + valor + \n
        comando = f"S{nuevo_valor}\n"
        
        with open("/dev/egb", "w") as f:
            f.write(comando)
            f.flush()
            # Aseguramos que el driver de Linux lo envíe al hardware
            import os
            os.fsync(f.fileno())
            
        estado_sistema["ultimo_seteo"] = comando.strip()
        print(f"Enviado a Pico: {comando.strip()}")
        return "OK", 200
    except Exception as e:
        print(f"Error al enviar: {e}")
        return str(e), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
