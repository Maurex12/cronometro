#include "botones.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "BOTONES"

esp_err_t init_botones(void)
{
    gpio_config_t config = {
        .pin_bit_mask = ((1ULL << TEC1_GPIO) | (1ULL << TEC2_GPIO) | (1ULL << TEC3_GPIO)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&config);
}

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
                    ESP_LOGI(TAG, "Se presiono el boton 1");
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
