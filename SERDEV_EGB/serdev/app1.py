from flask import Flask, render_template, jsonify, request
import threading
import time
import os

app = Flask(__name__)

DEVICE = "/dev/egb"

# Usamos tu misma estructura de telemetría
telemetria = {"v": 0.0, "i": 0.0, "status": "Iniciando..."}

def parse_data(line):
    """Tu lógica original de extracción"""
    try:
        if "V:" in line and ",I:" in line:
            parts = line.split("V:")[1].split(",I:")
            return float(parts[0]), float(parts[1].strip())
    except:
        pass
    return None, None

def hilo_lectura():
    """Lee el driver sin parar, tal como en tu script original"""
    global telemetria
    while True:
        try:
            if os.path.exists(DEVICE):
                with open(DEVICE, "r") as f:
                    while True:
                        line = f.readline()
                        if line:
                            # Imprimimos en consola para que veas que llegan datos
                            #print(f"Driver Raw: {line.strip()}")
                            
                            v, i = parse_data(line.strip())
                            if v is not None:
                                telemetria["v"] = v
                                telemetria["i"] = i
                                telemetria["status"] = "Datos Reales OK"
        except Exception as e:
            telemetria["status"] = f"Error: {e}"
        time.sleep(0.1)

# Iniciamos el hilo igual que en tu script
threading.Thread(target=hilo_lectura, daemon=True).start()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    # Enviamos los datos a la web
    return jsonify({
        "voltaje": telemetria["v"],
        "corriente": telemetria["i"],
        "status": telemetria["status"]
    })

@app.route('/enviar', methods=['POST'])
def enviar():
    valor = request.form.get('valor')
    try:
        # Usamos tu formato de envío: S + valor + \n
        comando = f"S{valor}\n"
        with open(DEVICE, "w") as f:
            f.write(comando)
            f.flush()
        return "OK", 200
    except Exception as e:
        return str(e), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
