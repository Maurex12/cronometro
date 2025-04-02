#ifndef BOTONES_H
#define BOTONES_H

#include "driver/gpio.h"
#include "esp_err.h"

// Defino GPIOs para los botones
#define TEC1_GPIO 32
#define TEC2_GPIO 35
#define TEC3_GPIO 34

typedef enum 
{
    BOTON_NINGUNO = 0, 
    BOTON_TEC1, 
    BOTON_TEC2, 
    BOTON_TEC3 
} boton_t;

// Inicializo los botones
esp_err_t init_botones(void);

// Tarea para escanear los botones
void tarea_escaneo_botones(void *pvParameters);

#endif // BOTONES