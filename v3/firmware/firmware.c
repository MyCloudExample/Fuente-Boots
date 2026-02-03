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
#define Rsensado    1
#define GAIN_OPAM   10
//===============================================DEFINICIONES PARA EL CONTROL PID================================================
/*float Kp = 1.5f; 
float Ki = 0.05f;
float Kd = 0.01f;
float error_acumulado = 0;
float ultimo_error = 0;*/
/*===============================================VARIABLES DEL CODIGO============================================================*/
static uint32_t pwm_wrap;
static uint slice_num, channel;
typedef struct
{
    float valor;//Valor seteado
    float decena;//Digito 1
    float unidad;//Digito 2
    float decimal;//Digito 3
    uint32_t v_pwm; //Valor del PWM para una tension especificada
    float porcentaje_pwm; //Valor del PWM expresado en porcentaje
    uint8_t hoja; //Seleccion de presentacion en pantalla
    char error; //Indicador visual para indicar seteo fuera de rango (12V-24V)
}dato_t;
float corriente_filtrada = 0;
QueueHandle_t duty_queue; //Envia datos desde task_configuracion a task_lcd_dispaly
QueueHandle_t queue_control; //Envia datos desde task_configuracion a task_pwm_control
/*===============================================FUNCIONES DEL CODIGO============================================================*/
//===============================================LECTURA DEL ADC0 PARA AJURTAR EL DUTY DEL SETPOINT===============================
float read_potentiometer() 
{
    adc_select_input(0);
    uint16_t raw = adc_read();
    float duty = DUTY_MIN + ((float)raw / 4095.0f) * (DUTY_MAX - DUTY_MIN);
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    if (duty > DUTY_MAX) duty = DUTY_MAX;
    return duty;
}
//===============================================CONVIERTE TENSION (12.0 - 24.0) A NIVEL DE PWM (BASADO EN CICLO 58% - 81%)
uint32_t calcular_nivel_pwm(float v_deseado, dato_t *aux) 
{
    // 1. Limitar el valor por seguridad (Clamping)
    aux->error = ' ';
    if (v_deseado < 12.0f)
    {
        v_deseado = 12.0f;
        aux->error = '*';
    } 
    if (v_deseado > 24.0f)
    {
        v_deseado = 24.0f;
        aux->error = '*';
    }
    // 2. Mapeo: 12V -> 58% Duty, 24V -> 81% Duty
    // Fórmula: Duty = Min_D + (V_in - 12) * (Rango_D / Rango_V)
    float duty = DUTY_MIN + (v_deseado - 12.0f) * (DUTY_MAX - DUTY_MIN) / (24.0f - 12.0f);
    // 3. Convertir porcentaje a valor de registro (0 a pwm_wrap)
    float duty_percent = DUTY_MIN + (v_deseado - 12.0f) * (DUTY_MAX - DUTY_MIN) / (24.0f - 12.0f);
    aux->porcentaje_pwm = duty_percent;
    //printf("PWM porcetanje:%.2f\n",duty_percent);
    return (uint32_t)((duty / 100.0f) * pwm_wrap);
}
//===============================================LECTURA DEL VOLTAJE DE SALIDA POR MEDIO DEL ADC1================================
float read_output_voltage() 
{
    adc_select_input(1);
    uint16_t raw = adc_read();
    float v_adc = (raw / 4095.0f) * 3.3f;
    float v_out = (v_adc / DIV_RATIO) - 0.20f;
    return v_out;
}
//===============================================LECTURA DE CORRIENTE============================================================
float read_output_current() 
{
    float suma_v = 0;
    int muestras = 20; // Promediamos 20 lecturas rápidas
    
    for(int i = 0; i < muestras; i++) 
    {
        adc_select_input(2); 
        suma_v += (adc_read() * 3.3f) / 4095.0f;
    }
    float v_adc = suma_v / muestras;

    // Usamos tu fórmula ajustada que ya funciona:
    float offset_voltaje = 0.666f; 
    float corriente_ma = (v_adc - offset_voltaje) * 1350.0f + 60.0f;

    if (corriente_ma < 0) corriente_ma = 0;
    if (read_output_voltage() < 1.0f) return 0.0f; // Si no hay voltaje, no hay corriente

    return corriente_ma / 1000.0f;
}
/*===============================================TAREA DE FREERTOS===============================================================*/
//===============================================CONFIGURA EL SETPOINT===========================================================
void task_configuracion(void *pv) 
{
    dato_t setpoint={.hoja=1};
    dato_t aux;
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
            xQueueReceive(duty_queue,&aux,0); //Borra la cola
            decena = decena + 1;
            if(decena > 2)
            {
                decena = 0 ;
            }
            while (!gpio_get(D1))
            {}
            setpoint.decena = decena;
            setpoint.valor = 10*decena;
            setpoint.hoja = 2;
            xQueueSend(duty_queue,&setpoint,portMAX_DELAY);
            //printf("Decena: %0.2f\n",decena);
        }
        if(gpio_get(D2) == 0)
        {
            xQueueReceive(duty_queue,&aux,0);
            unidad = unidad + 1;
            if(unidad > 9)
            {
                unidad = 0 ;
            }
            while (!gpio_get(D2))
            {}
            if(10*decena+unidad < 12)
            {
                unidad = 2;
            }
            setpoint.unidad = unidad;
            setpoint.valor = 10*decena + unidad;
            setpoint.hoja = 2;
            xQueueSend(duty_queue,&setpoint,portMAX_DELAY);
            //printf("Unidad: %0.2f\n",unidad);
        }
        if(gpio_get(D3) == 0)
        {
            xQueueReceive(duty_queue,&aux,0);
            decimal = decimal + 5;
            if(decimal > 5)
            {
                decimal = 0;
            }
            while (!gpio_get(D3))
            {}
            setpoint.decimal = decimal;
            setpoint.valor = 10*decena + unidad + (decimal/10);
            setpoint.hoja = 2;
            xQueueSend(duty_queue,&setpoint,portMAX_DELAY);
            //printf("Decimañ: %0.2f\n",decimal);
            
        }
        if(gpio_get(BOTON_OK) == 0)
        {
            xQueueReceive(duty_queue,&aux,0);
            setpoint.valor = 10*decena + unidad + (decimal/10.0f);
            setpoint.v_pwm = calcular_nivel_pwm(setpoint.valor, &setpoint);
            setpoint.hoja = 1;
            while(gpio_get(BOTON_OK)==0)
            {}
            //printf("Valor de PWM: %u\n",setpoint.v_pwm);
            xQueueSend(duty_queue, &setpoint, portMAX_DELAY);
            xQueueSend(queue_control,&setpoint,portMAX_DELAY);
            decena = 0;
            unidad = 0;
            decimal = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
//===============================================ESTABLCE EL VALOR DEL PWM UNA VEZ INDICADO =====================================
void task_pwm_control(void *pv) 
{
    dato_t config_recibida;
    // IMPORTANTE: Inicializar valores por defecto para que no haya basura
    config_recibida.valor = 0.0f; 
    config_recibida.porcentaje_pwm = DUTY_MIN;

    float current_duty = DUTY_MIN; 
    float error_acumulado = 0;
    const float Kp = 0.15f; 
    const float Ki = 0.005f; 

    while (1) 
    {
        // Intentar recibir datos. Si no hay, no pasa nada, seguimos con los anteriores.
        xQueueReceive(queue_control, &config_recibida, 0);

        float v_real = read_output_voltage();
        float v_objetivo = config_recibida.valor;

        // Solo activar el PID si el usuario ya configuró un voltaje válido
        if (v_objetivo >= 12.0f) 
        {
            float error = v_objetivo - v_real;
            error_acumulado += error;
            
            if(error_acumulado > 10.0f) error_acumulado = 10.0f;
            if(error_acumulado < -10.0f) error_acumulado = -10.0f;

            float ajuste = (Kp * error) + (Ki * error_acumulado);
            current_duty += ajuste;

            if (current_duty > DUTY_MAX) current_duty = DUTY_MAX;
            if (current_duty < DUTY_MIN) current_duty = DUTY_MIN;

            uint32_t level = (uint32_t)((current_duty / 100.0f) * pwm_wrap);
            pwm_set_chan_level(slice_num, channel, level);
            
            // Actualizamos la global para que el LCD pueda leerla
            config_recibida.v_pwm = level;
            config_recibida.porcentaje_pwm = current_duty; 
        }
        else 
        {
            // Si no hay consigna, PWM al mínimo por seguridad
            pwm_set_chan_level(slice_num, channel, (uint32_t)((DUTY_MIN / 100.0f) * pwm_wrap));
        }

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}
//===============================================VIZUALIZA LOS PARAMETROS CONFIGURADOS===========================================
void task_lcd_display(void *pv) 
{
    char buffer[21];
    dato_t setpoint;
    uint8_t hoja=0;
    float duty = DUTY_MIN;
    float voltage = 0;
    float valor;
    float current;
    lcd_init(I2C_PORT, LCD_ADDR);
    lcd_clear();
    lcd_set_cursor(0, 0); lcd_string("Fuente Boost 36kHz");
    lcd_set_cursor(1, 0); lcd_string("Duty: 58-81%");
    lcd_set_cursor(2, 0); lcd_string("Vout: 12-24V");
    lcd_set_cursor(3, 0); lcd_string("Iniciando...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    lcd_clear();
    lcd_set_cursor(0,0);
    lcd_string("Seteo de tension");
    lcd_set_cursor(1,0);
    lcd_string("Pulse el Digito 1");
    lcd_set_cursor(2,0);
    lcd_string("para iniciar");
    while (1) 
    {
        xQueuePeek(duty_queue, &setpoint, 0);
        //printf("Hoja:%.2f\n",setpoint.hoja);
        hoja = setpoint.hoja;
        switch (hoja)
        {
        case 1:
            voltage = read_output_voltage();
            current = read_output_current();
            corriente_filtrada = (corriente_filtrada * 0.9f) + (current * 0.1f);
            lcd_set_cursor(0, 0);
            snprintf(buffer, 21, "Vs:%.2f %c             ", setpoint.valor, setpoint.error);
            lcd_string(buffer);
            lcd_set_cursor(1, 0);
            snprintf(buffer, 21, "Vout: %.2f V            ", voltage);
            lcd_string(buffer);
            lcd_set_cursor(2, 0);
            snprintf(buffer, 21, "I:%.3f A               ", current);
            lcd_string(buffer);
            lcd_set_cursor(3, 0);
            snprintf(buffer, 21, "PWM:%.2f       ",setpoint.porcentaje_pwm);
            lcd_string(buffer);
            break;
        case 2:
            lcd_set_cursor(0, 0);
            snprintf(buffer, 21, "Vs:%.2f             ", setpoint.valor);
            lcd_string(buffer);
            lcd_set_cursor(1,0);
            lcd_string("Vs=12V si Vs<12V    ");
            lcd_set_cursor(2,0);
            lcd_string("Vs=24V si Vs>24V    ");
            break;
        default:
            break;
        }
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
    duty_queue = xQueueCreate(2, sizeof(dato_t));
    queue_control = xQueueCreate(1,sizeof(dato_t));
    xTaskCreate(task_configuracion, "Pot", 512, NULL, 2, NULL);
    xTaskCreate(task_pwm_control, "PWM", 512, NULL, 2, NULL);
    xTaskCreate(task_lcd_display, "LCD", 512, NULL, 2, NULL);
    vTaskStartScheduler();
    while (1) {
        gpio_xor_mask(1 << LED_PIN);
        sleep_ms(500);
    }
}
