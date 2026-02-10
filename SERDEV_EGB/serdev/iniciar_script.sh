#!/bin/bash

echo "--- Iniciando configuración de EGB UART ---"

# 1. Limpiar por si acaso hubo una carga previa fallida
sudo dtoverlay -r egb_uart 2>/dev/null
sudo rmmod egb_uart_chrdev 2>/dev/null
sudo rm -f /dev/egb

# 2. Cargar el módulo del driver
sudo insmod egb_uart_chrdev.ko
if [ $? -eq 0 ]; then
    echo "[OK] Módulo cargado correctamente."
else
    echo "[ERROR] No se pudo cargar el módulo .ko"
    exit 1
fi

# 3. Cargar el Device Tree Overlay
sudo dtoverlay egb_uart.dtbo
if [ $? -eq 0 ]; then
    echo "[OK] Overlay aplicado correctamente."
else
    echo "[ERROR] No se pudo aplicar el overlay .dtbo"
    exit 1
fi

# 4. Obtener el Major dinámico asignado por el Kernel
# Buscamos 'egb' en /proc/devices y tomamos el número
MAJOR=$(awk '$2=="egb" {print $1}' /proc/devices)

if [ -z "$MAJOR" ]; then
    echo "[ERROR] El dispositivo no aparece en /proc/devices. Revisa dmesg."
    exit 1
else
    # 5. Crear el nodo en /dev y dar permisos
    sudo mknod /dev/egb c $MAJOR 0
    sudo chmod 666 /dev/egb
    echo "[OK] Dispositivo /dev/egb creado con Major: $MAJOR"
    echo "--- Sistema listo para usar con Python o Web ---"
fi
