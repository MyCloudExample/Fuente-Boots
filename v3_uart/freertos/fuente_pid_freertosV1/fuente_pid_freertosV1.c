#include <stdio.h>
#include <math.h> // Necesario para la funcion fabs()
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "lcd.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// --- Configuracion de Hardware ---
#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define LCD_ADDR 0x27

#define POT_PIN 26
#define SALIDA_PIN 27

#define PWM_PIN 15
#define PWM_FREQ 100000 

#define BUZZER_PIN 14

// --- Conversiones y limites ---
#define ADC_MAX 4095.0f
#define VOLTAJE_MAX_ADC 3.3f
#define VOLTAJE_SALIDA_MAX 24.0f
#define VOLTAJE_SALIDA_MIN 12.0f

#define DIVISOR_FACTOR 0.1282f

// --- Coeficientes de Control PID (deben ser ajustados experimentalmente) ---
#define KP 5.0f
#define KI 0.1f
#define KD 1.0f
#define UMBRAL_ERROR 0.5f
#define DT 0.05f // Periodo de muestreo del controlador en segundos (50ms)

// --- Variables y objetos de FreeRTOS ---
static QueueHandle_t xQueueDisplay;
static SemaphoreHandle_t xMutexAdc;

// --- Variables compartidas ---
typedef struct {
    float v_referencia;
    float v_salida;
} DisplayData_t;

// --- Funciones auxiliares ---
float adc_a_voltaje_placa(uint16_t adc_raw) {
    return (float)adc_raw / ADC_MAX * VOLTAJE_MAX_ADC;
}

float adc_a_voltaje_salida(uint16_t adc_raw) {
    if (DIVISOR_FACTOR == 0) return 0.0f;
    return adc_a_voltaje_placa(adc_raw) / DIVISOR_FACTOR;
}

// --- Tarea de Control del PID (vTaskControl) ---
void vTaskControl(void *pvParameters) {
    float error_anterior = 0.0f;
    float error_integral = 0.0f;
    uint16_t wrap = 125000000 / PWM_FREQ - 1;

    DisplayData_t data;

    while (true) {
        // --- 1. Lectura de las entradas ---
        xSemaphoreTake(xMutexAdc, portMAX_DELAY); // Protegemos el ADC con un mutex
        adc_select_input(0);
        uint16_t adc_pot = adc_read();
        adc_select_input(1);
        uint16_t adc_salida = adc_read();
        xSemaphoreGive(xMutexAdc);

        data.v_referencia = adc_a_voltaje_salida(adc_pot);
        data.v_salida = adc_a_voltaje_salida(adc_salida);
        
        // --- 2. Calculo del error de control ---
        float error = data.v_referencia - data.v_salida;

        // --- 3. Logica de control PD/PI ---
        float salida_controlador = 0.0f;
        if (fabs(error) > UMBRAL_ERROR) {
            float error_derivativo = (error - error_anterior) / DT;
            salida_controlador = KP * error + KD * error_derivativo;
            error_integral = 0.0f; // Reset de la integral
        } else {
            error_integral += error * DT;
            salida_controlador = KP * error + KI * error_integral;
        }
        error_anterior = error;

        // --- 4. Conversion a Duty Cycle ---
        float duty_cycle_float = salida_controlador;
        if (duty_cycle_float < 0) duty_cycle_float = 0;
        if (duty_cycle_float > wrap) duty_cycle_float = wrap;
        uint16_t duty_cycle = (uint16_t)duty_cycle_float;
        
        // Ajustamos el PWM
        pwm_set_chan_level(pwm_gpio_to_slice_num(PWM_PIN), pwm_gpio_to_channel(PWM_PIN), duty_cycle);
        
        // Enviamos los datos a la tarea de display
        xQueueSend(xQueueDisplay, &data, 0);

        vTaskDelay(pdMS_TO_TICKS(DT * 1000));
    }
}

// --- Tarea de Display (vTaskDisplay) ---
void vTaskDisplay(void *pvParameters) {
    DisplayData_t data_rx;
    char lcd_str[16];

    while (true) {
        if (xQueueReceive(xQueueDisplay, &data_rx, portMAX_DELAY) == pdPASS) {
            // Logica de alarma y feedback visual
            if (data_rx.v_salida > VOLTAJE_SALIDA_MAX || data_rx.v_salida < VOLTAJE_SALIDA_MIN) {
                gpio_put(BUZZER_PIN, 1);
                sprintf(lcd_str, "ALERTA: %.2f V", data_rx.v_salida);
            } else {
                gpio_put(BUZZER_PIN, 0);
                sprintf(lcd_str, "Salida: %.2f V", data_rx.v_salida);
            }

            // Mostrar en LCD y Monitor Serie
            lcd_set_cursor(1, 0);
            lcd_string(lcd_str);
            printf("Ref: %.2f V | Salida: %.2f V | Frecuencia: %d Hz\n",
                data_rx.v_referencia, data_rx.v_salida, PWM_FREQ);
        }
    }
}

int main() {
    stdio_init_all();
    printf("Iniciando fuente con FreeRTOS...\n");

    // Inicializacion de Hardware
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    lcd_init(I2C_PORT, LCD_ADDR);

    adc_init();
    adc_gpio_init(POT_PIN);
    adc_gpio_init(SALIDA_PIN);

    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    uint16_t wrap = 125000000 / PWM_FREQ - 1;
    pwm_set_wrap(slice_num, wrap);
    pwm_set_enabled(slice_num, true);

    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_string("FreeRTOS OK");

    // Creacion de objetos de FreeRTOS
    xQueueDisplay = xQueueCreate(1, sizeof(DisplayData_t));
    xMutexAdc = xSemaphoreCreateMutex();

    // Creacion de tareas
    xTaskCreate(vTaskControl, "ControlTask", 256, NULL, 2, NULL);
    xTaskCreate(vTaskDisplay, "DisplayTask", 256, NULL, 1, NULL);

    // Inicio del planificador
    vTaskStartScheduler();
    
    // El codigo nunca llega aqui si el planificador inicia correctamente
    while(1){};
    return 0;
}
