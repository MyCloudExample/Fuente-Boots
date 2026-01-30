// Firmware completo corregido para Alan
// Pico + FreeRTOS + LCD I2C + PWM + ADC
// Comentado detalladamente

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
/*===============================================DEFINICIONES====================================================================*/
//===============================================DEFINICIONES PARA EL PWM
#define PWM_PIN     22
#define POT_PIN     26
#define VOUT_PIN    27
#define LED_PIN     25
#define PWM_FREQ    30000
//===============================================DEFINICIONES PARA EL I2C
#define I2C_PORT    i2c0
#define I2C_SDA     4
#define I2C_SCL     5
#define LCD_ADDR    0x27
//===============================================DEFINICIONES PARA LIMITAR EL DUTY
#define DUTY_MIN    58.0f
#define DUTY_MAX    81.0f
//===============================================DEEFINICIONES DE RESISTENCIAS PARA MEDIR LA TENSION DE SALIDA
#define R1          81000.0f
#define R2          9960.0f
#define DIV_RATIO   (R2 / (R1 + R2))
#define BOTON_OK    10
#define D1          13
#define D2          12
#define D3          11
/*===============================================VARIABLES DEL CODIGO============================================================*/
static uint32_t pwm_wrap;
static uint slice_num, channel;
QueueHandle_t duty_queue;
/*===============================================FUNCIONES DEL CODIGO============================================================*/
//===============================================LECTURA DEL ADC0 PARA AJURTAR EL DUTY DEL SETPOINT===============================
float read_potentiometer() {
    adc_select_input(0);
    uint16_t raw = adc_read();
    float duty = DUTY_MIN + ((float)raw / 4095.0f) * (DUTY_MAX - DUTY_MIN);
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    if (duty > DUTY_MAX) duty = DUTY_MAX;
    return duty;
}
//===============================================LECTURA DEL VOLTAJE DE SALIDA POR MEDIO DEL ADC1================================
float read_output_voltage() {
    adc_select_input(1);
    uint16_t raw = adc_read();
    float v_adc = (raw / 4095.0f) * 3.3f;
    float v_out = v_adc / DIV_RATIO;
    return v_out;
}
//===============================================LECTURA DE CORRIENTE============================================================
float read_current ()
{
    adc_select_input(2);
    uint16_t raw = adc_read();
    float v_adc = (raw / 4095.0f) * 3.3f;
    float i = (v_adc)/1000.0f;
}
/*===============================================TAREA DE FREERTOS===============================================================*/
//===============================================CONFIGURA EL SETPOINT===========================================================
void task_potentiometer(void *pv) {
    adc_init();
    adc_gpio_init(POT_PIN); //leo el setpoint
    adc_gpio_init(VOUT_PIN); //leo la tension de salida ADC1
    //Configuro el boton de ok
    gpio_init(BOTON_OK);
    gpio_set_dir(BOTON_OK,false);
    //Configuro el boton para el digito 1
    gpio_init(D1);
    gpio_set_dir(D1,false);
    float decena = 0;
    //Configuro el boton para el digito 2
    gpio_init(D2);
    gpio_set_dir(D2,false);
    float unidad = 0;
    //Configuro el boton para el digito 3, decimal
    gpio_init(D3);
    gpio_set_dir(D3,false);
    float decimal = 0;
    float last_duty = DUTY_MIN;
    while (1) 
    {
        float duty = read_potentiometer();
        //printf("Lectura del potenciometro: %.2f\n",duty);
        // Enviamos siempre el duty sin umbral para que se actualice
        if(gpio_get(D1) == 0)
        {
            decena = decena + 1;
            if(decena > 2)
            {
                decena = 0 ;
            }
            while (!gpio_get(D1))
            {}
            printf("Decena: %0.2f\n",decena);
        }
        if(gpio_get(D2) == 0)
        {
            unidad = unidad + 1;
            if(unidad > 9)
            {
                unidad = 0 ;
            }
            while (!gpio_get(D2))
            {}
            printf("Unidad: %0.2f\n",unidad);
        }
        if(gpio_get(D3) == 0)
        {
            decimal = 5;
            while (!gpio_get(D3))
            {}
            printf("Decimañ: %0.2f\n",decimal);
            
        }
        if(gpio_get(BOTON_OK) == 0)
        {
            float valor = 10*decena + unidad + decimal/10.0f;
            printf("Valor seteado: %0.2f\n",valor);
            while(gpio_get(BOTON_OK)==0)
            {}
            xQueueSend(duty_queue, &duty, 0);
        }
        last_duty = duty;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
//===============================================ESTABLCE EL VALOR DEL PWM UNA VEZ INDICADO =====================================
void task_pwm_control(void *pv) {
    float duty = DUTY_MIN;
    while (1) {
        if (xQueueReceive(duty_queue, &duty, portMAX_DELAY)) {
            uint32_t level = (uint32_t)((duty / 100.0f) * pwm_wrap);
            printf("Valor del Level: %d\n",level);
            pwm_set_chan_level(slice_num, channel, level);
        }
    }
}
//===============================================VIZUALIZA LOS PARAMETROS CONFIGURADOS===========================================
void task_lcd_display(void *pv) {
    char buffer[21];
    float duty = DUTY_MIN;
    float voltage = 0;
    lcd_init(I2C_PORT, LCD_ADDR);
    lcd_clear();
    lcd_set_cursor(0, 0); lcd_string("Fuente Boost 36kHz");
    lcd_set_cursor(1, 0); lcd_string("Duty: 58-81%");
    lcd_set_cursor(2, 0); lcd_string("Vout: 12-24V");
    lcd_set_cursor(3, 0); lcd_string("Iniciando...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    while (1) {
        xQueuePeek(duty_queue, &duty, 0);
        voltage = read_output_voltage();
        lcd_set_cursor(0, 0);
        snprintf(buffer, 21, "Duty: %5.1f%%       ", duty);
        lcd_string(buffer);
        lcd_set_cursor(1, 0);
        snprintf(buffer, 21, "Vout: %5.1f V       ", voltage);
        lcd_string(buffer);
        lcd_set_cursor(2, 0);
        snprintf(buffer, 21, "ADC: %4d           ", adc_read());
        lcd_string(buffer);
        lcd_set_cursor(3, 0);
        snprintf(buffer, 21, "PWM: %4u/%4u       ", (uint32_t)((duty/100.0f)*pwm_wrap), pwm_wrap);
        lcd_string(buffer);
        gpio_xor_mask(1 << LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main() {
    stdio_init_all();
    sleep_ms(1500);
    i2c_init(I2C_PORT, 100000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    channel = pwm_gpio_to_channel(PWM_PIN);
    uint32_t clock = 125000000;
    pwm_wrap = (clock / PWM_FREQ) - 1;
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_config_set_wrap(&config, pwm_wrap);
    pwm_init(slice_num, &config, true);
    uint32_t init_level = (uint32_t)((DUTY_MIN / 100.0f) * pwm_wrap);
    pwm_set_chan_level(slice_num, channel, init_level);
    duty_queue = xQueueCreate(2, sizeof(float));
    xTaskCreate(task_potentiometer, "Pot", 256, NULL, 2, NULL);
    //xTaskCreate(task_pwm_control, "PWM", 256, NULL, 3, NULL);
    //xTaskCreate(task_lcd_display, "LCD", 512, NULL, 1, NULL);
    vTaskStartScheduler();
    while (1) {
        gpio_xor_mask(1 << LED_PIN);
        sleep_ms(500);
    }
}
