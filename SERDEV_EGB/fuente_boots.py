import tkinter as tk
from tkinter import messagebox
import serial
import threading
import time

#AZUL  TX
#VERDE RX
#NEGRO MASA
class BoostInterface:
    def __init__(self, master):
        self.master = master
        master.title("Control PID Boost - RPi4 a Pico 2")
        master.geometry("450x450")
        
        # --- Configuración de Límites ---
        self.VOLT_MIN = 12.0
        self.VOLT_MAX = 24.0

        # --- Variables ---
        self.ser = None
        self.running = False
        self.voltage_set = tk.DoubleVar(value=self.VOLT_MIN)

        # --- UI Elements ---
        tk.Label(master, text="Panel de Control Fuente Boost", font=("Arial", 16, "bold")).pack(pady=10)

        # Monitor de datos (Lectura de la Pico 2)
        frame_monitor = tk.LabelFrame(master, text=" Monitoreo en Tiempo Real ", padx=10, pady=10)
        frame_monitor.pack(pady=10, fill="x", padx=20)

        self.lbl_v = tk.Label(frame_monitor, text="Vout: 0.00 V", font=("Consolas", 22), fg="blue")
        self.lbl_v.pack()
        self.lbl_i = tk.Label(frame_monitor, text="Iout: 0.00 A", font=("Consolas", 22), fg="green")
        self.lbl_i.pack()

        # Control de Setpoint
        frame_control = tk.LabelFrame(master, text=" Ajuste de Tensión (Setpoint) ", padx=10, pady=10)
        frame_control.pack(pady=10, fill="x", padx=20)

        tk.Label(frame_control, text=f"Rango permitido: {self.VOLT_MIN}V a {self.VOLT_MAX}V").pack()
        
        # Slider para ajuste rápido
        self.slider = tk.Scale(frame_control, from_=self.VOLT_MIN, to=self.VOLT_MAX, 
                              orient=tk.HORIZONTAL, resolution=0.1, variable=self.voltage_set,
                              length=300)
        self.slider.pack(pady=5)

        # Entrada numérica para ajuste fino
        self.ent_set = tk.Entry(frame_control, textvariable=self.voltage_set, width=10)
        self.ent_set.pack(pady=5)
        
        self.btn_send = tk.Button(frame_control, text="Actualizar Setpoint", 
                                 command=self.send_setpoint, bg="#e1ebe1")
        self.btn_send.pack(pady=5)
        
        # Estado de conexión
        self.btn_conn = tk.Button(master, text="Conectar UART", command=self.toggle_connection, height=2)
        self.btn_conn.pack(side="bottom", fill="x", padx=20, pady=20)

    def toggle_connection(self):
        if not self.ser:
            try:
                # En RPi4, /dev/ttyS0 suele ser el puerto GPIO
                self.ser = serial.Serial('/dev/ttyS0', 115200, timeout=1)
                self.running = True
                self.btn_conn.config(text="DESCONECTAR", bg="red", fg="white")
                threading.Thread(target=self.read_uart, daemon=True).start()
            except Exception as e:
                messagebox.showerror("Error de Conexión", f"No se pudo abrir el puerto:\n{e}")
        else:
            self.running = False
            if self.ser: self.ser.close()
            self.ser = None
            self.btn_conn.config(text="CONECTAR UART", bg="SystemButtonFace", fg="black")

    def send_setpoint(self):
        try:
            val = float(self.voltage_set.get())
            # Validación de límites por software
            if self.VOLT_MIN <= val <= self.VOLT_MAX:
                if self.ser and self.ser.is_open:
                    mensaje = f"S{val:.2f}\n" # Formato: S15.50
                    self.ser.write(mensaje.encode())
                    print(f"Enviado: {mensaje}")
                else:
                    messagebox.showwarning("UART", "No conectado. Conecta el puerto primero.")
            else:
                messagebox.showerror("Límite excedido", f"El valor debe estar entre {self.VOLT_MIN}V y {self.VOLT_MAX}V")
        except ValueError:
            messagebox.showerror("Error", "Por favor ingresa un número válido.")

    def read_uart(self):
        while self.running:
            if self.ser and self.ser.in_waiting > 0:
                try:
                    line = self.ser.readline().decode('utf-8').strip()
                    # Protocolo esperado: V:XX.X,I:X.X
                    if line.startswith("V:"):
                        parts = line.split(',')
                        v_val = parts[0].split(':')[1]
                        i_val = parts[1].split(':')[1]
                        
                        # Actualizar UI desde el hilo principal de forma segura
                        self.master.after(0, self.update_labels, v_val, i_val)
                except Exception as e:
                    print(f"Error de lectura: {e}")
            time.sleep(0.05) # Muestreo de 20Hz aprox.

    def update_labels(self, v, i):
        self.lbl_v.config(text=f"Vout: {v} V")
        self.lbl_i.config(text=f"Iout: {i} A")

if __name__ == "__main__":
    root = tk.Tk()
    app = BoostInterface(root)
    root.mainloop()
