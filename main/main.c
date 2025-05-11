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
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ili9341.h"
#include "digitos.h"
#include "driver/gpio.h"
#include "esp_err.h"

/* === Macros definitions =========================================================================================== */

#define DIGITO_ANCHO          60

#define DIGITO_ALTO           100

#define DIGITO_ENCENDIDO      ILI9341_RED

#define DIGITO_APAGADO        0x0000 // 1800

#define DIGITO_FONDO          ILI9341_BLACK

// Defino GPIOs para los botones
#define BOTON1                32
#define BOTON2                35
#define BOTON3                34

// Defino los bits para los eventos de los botones
#define EVENT_BOTON1          (1 << 0)
#define EVENT_BOTON2          (1 << 1)
#define EVENT_BOTON3          (1 << 2)

// Constantes para los botones
#define CANT_MAX_BOTONES      3
#define BOTON1_INDEX          0
#define BOTON2_INDEX          1
#define BOTON3_INDEX          2

// Defino GPIOs para los leds
#define LED_VERDE             GPIO_NUM_4
#define LED_ROJO              GPIO_NUM_2

#define TIEMPOS_PARCIALES_MAX 2

static const char * TAG = "BOTONES";

typedef struct {
    bool cronometro_activo;
    bool reiniciar_display;
    uint16_t cuenta_cronometro;
    EventGroupHandle_t botones_event;
    QueueHandle_t cola_tiempos_parciales;
    SemaphoreHandle_t mutex_tiempos;
} app_data_t;

/* === Private data type declarations =============================================================================== */
typedef enum { BOTON_NINGUNO = 0, BOTON_TEC1, BOTON_TEC2, BOTON_TEC3 } boton_t;

esp_err_t init_botones(void) {
    gpio_config_t boton = {.pin_bit_mask = ((1ULL << BOTON1) | (1ULL << BOTON2) | (1ULL << BOTON3)),
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_ENABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};

    return gpio_config(&boton);
}

esp_err_t init_leds(void) {
    gpio_reset_pin(LED_ROJO);
    gpio_reset_pin(LED_VERDE);

    gpio_config_t leds = {.pin_bit_mask = ((1ULL << LED_ROJO) | (1ULL << LED_VERDE)),
                          .mode = GPIO_MODE_OUTPUT,
                          .pull_up_en = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type = GPIO_INTR_DISABLE};

    gpio_set_level(LED_VERDE, 1);
    gpio_set_level(LED_ROJO, 0);

    return gpio_config(&leds);
}

void tarea_escaneo_botones(void * pvParameters) {
    app_data_t * ctx = (app_data_t *)pvParameters;

    int estado_previo[CANT_MAX_BOTONES] = {1, 1, 1}; // 1 = botón no presionado
    gpio_num_t pines_botones[CANT_MAX_BOTONES] = {BOTON1, BOTON2, BOTON3};

    while (1) {
        for (int i = 0; i < CANT_MAX_BOTONES; i++) {
            int estado_actual = gpio_get_level(pines_botones[i]);
            vTaskDelay(pdMS_TO_TICKS(20)); // Antirrebote
            int estado_confirmado = gpio_get_level(pines_botones[i]);

            if (estado_actual == estado_confirmado) {
                if (estado_confirmado == 0 && estado_previo[i] == 1) {
                    // Se detectó una pulsación nueva
                    switch (i) {
                    case BOTON1_INDEX:
                        xEventGroupSetBits(ctx->botones_event, EVENT_BOTON1);
                        break;
                    case BOTON2_INDEX:
                        xEventGroupSetBits(ctx->botones_event, EVENT_BOTON2);
                        break;
                    case BOTON3_INDEX:
                        xEventGroupSetBits(ctx->botones_event, EVENT_BOTON3);
                        break;
                    }

                    estado_previo[i] = 0;
                }

                // Se suelta el botón, actualiza el estado
                else if (estado_confirmado == 1) {
                    estado_previo[i] = 1;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30)); // Tiempo entre escaneos
    }
}

void tarea_eventos_botones(void * pvParameters) {
    app_data_t * ctx = (app_data_t *)pvParameters;
    EventBits_t eventos;

    while (1) {
        eventos = xEventGroupWaitBits(ctx->botones_event, EVENT_BOTON1 | EVENT_BOTON2 | EVENT_BOTON3, pdTRUE, pdFALSE,
                                      portMAX_DELAY);

        if (eventos & EVENT_BOTON1) {
            xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
            ctx->cronometro_activo = !ctx->cronometro_activo;
            xSemaphoreGive(ctx->mutex_tiempos);
            ESP_LOGI("EVENTO", "botón 1 presionado: cronómetro %s", ctx->cronometro_activo ? "activo" : "detenido");
        }

        if (eventos & EVENT_BOTON2) {
            xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
            if (!ctx->cronometro_activo) {
                ctx->cuenta_cronometro = 0;
                ctx->reiniciar_display = true;
                ESP_LOGI("EVENTO", "botón 2 presionado: se reinició el cronómetro");
            }
            xSemaphoreGive(ctx->mutex_tiempos);
        }

        if (eventos & EVENT_BOTON3) {
            ESP_LOGI("EVENTO", "botón 3 presionado: tiempo parcial capturado");
        }
    }
}

void tarea_leds(void * pvParameters) {
    app_data_t * ctx = (app_data_t *)pvParameters;
    // gpio_set_direction(LED_ROJO, GPIO_MODE_OUTPUT);
    // gpio_set_direction(LED_VERDE, GPIO_MODE_OUTPUT);

    while (1) {
        xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
        bool activo = ctx->cronometro_activo;
        xSemaphoreGive(ctx->mutex_tiempos);

        if (activo) {
            gpio_set_level(LED_ROJO, 0);
            gpio_set_level(LED_VERDE, 1);
            vTaskDelay(pdMS_TO_TICKS(250));
            gpio_set_level(LED_VERDE, 0);
            vTaskDelay(pdMS_TO_TICKS(250));
        }

        else {
            gpio_set_level(LED_ROJO, 1);
            gpio_set_level(LED_VERDE, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void tarea_display(void * pvParameters) {
    app_data_t * ctx = (app_data_t *)pvParameters;

    panel_t p_decenas =
        CrearPanel(30, 60, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t p_unidades =
        CrearPanel(95, 60, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t p_decimas =
        CrearPanel(170, 60, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    // Variables para utilizar DelayUntil() y contar cada 100 milisegundo
    const TickType_t tiempo = pdMS_TO_TICKS(100);
    TickType_t last_time = xTaskGetTickCount();

    /* Variables para dibujar los dígitos solamente si cambian */
    int decenas_segundos_anterior = -1;
    int unidades_segundos_anterior = -1;
    int decimas_segundos_anterior = -1;

    // Cuenta máxima de 60 segundos
    const int cuenta_maxima = 600;

    while (1) {
        xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
        if (ctx->reiniciar_display) {
            DibujarDigito(p_decenas, 0, 0);
            DibujarDigito(p_unidades, 0, 0);
            DibujarDigito(p_decimas, 0, 0);
            ctx->reiniciar_display = false;
            // vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (ctx->cronometro_activo) {
            ctx->cuenta_cronometro++;
            if (ctx->cuenta_cronometro >= cuenta_maxima) {
                ctx->cuenta_cronometro = 0;
            }

            // Variables para los dígitos del cronómetro
            int decenas_segundos = (ctx->cuenta_cronometro / 100) % 10;
            int unidades_segundos = (ctx->cuenta_cronometro / 10) % 10;
            int decimas_segundos = (ctx->cuenta_cronometro % 10);

            if (decenas_segundos_anterior != decenas_segundos) {
                DibujarDigito(p_decenas, 0, decenas_segundos);
                decenas_segundos_anterior = decenas_segundos;
            }

            if (unidades_segundos_anterior != unidades_segundos) {
                DibujarDigito(p_unidades, 0, unidades_segundos);
                unidades_segundos_anterior = unidades_segundos;
            }

            if (decimas_segundos_anterior != decimas_segundos) {
                DibujarDigito(p_decimas, 0, decimas_segundos);
                decimas_segundos_anterior = decimas_segundos;
            }
        }

        xSemaphoreGive(ctx->mutex_tiempos);
        vTaskDelayUntil(&last_time, tiempo);
    }
}

/* === Private variable declarations ================================================================================ */

/* === Private function declarations ================================================================================ */

/* === Public variable definitions ================================================================================== */

/* === Private variable definitions ================================================================================= */

/* === Private function implementation ============================================================================== */

/* === Public function implementation =============================================================================== */

void app_main(void) {
    static app_data_t ctx = {.cronometro_activo = false, .cuenta_cronometro = 0, .reiniciar_display = false};

    ctx.botones_event = xEventGroupCreate();
    ctx.mutex_tiempos = xSemaphoreCreateMutex();
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    if (init_botones() == ESP_OK) {
        xTaskCreate(tarea_escaneo_botones, "Escaneo botones", 2048, &ctx, 3, NULL);
    }

    else {
        ESP_LOGE(TAG, "Error al inicializar botones");
    }

    if (init_leds() == ESP_OK) {
        xTaskCreate(tarea_leds, "Tarea Leds", 2048, &ctx, 1, NULL);
    }

    else {
        ESP_LOGE("LED", "Error al inicializar Leds");
    }

    xTaskCreate(tarea_eventos_botones, "Eventos botones", 2048, &ctx, 4, NULL);

    panel_t decenas_segundos_inicial =
        CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t unidades_segundos_inicial =
        CrearPanel(170, 60, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    DibujarDigito(decenas_segundos_inicial, 0, 0);
    DibujarDigito(decenas_segundos_inicial, 1, 0);

    /*
    ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
    ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);

    USAR PARA EL RELOJ, Se usa para dibujar los dos puntos que separan las horas de los minutos */

    ILI9341DrawFilledCircle(160, 150, 5, DIGITO_ENCENDIDO);

    DibujarDigito(unidades_segundos_inicial, 0, 0);
    DibujarDigito(unidades_segundos_inicial, 1, 0);

    xTaskCreate(tarea_display, "Tarea cronómetro", 2048, &ctx, 6, NULL);
}

/* === End of documentation ========================================================================================= */

/** @} End of module definition for doxygen */
