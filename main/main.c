/************************************************************************************************
Copyright (c) 2022-2023, Laboratorio de Microprocesadores
Facultad de Ciencias Exactas y Tecnología, Universidad Nacional de Tucumán
https://www.microprocesadores.unt.edu.ar/

Copyright (c) 2022-2023, Esteban Volentini <evolentini@herrera.unt.edu.ar>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

SPDX-License-Identifier: MIT
*************************************************************************************************/

/** @file blinking.c
 **
 ** @brief Ejemplo de un led parpadeando
 **
 ** Ejemplo de un led parpadeando utilizando la capa de abstraccion de
 ** hardware y sin sistemas operativos.
 **
 ** | RV | YYYY.MM.DD | Autor       | Descripción de los cambios              |
 ** |----|------------|-------------|-----------------------------------------|
 ** |  3 | 2025.03.12 | evolentini  | Adaptación para plataforma ESP-32       |
 ** |  2 | 2017.10.16 | evolentini  | Correción en el formato del archivo     |
 ** |  1 | 2017.09.21 | evolentini  | Version inicial del archivo             |
 **
 ** @defgroup ejemplos Proyectos de ejemplo
 ** @brief Proyectos de ejemplo de la Especialización en Sistemas Embebidos
 ** @{
 */

/* === Headers files inclusions ==================================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ili9341.h"
#include "digitos.h"
#include "driver/gpio.h"
#include "esp_err.h"

/* === Macros definitions =========================================================================================== */

#define DIGITO_ANCHO     60

#define DIGITO_ALTO      100

#define DIGITO_ENCENDIDO ILI9341_RED

#define DIGITO_APAGADO   0x1800

#define DIGITO_FONDO     ILI9341_BLACK

// Defino GPIOs para los botones
#define TEC1_GPIO   32
#define TEC2_GPIO   35
#define TEC3_GPIO   34

// Defino GPIOs para los leds
#define LED_VERDE   GPIO_NUM_4
#define LED_ROJO    GPIO_NUM_2

static const char *TAG = "BOTONES";
volatile bool cronometro_activo = false;

/* === Private data type declarations =============================================================================== */
typedef enum 
{
    BOTON_NINGUNO = 0, 
    BOTON_TEC1, 
    BOTON_TEC2, 
    BOTON_TEC3 
} boton_t;

// Inicializo los botones
esp_err_t init_botones(void)
{
    gpio_config_t boton = {
        .pin_bit_mask = ((1ULL << TEC1_GPIO) | (1ULL << TEC2_GPIO) | (1ULL << TEC3_GPIO)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&boton);
}
/*
esp_err_t init_leds(void)
{
    gpio_config_t leds = {
        .pin_bit_mask = ((1ULL << LED_ROJO) | (LED_VERDE)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&leds);

    gpio_set_level(LED_VERDE, 1);
    gpio_set_level(LED_ROJO, 0);

    return ret;
}
*/
void tarea_escaneo_botones(void *pvParameters)
{
    boton_t estado_previo = BOTON_NINGUNO;

    while (1)
    {
        boton_t boton_actual = BOTON_NINGUNO;

        if (gpio_get_level(TEC1_GPIO) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(50));

            if (gpio_get_level(TEC1_GPIO) == 0)
            {
                boton_actual = BOTON_TEC1;
            }
        }

        else if (gpio_get_level(TEC2_GPIO) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(50));

            if (gpio_get_level(TEC2_GPIO) == 0)
            {
                boton_actual = BOTON_TEC2;
            }
        }

        else if (gpio_get_level(TEC3_GPIO) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(50));

            if (gpio_get_level(TEC3_GPIO) == 0)
            {
                boton_actual = BOTON_TEC3;
            }
        }

        if (boton_actual != BOTON_NINGUNO && boton_actual != estado_previo)
        {
            switch(boton_actual)
            {
                case BOTON_TEC1:
                    cronometro_activo = !cronometro_activo;
                    ESP_LOGI(TAG, "Botón 1 presionado: cronómetro %s", cronometro_activo ? "ACTIVO" : "DETENIDO");
                    break;
                case BOTON_TEC2:
                    ESP_LOGI(TAG, "Se presiono el boton 2");
                    break;
                case BOTON_TEC3:
                    ESP_LOGI(TAG, "Se presiono el boton 3");
                    break;
                default:
                    break;
            }
            
            estado_previo = boton_actual;
        }

        if (boton_actual == BOTON_NINGUNO)
        {
            estado_previo = BOTON_NINGUNO;
        }
        vTaskDelay(pdMS_TO_TICKS(100));     // Escaneo cada 100 ms
    }
}

void tarea_leds(void *pvParameters)
{
    gpio_set_direction(LED_ROJO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_VERDE, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_ROJO, 1);
    gpio_set_level(LED_VERDE, 0);

    while (1)
    {
        if (cronometro_activo)
        {
            gpio_set_level(LED_ROJO, 0);
            gpio_set_level(LED_VERDE, 1);
            vTaskDelay(pdMS_TO_TICKS(250));
            gpio_set_level(LED_VERDE, 0);
            vTaskDelay(pdMS_TO_TICKS(250));
        }

        else
        {
            gpio_set_level(LED_ROJO, 1);
            gpio_set_level(LED_VERDE, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
       
}
/* === Private variable declarations ================================================================================ */

/* === Private function declarations ================================================================================ */

/* === Public variable definitions ================================================================================== */

/* === Private variable definitions ================================================================================= */

/* === Private function implementation ============================================================================== */

/* === Public function implementation =============================================================================== */

void app_main(void) {
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);


    if (init_botones() == ESP_OK)
    {
        xTaskCreate(tarea_escaneo_botones, "Tarea Botones", 2048, NULL, 5, NULL);
    }

    else
    {
        ESP_LOGE("MAIN", "Error al inicializar botones");
    }

    /*
    if (init_leds() == ESP_OK)
    {
        xTaskCreate(tarea_leds, "Tarea Leds", 2048, NULL, 5, NULL);
    }

    else
    {
        ESP_LOGE("MAIN", "Error al inicializar los leds");
    }
    */

    xTaskCreate(tarea_leds, "Tarea Leds", 2048, NULL, 5, NULL);
    

    panel_t horas = CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t minutos = CrearPanel(170, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    DibujarDigito(horas, 0, 1);
    DibujarDigito(horas, 1, 2);

    ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
    ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);

    DibujarDigito(minutos, 0, 3);
    DibujarDigito(minutos, 1, 4);
}

/* === End of documentation ========================================================================================= */

/** @} End of module definition for doxygen */
