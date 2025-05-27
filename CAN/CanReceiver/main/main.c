#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "driver/gpio.h"

#define TX_GPIO_NUM ((gpio_num_t)4)
#define RX_GPIO_NUM ((gpio_num_t)5)
#define LED_GPIO ((gpio_num_t)17)

// Parâmetros do LED
#define MIN_DISTANCE_CM 5.0
#define MAX_DISTANCE_CM 50.0
#define MIN_FREQUENCY_HZ 1.0
#define MAX_FREQUENCY_HZ 10.0

// Tempo máximo sem mensagem antes de considerar desconectado (em ms)
#define TIMEOUT_SEM_MENSAGEM_MS 1000

void app_main(void) {
    // Configura GPIO do LED
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // Configura TWAI (CAN)
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        printf("Erro ao instalar TWAI.\n");
        return;
    }

    if (twai_start() != ESP_OK) {
        printf("Erro ao iniciar TWAI.\n");
        return;
    }

    printf("Aguardando mensagens no barramento CAN...\n");

    float distancia_atual = 0;
    float frequencia_led = 0;
    TickType_t ultimo_toggle = xTaskGetTickCount();
    TickType_t tempo_sem_mensagem = 0;
    bool led_estado = false;
    bool recebeu_mensagem = false;

    while (1) {
        twai_message_t message;

        // ====================== ESCUTA CAN ======================
        if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
            recebeu_mensagem = true;
            tempo_sem_mensagem = 0;

            printf("====================================\n");
            printf("Mensagem recebida:\n");
            printf("ID: 0x%" PRIX32 "\n", message.identifier);
            printf("DLC: %d\n", message.data_length_code);

            if (message.data_length_code >= 5) {
                uint8_t status = message.data[0];
                memcpy(&distancia_atual, &message.data[1], sizeof(float));

                printf("Status: %d\n", status);
                printf("Distância: %.2f cm\n", distancia_atual);

                // Cálculo da frequência
                if (distancia_atual <= MIN_DISTANCE_CM) {
                    frequencia_led = MAX_FREQUENCY_HZ;
                } else if (distancia_atual >= MAX_DISTANCE_CM) {
                    frequencia_led = 0; // LED desligado
                } else {
                    frequencia_led = MIN_FREQUENCY_HZ + 
                                     (MAX_FREQUENCY_HZ - MIN_FREQUENCY_HZ) * 
                                     (1 - (distancia_atual - MIN_DISTANCE_CM) / 
                                     (MAX_DISTANCE_CM - MIN_DISTANCE_CM));
                }

                printf("Frequência LED: %.1f Hz\n", frequencia_led);
            }

            printf("Dados brutos: ");
            for (int i = 0; i < message.data_length_code; i++) {
                printf("%02X ", message.data[i]);
            }
            printf("\n====================================\n");

        } else {
            // Sem mensagem neste ciclo (100ms)
            tempo_sem_mensagem += 100;

            if (tempo_sem_mensagem >= TIMEOUT_SEM_MENSAGEM_MS) {
                if (recebeu_mensagem) {
                    printf("Nenhuma mensagem recebida. Aguardando...\n");
                }
                recebeu_mensagem = false;
                frequencia_led = 0;
                gpio_set_level(LED_GPIO, 0); // LED apagado
                led_estado = false;
            }
        }

        // =================== CONTROLE DO LED ===================
        if (recebeu_mensagem) {
            if (frequencia_led == 0) {
                gpio_set_level(LED_GPIO, 0); // LED desligado
                led_estado = false;
            } else {
                TickType_t intervalo = (TickType_t)(1000 / frequencia_led / 2 / portTICK_PERIOD_MS);
                if (xTaskGetTickCount() - ultimo_toggle >= intervalo) {
                    led_estado = !led_estado;
                    gpio_set_level(LED_GPIO, led_estado);
                    ultimo_toggle = xTaskGetTickCount();
                }
            }
        } else {
            gpio_set_level(LED_GPIO, 0); // LED sempre desligado sem mensagens
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    twai_stop();
    twai_driver_uninstall();
}
