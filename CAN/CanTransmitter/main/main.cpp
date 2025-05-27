#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp32/rom/ets_sys.h"

#include "mcp2515.h"

#define TAG "CAN_ULTRASONIC_CPP"

// Configuração SPI (VSPI)
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5

// Pinos do sensor ultrassônico
#define TRIGGER_GPIO    12
#define ECHO_GPIO       13

#define ULTRASONIC_TIMEOUT_US 30000

// Limites de distância
#define MAX_DISTANCE_CM 50.0f
#define MIN_DISTANCE_CM 0.0f

spi_device_handle_t spi_handle;

// Função para medir a distância
float measure_distance() {
    gpio_set_level((gpio_num_t)TRIGGER_GPIO, 0);
    ets_delay_us(2);
    gpio_set_level((gpio_num_t)TRIGGER_GPIO, 1);
    ets_delay_us(10);
    gpio_set_level((gpio_num_t)TRIGGER_GPIO, 0);

    uint32_t start_time_us = 0, end_time_us = 0;
    uint32_t timeout_start_us;

    timeout_start_us = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)ECHO_GPIO) == 0) {
        if ((esp_timer_get_time() - timeout_start_us) > ULTRASONIC_TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout esperando ECHO HIGH");
            return -1.0f;
        }
    }
    start_time_us = esp_timer_get_time();

    timeout_start_us = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)ECHO_GPIO) == 1) {
        if ((esp_timer_get_time() - timeout_start_us) > ULTRASONIC_TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout esperando ECHO LOW");
            return -1.0f;
        }
    }
    end_time_us = esp_timer_get_time();

    if (end_time_us > start_time_us) {
        uint32_t duration_us = end_time_us - start_time_us;
        return (float)duration_us * 0.0343f / 2.0f;
    }
    ESP_LOGW(TAG, "Erro na medição (end <= start)");
    return -1.0f;
}

extern "C" void app_main(void) {
    gpio_config_t io_conf_trigger = {};
    io_conf_trigger.pin_bit_mask = (1ULL << TRIGGER_GPIO);
    io_conf_trigger.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf_trigger);

    gpio_config_t io_conf_echo = {};
    io_conf_echo.pin_bit_mask = (1ULL << ECHO_GPIO);
    io_conf_echo.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf_echo);

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = PIN_NUM_MOSI;
    bus_cfg.miso_io_num = PIN_NUM_MISO;
    bus_cfg.sclk_io_num = PIN_NUM_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = 10 * 1000 * 1000;
    dev_cfg.spics_io_num = PIN_NUM_CS;
    dev_cfg.queue_size = 7;
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    ESP_ERROR_CHECK(ret);

    MCP2515 mcp_can_controller(&spi_handle);

    ESP_LOGI(TAG, "Resetando MCP2515...");
    if (mcp_can_controller.reset() != MCP2515::ERROR_OK) {
        ESP_LOGE(TAG, "Falha no reset do MCP2515!");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Configurando bitrate...");
    if (mcp_can_controller.setBitrate(CAN_500KBPS, MCP_8MHZ) != MCP2515::ERROR_OK) {
        ESP_LOGE(TAG, "Erro na configuração do bitrate!");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    ESP_LOGI(TAG, "Configurando modo normal...");
    if (mcp_can_controller.setNormalMode() != MCP2515::ERROR_OK) {
        ESP_LOGE(TAG, "Falha ao configurar o modo normal do MCP2515");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    struct can_frame tx_frame;

    while (1) {
        float distance = measure_distance();
        ESP_LOGI(TAG, "Distância medida: %.2f cm", distance);

        if (distance >= MIN_DISTANCE_CM && distance <= MAX_DISTANCE_CM) {
            memset(&tx_frame, 0, sizeof(struct can_frame));
            tx_frame.can_id = 0x123;

            tx_frame.can_dlc = 5; // 1 byte status + 4 bytes float

            tx_frame.data[0] = 0x01; // Status ou código da mensagem

            memcpy(&tx_frame.data[1], &distance, sizeof(float));

            if (mcp_can_controller.sendMessage(&tx_frame) == MCP2515::ERROR_OK) {
                ESP_LOGI(TAG, "Mensagem CAN enviada. ID: 0x%lX, Distância: %.2f cm",
                         (unsigned long)tx_frame.can_id, distance);
            } else {
                ESP_LOGE(TAG, "Falha ao enviar mensagem CAN.");
            }
        } else if (distance < 0) {
            ESP_LOGW(TAG, "Leitura inválida do sensor ultrassônico.");
        } else if (distance > MAX_DISTANCE_CM) {
            ESP_LOGW(TAG, "Distância fora do limite máximo de %.2f cm. Ignorada.", MAX_DISTANCE_CM);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}