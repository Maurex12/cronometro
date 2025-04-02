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
#include "botones.h"

/* === Macros definitions =========================================================================================== */

#define DIGITO_ANCHO     60

#define DIGITO_ALTO      100

#define DIGITO_ENCENDIDO ILI9341_RED

#define DIGITO_APAGADO   0x1800

#define DIGITO_FONDO     ILI9341_BLACK

/* === Private data type declarations =============================================================================== */

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
