#!/bin/bash
# 1. Cargar el driver
sudo insmod egb_uart_chrdev.ko
# 2. Cargar el overlay (el sistema lo busca en /boot/overlays)
sudo dtoverlay egb_uart
# 3. Obtener el Major dinámicamente y crear el nodo
MAJOR=$(awk '$2=="egb" {print $1}' /proc/devices)
if [ -z "$MAJOR" ]; then
    echo "Error: El driver no asignó un Major."
else
    sudo rm -f /dev/egb
    sudo mknod /dev/egb c $MAJOR 0
    sudo chmod 666 /dev/egb
    echo "Listo! Dispositivo /dev/egb creado con Major $MAJOR"
fi
