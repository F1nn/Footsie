// System includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali_scheme.h"
#include "system.h"
#include "esp_ws28xx.h"
#include "dac80501.h"

#define EXAMPLE_READ_LEN                    64 // 16 samples, each sample has 4 bytes (see SOC_ADC_DIGI_RESULT_BYTES)
#define MAX_PARSED_SAMPLES                  (EXAMPLE_READ_LEN / SOC_ADC_DIGI_RESULT_BYTES)
/* TODO: move calibration and scale constants to Kconfig/menuconfig or NVS */
/* ADC reference (in mV) for converting ADC code to millivolts */
#define ADC_VREF_MV                         3300
/* DAC full-scale output target in mV */
#define DAC_FULL_SCALE_MV                   5000
/* Maximum DAC update rate */
#define DAC_UPDATE_PERIOD_MS                20
/* Output curve exponent: >1.0 gives finer control at the low end. */
#define OUTPUT_CURVE_GAMMA                  2.2f
/* ADC calibrated range (mV) - adjust these to match your potentiometer's actual measurement range */
/* Set to the calibrated ADC reading at the pot's minimum position */
#define ADC_MIN_CALIBRATED_MV               139
/* Set to the calibrated ADC reading at the pot's maximum position */
#define ADC_MAX_CALIBRATED_MV               3181
#define ADC_SPAN_MV                         (ADC_MAX_CALIBRATED_MV - ADC_MIN_CALIBRATED_MV)

static adc_channel_t channel[1] = {ADC_CHANNEL_2}; // Channel 2 maps to GPIO3

static const char *TAG = "Footsie";

CRGB *ws2812_buffer;
static bool ws2812_ready = false;
dac80501_device_t dac80501_dev;
i2c_master_bus_handle_t i2c_bus_handle = NULL;
static dac80501_status_t dac_status = DAC80501_ERR_NOT_INITIALIZED;
static adc_cali_handle_t adc_cali_handle = NULL;

static adc_continuous_data_t s_parsed_data[MAX_PARSED_SAMPLES];

static uint32_t adc_mV_to_curved_dac_mV(uint32_t adc_mV)
{
    if (ADC_SPAN_MV <= 0) {
        return 0;
    }

    if (adc_mV <= ADC_MIN_CALIBRATED_MV) {
        return 0;
    }

    if (adc_mV >= ADC_MAX_CALIBRATED_MV) {
        return DAC_FULL_SCALE_MV;
    }

    float normalized = (float)(adc_mV - ADC_MIN_CALIBRATED_MV) / (float)ADC_SPAN_MV;
    if (normalized < 0.0f) {
        normalized = 0.0f;
    } else if (normalized > 1.0f) {
        normalized = 1.0f;
    }

    float curved = powf(normalized, OUTPUT_CURVE_GAMMA);
    uint32_t output_mV = (uint32_t)((curved * (float)DAC_FULL_SCALE_MV) + 0.5f);
    if (output_mV > DAC_FULL_SCALE_MV) {
        output_mV = DAC_FULL_SCALE_MV;
    }

    return output_mV;
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20000, // 20kHz
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

static void adc_calibration_init(void)
{
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_2,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = SOC_ADC_DIGI_MAX_BITWIDTH,
    };

    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration initialized");
    } else {
        ESP_LOGW(TAG, "ADC calibration init failed: %s", esp_err_to_name(ret));
        adc_cali_handle = NULL;
    }
}

static void sys_init(void) {
    // Delay to wait for uart connection so nothing is missed.
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    char *task_name = pcTaskGetName(NULL);
    ESP_LOGI(task_name, "Initialisation Started.");

    esp_log_level_set("I2C", ESP_LOG_DEBUG);

    // I2C bus configuration
    i2c_master_bus_config_t i2c_master_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT_NUM,
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_config, &i2c_bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized");

    // Initialize DAC80501 at default address 0x48
    dac_status = dac80501_init(&dac80501_dev, i2c_bus_handle, 0x48);
    if (dac_status != DAC80501_OK) {
        ESP_LOGE(TAG, "Failed to initialize DAC80501: %d", dac_status);
    } else {
        ESP_LOGI(TAG, "DAC80501 initialized successfully");
        // Set DAC to 0V (zero) initially
          /* Defaults per datasheet: BUF-GAIN=1 (2x internal 2.5V ref) and REF-DIV=0 (no divider)
              mean the hardware will provide 0-5V full-scale when powered from 5V.
              The driver sets these defaults in init; just write 0 to the DAC output. */
          dac80501_write_dac(&dac80501_dev, 0x0000);
    }

    gpio_reset_pin(PIN_ADC_IN);
    gpio_set_direction(PIN_ADC_IN, GPIO_MODE_INPUT);

    // WS2812B RGB LED driver initialisation.
    if (ws28xx_init(PIN_RGB, WS2812B, 1, &ws2812_buffer) == ESP_OK) {
        // Set LED to green as init complete confirmation
        ws2812_ready = true;
        ws2812_buffer[0] = (CRGB){.r=0, .g=1, .b=0};
        ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());
    } else {
        ESP_LOGW(TAG, "WS2812 init failed, skipping LED confirmation");
    }

    ESP_LOGI(task_name, "Initialisation Complete.");
}

void app_main(void) {

    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    uint64_t window_sample_sum_mV = 0;
    uint32_t window_sample_count = 0;
    uint32_t last_avg_value_mV = ADC_MIN_CALIBRATED_MV;
    uint32_t last_clamped_value_mV = ADC_MIN_CALIBRATED_MV;
    int32_t last_unclamped_target_mV = 0;
    uint32_t last_target_mV = 0;
    TickType_t last_dac_update_tick = 0;
    const TickType_t dac_update_period_ticks = pdMS_TO_TICKS(DAC_UPDATE_PERIOD_MS);

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);
    adc_calibration_init();

    ESP_LOGI(TAG, "Scale config: adc_min=%u mV, adc_max=%u mV, adc_span=%u mV, dac_full_scale=%u mV",
             ADC_MIN_CALIBRATED_MV,
             ADC_MAX_CALIBRATED_MV,
             (unsigned)(ADC_MAX_CALIBRATED_MV - ADC_MIN_CALIBRATED_MV),
             DAC_FULL_SCALE_MV);
    ESP_LOGI(TAG, "ADC config: vref=%u mV, attenuation=%d, calibration=%s",
             ADC_VREF_MV,
             ADC_ATTEN_DB_12,
             (adc_cali_handle != NULL) ? "curve-fitting" : "fallback-linear");
    ESP_LOGI(TAG, "Update rate: period=%u ms", DAC_UPDATE_PERIOD_MS);

    // Start ADC reading continuously
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    sys_init();

    // Output to DAC80501 only after a successful init.
    if (dac_status == DAC80501_OK) {
        /* write 0mV using device default vref (pass 0 to use device vref) */
        if (dac80501_write_voltage(&dac80501_dev, 0, 0) != DAC80501_OK) {
            ESP_LOGW(TAG, "Failed to write 0mV to DAC");
        }
    }

    while (1) {
        uint32_t num_parsed_samples = 0;
        bool adc_read_valid = false;

        ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
        if (ret == ESP_OK) {
            adc_read_valid = true;
            esp_err_t parse_ret = adc_continuous_parse_data(handle, result, ret_num, s_parsed_data, &num_parsed_samples);
            if (parse_ret == ESP_OK) {
                        if (num_parsed_samples > MAX_PARSED_SAMPLES) {
                            ESP_LOGW(TAG, "Parsed samples (%u) exceed buffer (%u), clipping", num_parsed_samples, MAX_PARSED_SAMPLES);
                            num_parsed_samples = MAX_PARSED_SAMPLES;
                        }
                for (uint32_t i = 0; i < num_parsed_samples; i++) {
                    if (s_parsed_data[i].valid) {
                        int calibrated_mv = 0;
                        if (adc_cali_handle != NULL) {
                            if (adc_cali_raw_to_voltage(adc_cali_handle, s_parsed_data[i].raw_data, &calibrated_mv) != ESP_OK) {
                                calibrated_mv = (int)(((uint64_t)s_parsed_data[i].raw_data * ADC_VREF_MV + 2047) / 4095);
                            }
                        } else {
                            calibrated_mv = (int)(((uint64_t)s_parsed_data[i].raw_data * ADC_VREF_MV + 2047) / 4095);
                        }

                        // Accumulate all samples within the current update window.
                        window_sample_sum_mV += (uint32_t)calibrated_mv;
                        window_sample_count++;
                    } else {
                        ESP_LOGW(TAG, "Invalid data [ADC%d_Ch%d_%"PRIu32"]",
                                 s_parsed_data[i].unit + 1,
                                 s_parsed_data[i].channel,
                                 s_parsed_data[i].raw_data);
                    }
                }
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (last_dac_update_tick == 0 || (now - last_dac_update_tick) >= dac_update_period_ticks) {
            uint32_t samples_used = window_sample_count;
            bool update_valid = false;
            float last_normalized_value = 0.0f;
            float last_curved_value = 0.0f;

                if (window_sample_count > 0) {
                    uint32_t avg_value = (uint32_t)(window_sample_sum_mV / window_sample_count);
                    last_avg_value_mV = avg_value;

                    /* Clamp averaged reading to the calibrated measurement range. */
                    uint32_t clamped_value = avg_value;
                    if (clamped_value < ADC_MIN_CALIBRATED_MV) {
                        clamped_value = ADC_MIN_CALIBRATED_MV;
                    }
                    if (clamped_value > ADC_MAX_CALIBRATED_MV) {
                        clamped_value = ADC_MAX_CALIBRATED_MV;
                    }

                    last_clamped_value_mV = clamped_value;
                    if (ADC_SPAN_MV <= 0) {
                        ESP_LOGW(TAG, "Invalid ADC calibration span (denom=0), skipping mapping to DAC");
                        last_unclamped_target_mV = 0;
                        last_target_mV = 0;
                    } else {
                        last_normalized_value = (float)(clamped_value - ADC_MIN_CALIBRATED_MV) / (float)ADC_SPAN_MV;
                        if (last_normalized_value < 0.0f) {
                            last_normalized_value = 0.0f;
                        } else if (last_normalized_value > 1.0f) {
                            last_normalized_value = 1.0f;
                        }
                        last_curved_value = powf(last_normalized_value, OUTPUT_CURVE_GAMMA);
                        last_unclamped_target_mV = (int32_t)(((int64_t)((int32_t)avg_value - (int32_t)ADC_MIN_CALIBRATED_MV) * DAC_FULL_SCALE_MV) / (int32_t)ADC_SPAN_MV);
                        last_target_mV = adc_mV_to_curved_dac_mV(clamped_value);
                        update_valid = true;
                    }
                }

            if (ws2812_ready && ws2812_buffer != NULL) {
                uint8_t led_green = 0;
                if (update_valid) {
                    led_green = (uint8_t)(((uint64_t)last_target_mV * 255u + (DAC_FULL_SCALE_MV / 2u)) / DAC_FULL_SCALE_MV);
                }

                ws2812_buffer[0] = (CRGB){.r=0, .g=led_green, .b=0};
                ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());
            }

            if (update_valid && dac_status == DAC80501_OK) {
                if (dac80501_write_voltage(&dac80501_dev, last_target_mV, 0) != DAC80501_OK) {
                    ESP_LOGW(TAG, "Failed to write %u mV to DAC", last_target_mV);
                }
            } else {
                if (dac_status == DAC80501_OK && dac80501_write_voltage(&dac80501_dev, 0, 0) != DAC80501_OK) {
                    ESP_LOGW(TAG, "Failed to write 0mV to DAC during fail-safe handling");
                }
                if (!adc_read_valid) {
                    ESP_LOGW(TAG, "ADC read failed, driving DAC to zero");
                } else {
                    ESP_LOGW(TAG, "No valid ADC update, driving DAC to zero");
                }
            }

            uint32_t applied_target_mV = update_valid ? last_target_mV : 0;
            int32_t curve_delta_mV = (int32_t)applied_target_mV - last_unclamped_target_mV;

            ESP_LOGI(TAG, "Map: s=%u avg=%u in=%u lin=%" PRId32 " out=%u d=%+" PRId32 " n=%0.3f g=%0.3f",
                     samples_used,
                     last_avg_value_mV,
                     last_clamped_value_mV,
                     last_unclamped_target_mV,
                     applied_target_mV,
                     curve_delta_mV,
                     (double)last_normalized_value,
                     (double)last_curved_value);

            window_sample_sum_mV = 0;
            window_sample_count = 0;
            last_dac_update_tick = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

}