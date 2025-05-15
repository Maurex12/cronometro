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

#define DIGITO_ANCHO             60
#define DIGITO_ALTO              100
#define DIGITO_ENCENDIDO         ILI9341_RED
#define DIGITO_APAGADO           0x0000
#define DIGITO_FONDO             ILI9341_BLACK
#define DIGITO_ENCENDIDO_PARCIAL ILI9341_LIGHTGREY

#define BOTON1                   32
#define BOTON2                   35
#define BOTON3                   34

#define EVENT_BOTON1             (1 << 0)
#define EVENT_BOTON2             (1 << 1)
#define EVENT_BOTON3             (1 << 2)

#define LED_VERDE                GPIO_NUM_4
#define LED_ROJO                 GPIO_NUM_2

#define CUENTA_MAXIMA            600
#define MAX_TIEMPOS_PARCIALES    2

static const char * TAG = "CRONOMETRO";

/* === Private data type declarations =============================================================================== */

typedef struct {
    volatile bool cronometro_activo;
    volatile bool reiniciar_display;
    volatile uint16_t cuenta_cronometro;
    EventGroupHandle_t botones_event;
    QueueHandle_t cola_tiempos_parciales;
    SemaphoreHandle_t mutex_tiempos;
} app_data_t;

esp_err_t init_botones(void) {
    gpio_config_t boton = {.pin_bit_mask = (1ULL << BOTON1) | (1ULL << BOTON2) | (1ULL << BOTON3),
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_ENABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};
    return gpio_config(&boton);
}

esp_err_t init_leds(void) {
    gpio_reset_pin(LED_ROJO);
    gpio_reset_pin(LED_VERDE);

    gpio_config_t leds = {.pin_bit_mask = (1ULL << LED_ROJO) | (1ULL << LED_VERDE),
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
    int estado_previo[3] = {1, 1, 1};
    gpio_num_t pines[3] = {BOTON1, BOTON2, BOTON3};

    while (1) {
        for (int i = 0; i < 3; i++) {
            int estado_actual = gpio_get_level(pines[i]);
            vTaskDelay(pdMS_TO_TICKS(20));
            int estado_confirmado = gpio_get_level(pines[i]);

            if (estado_actual == estado_confirmado && estado_confirmado == 0 && estado_previo[i] == 1) {
                xEventGroupSetBits(ctx->botones_event, (1 << i));
                estado_previo[i] = 0;
            }
            if (estado_confirmado == 1)
                estado_previo[i] = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
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
            ESP_LOGI(TAG, "Botón 1: Cronómetro %s", ctx->cronometro_activo ? "activo" : "detenido");
        }

        if (eventos & EVENT_BOTON2) {
            xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
            if (!ctx->cronometro_activo) {
                ctx->cuenta_cronometro = 0;
                ctx->reiniciar_display = true;
                xQueueReset(ctx->cola_tiempos_parciales);
                ESP_LOGI(TAG, "Botón 2: Cronómetro reiniciado");
            }
            xSemaphoreGive(ctx->mutex_tiempos);
        }

        if (eventos & EVENT_BOTON3) {
            xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
            uint16_t tiempo_parcial = ctx->cuenta_cronometro;
            xSemaphoreGive(ctx->mutex_tiempos);

            if (uxQueueSpacesAvailable(ctx->cola_tiempos_parciales) == 0) {
                uint16_t descarte;
                xQueueReceive(ctx->cola_tiempos_parciales, &descarte, 0);
            }
            xQueueSend(ctx->cola_tiempos_parciales, &tiempo_parcial, 0);
            ESP_LOGI(TAG, "Botón 3: Tiempo parcial capturado %d", tiempo_parcial);
        }
    }
}

void tarea_leds(void * pvParameters) {
    app_data_t * ctx = (app_data_t *)pvParameters;
    bool ultimo_estado = false;

    while (1) {
        xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
        bool activo = ctx->cronometro_activo;
        xSemaphoreGive(ctx->mutex_tiempos);

        if (activo != ultimo_estado) {
            gpio_set_level(LED_VERDE, activo ? 1 : 0);
            gpio_set_level(LED_ROJO, activo ? 0 : 1);
            ultimo_estado = activo;
        }

        if (activo) {
            vTaskDelay(pdMS_TO_TICKS(250));
            gpio_set_level(LED_VERDE, 0);
            vTaskDelay(pdMS_TO_TICKS(250));
            gpio_set_level(LED_VERDE, 1);
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void tarea_display(void * pvParameters) {
    app_data_t * ctx = (app_data_t *)pvParameters;

    panel_t p_decenas =
        CrearPanel(30, 40, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t p_unidades =
        CrearPanel(95, 40, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t p_decimas =
        CrearPanel(170, 40, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    panel_t p_parcial[MAX_TIEMPOS_PARCIALES];
    for (int i = 0; i < MAX_TIEMPOS_PARCIALES; i++) {
        p_parcial[i] = CrearPanel(30, 140 + (i * 40), 4, DIGITO_ALTO / 2, DIGITO_ANCHO / 2, DIGITO_ENCENDIDO_PARCIAL,
                                  DIGITO_APAGADO, DIGITO_FONDO);
    }

    const TickType_t tiempo = pdMS_TO_TICKS(100);
    TickType_t last_time = xTaskGetTickCount();

    /* Variables para dibujar los dígitos solomente si cambian */
    int decenas_segundos_anterior = -1;
    int unidades_segundos_anterior = -1;
    int decimas_segundos_anterior = -1;

    while (1) {
        xSemaphoreTake(ctx->mutex_tiempos, portMAX_DELAY);
        if (ctx->reiniciar_display) {
            DibujarDigito(p_decenas, 0, 0);
            DibujarDigito(p_unidades, 0, 0);
            DibujarDigito(p_decimas, 0, 0);
            for (int i = 0; i < MAX_TIEMPOS_PARCIALES; i++) {
                DibujarDigito(p_parcial[i], 0, 0);
                DibujarDigito(p_parcial[i], 1, 0);
                DibujarDigito(p_parcial[i], 2, 0);
            }
            ctx->reiniciar_display = false;
        }

        if (ctx->cronometro_activo) {
            ctx->cuenta_cronometro++;
            if (ctx->cuenta_cronometro >= CUENTA_MAXIMA) // 59.9 segundos como máximo
                ctx->cuenta_cronometro = 0;
        }
        uint16_t cuenta = ctx->cuenta_cronometro;
        xSemaphoreGive(ctx->mutex_tiempos);

        int decenas_segundos = (cuenta / 100) % 10;
        int unidades_segundos = (cuenta / 10) % 10;
        int decimas_segundos = cuenta % 10;

        if (decenas_segundos_anterior != decenas_segundos) {
            DibujarDigito(p_decenas, 0, decenas_segundos);
            decenas_segundos_anterior = decenas_segundos;
        }

        if (unidades_segundos_anterior != unidades_segundos) {
            DibujarDigito(p_unidades, 0, unidades_segundos);
            unidades_segundos_anterior = unidades_segundos;
        }

        ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);

        if (decimas_segundos_anterior != decimas_segundos) {
            DibujarDigito(p_decimas, 0, decimas_segundos);
            decimas_segundos_anterior = decenas_segundos;
        }

        uint16_t tiempos_parciales[MAX_TIEMPOS_PARCIALES] = {0};
        int cantidad = uxQueueMessagesWaiting(ctx->cola_tiempos_parciales);

        for (int i = 0; i < cantidad; i++) {
            xQueueReceive(ctx->cola_tiempos_parciales, &tiempos_parciales[i], 0);
        }

        for (int i = 0; i < cantidad; i++) {
            xQueueSend(ctx->cola_tiempos_parciales, &tiempos_parciales[i], 0);
        }

        for (int i = 0; i < MAX_TIEMPOS_PARCIALES; i++) {
            if (i < cantidad) {
                int t = tiempos_parciales[i];
                DibujarDigito(p_parcial[i], 0, (t / 100) % 10);
                DibujarDigito(p_parcial[i], 1, (t / 10) % 10);
                // ILI9341DrawFilledCircle(160, 180, 5, DIGITO_ENCENDIDO_PARCIAL);
                DibujarDigito(p_parcial[i], 2, t % 10);
            } else {
                DibujarDigito(p_parcial[i], 0, 0);
                DibujarDigito(p_parcial[i], 1, 0);
                // ILI9341DrawFilledCircle(160, 180, 2, DIGITO_ENCENDIDO_PARCIAL);
                DibujarDigito(p_parcial[i], 2, 0);
            }
        }

        vTaskDelayUntil(&last_time, tiempo);
    }
}

void app_main(void) {
    static app_data_t ctx = {0};

    ctx.botones_event = xEventGroupCreate();
    ctx.mutex_tiempos = xSemaphoreCreateMutex();
    ctx.cola_tiempos_parciales = xQueueCreate(MAX_TIEMPOS_PARCIALES, sizeof(uint16_t));

    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    init_botones();
    init_leds();

    xTaskCreate(tarea_escaneo_botones, "Escaneo Botones", 2048, &ctx, 3, NULL);
    xTaskCreate(tarea_eventos_botones, "Eventos Botones", 2048, &ctx, 4, NULL);
    xTaskCreate(tarea_leds, "Control Leds", 2048, &ctx, 2, NULL);
    xTaskCreate(tarea_display, "Display Cronometro", 4096, &ctx, 5, NULL);
}

/* === End of documentation ========================================================================================= */
