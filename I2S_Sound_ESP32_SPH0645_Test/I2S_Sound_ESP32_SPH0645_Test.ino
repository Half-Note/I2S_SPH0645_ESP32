#include "driver/i2s.h"

const i2s_port_t I2S_PORT = I2S_NUM_0;

unsigned long lastPrint = 0;
const unsigned long printInterval = 1000; // 1 second interval for printing

int32_t min_sample = INT32_MAX;
int32_t max_sample = INT32_MIN;

void setup() {
  Serial.begin(115200);
  esp_err_t err;

  // I2S configuration
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Master, Receive
      .sample_rate = 44100,                         // setup the I2S audio input for 44.1 kHz with 32-bits per sample
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // 32-bit samples (required by many I2S mics)
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Use left channel only
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // Number of DMA buffers
      .dma_buf_len = 8,                             // Samples per buffer
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
  };

  // Pin configuration for I2S
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 26,               // Bit clock (SCK)
      .ws_io_num = 25,                // Word select (LRCK)
      .data_out_num = I2S_PIN_NO_CHANGE, // Not used
      .data_in_num = 33               // Data in (SD)
  };

  // Install and start I2S driver
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }

  Serial.println("I2S driver installed.");
}

void loop() {
  int32_t sample = 0;
  size_t bytes_read;

  esp_err_t err = i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);

  if (err == ESP_OK && bytes_read == sizeof(sample)) {
    int32_t shifted = sample >> 8;  // Shift for 24-bit mic data

    if (shifted < min_sample) min_sample = shifted;
    if (shifted > max_sample) max_sample = shifted;

    unsigned long now = millis();
    if (now - lastPrint >= printInterval) {
      Serial.printf("Min sample: %d, Max sample: %d\n", min_sample, max_sample);
      // Reset min and max for next interval
      min_sample = INT32_MAX;
      max_sample = INT32_MIN;
      lastPrint = now;
    }
  } else {
    Serial.printf("Read error: %d | Bytes read: %u\n", err, (unsigned int)bytes_read);
  }
}
