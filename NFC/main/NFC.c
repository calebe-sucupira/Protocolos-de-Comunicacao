#include <esp_log.h>
#include <esp_check.h>
#include <string.h>
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "picc/rc522_mifare.h"

static const char *TAG = "NFC_URL_WRITER";

const char *URL_PARA_GRAVAR = "https://www.ufc.br";

#define RC522_SPI_BUS_GPIO_MISO   19
#define RC522_SPI_BUS_GPIO_MOSI   23
#define RC522_SPI_BUS_GPIO_SCLK   18
#define RC522_SPI_SCANNER_GPIO_SDA 5  
#define RC522_SCANNER_GPIO_RST    4  

// Chaves
static const rc522_mifare_key_t default_key = { .value = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };
static const rc522_mifare_key_t ndef_key = { .value = { 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7 } };

static rc522_spi_config_t driver_config = {
    .host_id = SPI2_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
        .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
        .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
    },
    .dev_config = { .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA },
    .rst_io_num = RC522_SCANNER_GPIO_RST,
};

static rc522_driver_handle_t driver;
static rc522_handle_t scanner;

/**
 * @brief Formata um cartão para NDEF.
 */
static esp_err_t format_tag(rc522_handle_t scanner, rc522_picc_t *picc) {
    ESP_LOGI(TAG, "A formatar o cartao para NDEF...");
    for (uint8_t i = 1; i < 16; i++) {
        uint8_t trailer_block = (i * 4) + 3;
        ESP_RETURN_ON_ERROR(rc522_mifare_auth(scanner, picc, trailer_block, &default_key), TAG, "format: auth fail setor %d", i);
        
        uint8_t new_trailer[RC522_MIFARE_BLOCK_SIZE] = {
            0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 
            0xFF, 0x07, 0x80, 0x69,             
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  
        };
        ESP_RETURN_ON_ERROR(rc522_mifare_write(scanner, picc, trailer_block, new_trailer), TAG, "format: write fail setor %d", i);
    }
    ESP_LOGI(TAG, "Formatacao NDEF concluida.");
    return ESP_OK;
}

/**
 * @brief Escreve a URL no cartão já formatado.
 */
static esp_err_t write_url(rc522_handle_t scanner, rc522_picc_t *picc) {
    uint8_t url_len = strlen(URL_PARA_GRAVAR);
    uint8_t ndef_msg_len = url_len + 5;
    uint8_t tlv_len = ndef_msg_len + 2;
    uint8_t final_buffer_len = tlv_len + 1;

    uint8_t buffer[256] = {0};
    buffer[0] = 0x03; 
    buffer[1] = ndef_msg_len;
    buffer[2] = 0xD1; 
    buffer[3] = 0x01; 
    buffer[4] = url_len + 1;
    buffer[5] = 'U';
    buffer[6] = 0x00;
    memcpy(&buffer[7], URL_PARA_GRAVAR, url_len);
    buffer[final_buffer_len - 1] = 0xFE; 

    ESP_LOGI(TAG, "A escrever URL no cartao...");
    uint32_t bytes_written = 0;
    uint8_t current_block = 4;
    while(bytes_written < final_buffer_len) {
        if (current_block % 4 == 3) { current_block++; continue; }
        ESP_RETURN_ON_ERROR(rc522_mifare_auth(scanner, picc, current_block, &ndef_key), TAG, "write: auth fail bloco %d", current_block);
        
        uint8_t block_buffer[RC522_MIFARE_BLOCK_SIZE] = {0};
        memcpy(block_buffer, &buffer[bytes_written], RC522_MIFARE_BLOCK_SIZE);
        
        ESP_RETURN_ON_ERROR(rc522_mifare_write(scanner, picc, current_block, block_buffer), TAG, "write: write fail bloco %d", current_block);
        
        bytes_written += RC522_MIFARE_BLOCK_SIZE;
        current_block++;
    }
    return ESP_OK;
}

/**
 * @brief Função principal chamada quando um cartão é detetado.
 */
static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state != RC522_PICC_STATE_ACTIVE) {
        return;
    }


    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "CARTAO DETETADO!");
    rc522_picc_print(picc);

    if (!rc522_mifare_type_is_classic_compatible(picc->type)) {
        ESP_LOGW(TAG, "O cartao nao e compativel com este exemplo (MIFARE Classic).");
        return;
    }

    if (format_tag(scanner, picc) == ESP_OK) {
        ESP_LOGI(TAG, "Formatacao bem-sucedida. A escrever a URL...");
        if (write_url(scanner, picc) == ESP_OK) {
            ESP_LOGI(TAG, "********************************");
            ESP_LOGI(TAG, "URL gravada com sucesso!");
            ESP_LOGI(TAG, "Teste com o seu telemovel.");
            ESP_LOGI(TAG, "********************************");
        } else {
            ESP_LOGE(TAG, "Falha ao gravar a URL apos formatacao.");
        }
    } else {
        ESP_LOGW(TAG, "Formatacao falhou. A assumir que o cartao ja esta formatado e a tentar escrever...");
        if (write_url(scanner, picc) == ESP_OK) {
            ESP_LOGI(TAG, "********************************");
            ESP_LOGI(TAG, "URL gravada em cartao ja formatado!");
            ESP_LOGI(TAG, "Teste com o seu telemovel.");
            ESP_LOGI(TAG, "********************************");
        } else {
            ESP_LOGE(TAG, "Falha ao gravar a URL. O cartao pode ter chaves desconhecidas.");
        }
    }

    rc522_mifare_deauth(scanner, picc);
    ESP_LOGI(TAG, "Operacao concluida. Aproxime outro cartao.");
    ESP_LOGI(TAG, "===================================\n");
}

void app_main() {
    rc522_spi_create(&driver_config, &driver);
    rc522_driver_install(driver);
    rc522_config_t scanner_config = { .driver = driver };
    rc522_create(&scanner_config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
    rc522_start(scanner);
    ESP_LOGI(TAG, "Leitor iniciado. Aproxime um cartao para gravar a URL...");
}
