// main.cpp
#include <stdio.h>
#include <string.h> // Para memset
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp32/rom/ets_sys.h" // Para ets_delay_us

// Inclua o cabeçalho da sua biblioteca C++ MCP2515 para ESP-IDF
#include "mcp2515.h" // Este é o mcp2515.h que você forneceu com a classe C++

#define TAG "CAN_ULTRASONIC_CPP"

// Configuração SPI (VSPI)
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5

// Pinos do sensor ultrassônico
#define TRIGGER_GPIO    12
#define ECHO_GPIO       13
#define MIN_DISTANCE_CM 20

#define ULTRASONIC_TIMEOUT_US 30000 // Timeout para a leitura do echo (ex: 30ms para ~5 metros)

spi_device_handle_t spi_handle;
// MCP2515 mcp_can_controller; // Será instanciado em app_main após spi_handle ser válido

// Função para medir a distância
float measure_distance() {
    gpio_set_level((gpio_num_t)TRIGGER_GPIO, 0);
    ets_delay_us(2);
    gpio_set_level((gpio_num_t)TRIGGER_GPIO, 1);
    ets_delay_us(10);
    gpio_set_level((gpio_num_t)TRIGGER_GPIO, 0);

    uint32_t start_time_us = 0, end_time_us = 0;
    uint32_t timeout_start_us;

    // Espera o pino ECHO ir para HIGH (com timeout)
    timeout_start_us = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)ECHO_GPIO) == 0) {
        if ((esp_timer_get_time() - timeout_start_us) > ULTRASONIC_TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout esperando ECHO ir para HIGH");
            return -1.0f;
        }
    }
    start_time_us = esp_timer_get_time();

    // Espera o pino ECHO ir para LOW (com timeout)
    timeout_start_us = esp_timer_get_time(); // Reinicia o contador de timeout para esta fase
    while (gpio_get_level((gpio_num_t)ECHO_GPIO) == 1) {
        if ((esp_timer_get_time() - timeout_start_us) > ULTRASONIC_TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout esperando ECHO ir para LOW");
            return -1.0f;
        }
    }
    end_time_us = esp_timer_get_time();

    if (end_time_us > start_time_us) {
        uint32_t duration_us = end_time_us - start_time_us;
        // Velocidade do som ~343 m/s ou 0.0343 cm/µs
        // Distância = (tempo * velocidade) / 2
        return (float)duration_us * 0.0343f / 2.0f;
    }
    ESP_LOGW(TAG, "Erro na medição do tempo do echo (end <= start)");
    return -1.0f; // Retorna um valor inválido em caso de erro de leitura
}

extern "C" void app_main(void) {
    // Configuração do GPIO para o sensor ultrassônico
    gpio_config_t io_conf_trigger = {0}; // Inicializa com zeros
    io_conf_trigger.pin_bit_mask = (1ULL << TRIGGER_GPIO);
    io_conf_trigger.mode = GPIO_MODE_OUTPUT;
    io_conf_trigger.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf_trigger.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_trigger.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_trigger);

    gpio_config_t io_conf_echo = {0}; // Inicializa com zeros
    io_conf_echo.pin_bit_mask = (1ULL << ECHO_GPIO);
    io_conf_echo.mode = GPIO_MODE_INPUT;
    io_conf_echo.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf_echo.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_echo.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_echo);

    // Inicialização do SPI
    spi_bus_config_t bus_cfg = {0}; // Inicializa com zeros
    bus_cfg.mosi_io_num = PIN_NUM_MOSI;
    bus_cfg.miso_io_num = PIN_NUM_MISO;
    bus_cfg.sclk_io_num = PIN_NUM_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    // max_transfer_sz = 0 para usar o padrão do driver
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    spi_device_interface_config_t dev_cfg = {0}; // Inicializa com zeros
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = 10 * 1000 * 1000; // 10 MHz
    dev_cfg.spics_io_num = PIN_NUM_CS;
    dev_cfg.queue_size = 7;
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    ESP_ERROR_CHECK(ret);

    // Instanciação do objeto MCP2515 DEPOIS que spi_handle é válido
    MCP2515 mcp_can_controller(&spi_handle);

    // Inicialização do MCP2515 usando o objeto
    ESP_LOGI(TAG, "Resetando MCP2515...");
    if (mcp_can_controller.reset() != MCP2515::ERROR_OK) {
        ESP_LOGE(TAG, "Falha no reset do MCP2515!");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Configurando bitrate...");
    if (mcp_can_controller.setBitrate(CAN_500KBPS, MCP_8MHZ) != MCP2515::ERROR_OK) {
        ESP_LOGE(TAG, "Erro na configuração do bitrate!");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "Bitrate configurado.");

    ESP_LOGI(TAG, "Configurando modo normal...");
    if (mcp_can_controller.setNormalMode() != MCP2515::ERROR_OK) {
        ESP_LOGE(TAG, "Falha ao configurar o modo normal do MCP2515");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "Modo normal configurado.");

    struct can_frame tx_frame;

    while (1) {
        float distance = measure_distance();
        ESP_LOGI(TAG, "Distância: %.2f cm", distance);

        // Verifica se a leitura da distância foi válida antes de prosseguir
        if (distance >= 0 && distance < MIN_DISTANCE_CM) {
            memset(&tx_frame, 0, sizeof(struct can_frame));
            tx_frame.can_id = 0x123;
            // Para usar IDs estendidos, você precisaria OR com CAN_EFF_FLAG na biblioteca ou se ela o fizer internamente.
            // Ex: tx_frame.can_id = 0x123 | CAN_EFF_FLAG; (se a struct can_frame suportar isso diretamente e a lib também)
            // A sua classe MCP2515 no mcp2515.h tem um método prepareId que trata CAN_EFF_FLAG,
            // então você deve passar o ID com a flag se quiser estendido.
            // No entanto, o método sendMessage que recebe só can_frame* pode não interpretar
            // essa flag diretamente no can_id, ele espera que o ID já seja SFF ou EFF.
            // Para este exemplo, mantendo simples com ID padrão.
            tx_frame.can_dlc = 4;
            tx_frame.data[0] = 0x01;       // Byte de alerta
            tx_frame.data[1] = (uint8_t)distance; // Distância como byte
            // tx_frame.data[2] = ...;
            // tx_frame.data[3] = ...;

            if (mcp_can_controller.sendMessage(&tx_frame) == MCP2515::ERROR_OK) {
                // Use %lX para unsigned long int, ou %X para unsigned int.
                // can_id é uint32_t, então %lu ou %u para decimal, %lX ou %X para hexadecimal.
                // %lX é mais comum para IDs CAN.
                ESP_LOGW(TAG, "Alerta CAN enviado! ID: 0x%lX, Dados: %02X %02X, Dist: %.2f cm",
                         (unsigned long)tx_frame.can_id, tx_frame.data[0], tx_frame.data[1], distance);
            } else {
                ESP_LOGE(TAG, "Falha ao enviar mensagem CAN.");
            }
        } else if (distance < 0) {
            ESP_LOGW(TAG, "Leitura inválida do sensor ultrassônico.");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}