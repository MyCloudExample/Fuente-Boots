savedcmd_/home/wilson/EGB/serdev/egb_uart_chrdev.mod := printf '%s\n'   egb_uart_chrdev.o | awk '!x[$$0]++ { print("/home/wilson/EGB/serdev/"$$0) }' > /home/wilson/EGB/serdev/egb_uart_chrdev.mod
