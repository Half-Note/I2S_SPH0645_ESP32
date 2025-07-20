#include "driver/i2s.h"
#include <math.h>

// === I2S Configuration ===
const i2s_port_t I2S_PORT = I2S_NUM_0;
#define SAMPLE_RATE     44100
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define MIC_RESOLUTION  8388608.0f  // For 24-bit mic: 2^23

// === Calibration Parameters ===
float dbA_offset = 100.0;          // Additive shift to match SPL meter
float gain_adjustment = 1.0;       // Manual gain correction if needed
bool use_rms = false;              // Toggle RMS vs Peak-to-peak for dBA

// === Sampling and Print Settings ===
const int buffer_size = 256;       // Number of samples per window
int32_t samples[buffer_size];
unsigned long lastPrint = 0;
const unsigned long printInterval = 1000;

// === I2S Setup ===
void setup() {
  Serial.begin(115200);
  delay(100);

  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 33
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
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
  size_t bytes_read;
  int32_t min_val = INT32_MAX, max_val = INT32_MIN;
  double sum_squares = 0;

  for (int i = 0; i < buffer_size; i++) {
    int32_t sample = 0;
    esp_err_t err = i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);

    if (err == ESP_OK && bytes_read == sizeof(sample)) {
      int32_t shifted = sample >> 8;
      samples[i] = shifted;

      if (shifted < min_val) min_val = shifted;
      if (shifted > max_val) max_val = shifted;

      float norm_sample = (float)shifted / MIC_RESOLUTION;
      sum_squares += norm_sample * norm_sample;
    }
  }

  unsigned long now = millis();
  if (now - lastPrint >= printInterval) {
    float dB;

    if (use_rms) {
      float rms = sqrt(sum_squares / buffer_size);
      float dbFS = 20.0 * log10(rms + 1e-10);  // Avoid log(0)
      dB = dbFS + dbA_offset;
    } else {
      float peak_to_peak = max_val - min_val;
      float amplitude = (peak_to_peak / 2.0) * gain_adjustment;
      float normalized = amplitude / MIC_RESOLUTION;
      float dbFS = 20.0 * log10(normalized + 1e-10);
      dB = dbFS + dbA_offset;
    }

    // === Output ===
    Serial.printf("Min: %d, Max: %d | ", min_val, max_val);
    Serial.printf("%s: %.2f dBA\n", use_rms ? "RMS" : "Peak", dB);

    lastPrint = now;
  }
}
