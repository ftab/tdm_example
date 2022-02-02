/* TDM8 Example

    This example code will output 100Hz sine wave and triangle wave to 8 channels of I2S driver

    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include <math.h>

#if !SOC_I2S_SUPPORTS_TDM
#error Must build for a target that supports TDM (e.g. ESP32S3)
#endif

#define SAMPLE_RATE     (44100)

#define CHANNEL_NUM     (8)

#if CHANNEL_NUM == 4
#define CHANNEL_MASK    (I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1 | I2S_TDM_ACTIVE_CH2 | I2S_TDM_ACTIVE_CH3)
#elif CHANNEL_NUM == 8
#define CHANNEL_MASK    (I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1 | I2S_TDM_ACTIVE_CH2 | I2S_TDM_ACTIVE_CH3 | I2S_TDM_ACTIVE_CH4 | I2S_TDM_ACTIVE_CH5 | I2S_TDM_ACTIVE_CH6 | I2S_TDM_ACTIVE_CH7)
#elif CHANNEL_NUM == 16
#define CHANNEL_MASK    (I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1 | I2S_TDM_ACTIVE_CH2 | I2S_TDM_ACTIVE_CH3 | I2S_TDM_ACTIVE_CH4 | I2S_TDM_ACTIVE_CH5 | I2S_TDM_ACTIVE_CH6 | I2S_TDM_ACTIVE_CH7 | \
                         I2S_TDM_ACTIVE_CH8 | I2S_TDM_ACTIVE_CH9 | I2S_TDM_ACTIVE_CH10 | I2S_TDM_ACTIVE_CH11 | I2S_TDM_ACTIVE_CH12 | I2S_TDM_ACTIVE_CH13 | I2S_TDM_ACTIVE_CH14 | I2S_TDM_ACTIVE_CH15)
#else
#error Channel numbers supported: 4, 8, 16
#endif

#define I2S_NUM         (0)
#define WAVE_FREQ_HZ    (100)
#define PI              (3.14159265)
#define I2S_MCK_IO      (GPIO_NUM_5)
#define I2S_BCK_IO      (GPIO_NUM_6)
#define I2S_WS_IO       (GPIO_NUM_4)
#define I2S_DO_IO       (GPIO_NUM_7)
#define I2S_DI_IO       (-1)

#define SAMPLE_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)

static const char* TAG = "tdm_example";

static void setup_triangle_sine_waves(int bits)
{
    int *samples_data = malloc(SAMPLE_PER_CYCLE*CHANNEL_NUM*sizeof(int));
    unsigned int i;
    double sin_float, triangle_float, triangle_step = (double) pow(2, bits) / SAMPLE_PER_CYCLE;
    size_t i2s_bytes_write = 0;

    printf("\r\nTest bits=%d free mem=%d, written data=%d\n", bits, esp_get_free_heap_size(), SAMPLE_PER_CYCLE*CHANNEL_NUM*sizeof(int));

    triangle_float = -(pow(2, bits)/2 - 1);

    for(i = 0; i < SAMPLE_PER_CYCLE; i++) {
        sin_float = sin(i * 2 * PI / SAMPLE_PER_CYCLE);
        if(sin_float >= 0)
            triangle_float += triangle_step;
        else
            triangle_float -= triangle_step;

        sin_float *= (pow(2, bits)/2 - 1);

        if (bits == 24) { //1-bytes unused
            samples_data[i*CHANNEL_NUM] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 1] = ((int) sin_float) << 8;
#if CHANNEL_NUM > 2
            samples_data[i*CHANNEL_NUM + 2] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 3] = ((int) sin_float) << 8;
#endif
#if CHANNEL_NUM > 4
            samples_data[i*CHANNEL_NUM + 4] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 5] = ((int) sin_float) << 8;
            samples_data[i*CHANNEL_NUM + 6] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 7] = ((int) sin_float) << 8;
#endif
#if CHANNEL_NUM > 8
            samples_data[i*CHANNEL_NUM + 8] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 9] = ((int) sin_float) << 8;
            samples_data[i*CHANNEL_NUM + 10] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 11] = ((int) sin_float) << 8;
            samples_data[i*CHANNEL_NUM + 12] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 13] = ((int) sin_float) << 8;
            samples_data[i*CHANNEL_NUM + 14] = ((int) triangle_float) << 8;
            samples_data[i*CHANNEL_NUM + 15] = ((int) sin_float) << 8;
#endif
        } else {
            samples_data[i*CHANNEL_NUM] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 1] = ((int) sin_float);
#if CHANNEL_NUM > 2
            samples_data[i*CHANNEL_NUM + 2] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 3] = ((int) sin_float);
#endif
#if CHANNEL_NUM > 4
            samples_data[i*CHANNEL_NUM + 4] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 5] = ((int) sin_float);
            samples_data[i*CHANNEL_NUM + 6] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 7] = ((int) sin_float);
#endif
#if CHANNEL_NUM > 8
            samples_data[i*CHANNEL_NUM + 8] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 9] = ((int) sin_float);
            samples_data[i*CHANNEL_NUM + 10] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 11] = ((int) sin_float);
            samples_data[i*CHANNEL_NUM + 12] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 13] = ((int) sin_float);
            samples_data[i*CHANNEL_NUM + 14] = ((int) triangle_float);
            samples_data[i*CHANNEL_NUM + 15] = ((int) sin_float);
#endif
        }

    }
    ESP_LOGI(TAG, "set clock");
    /* Set 32 bits per channel regardless of number of sample bits */
    i2s_set_clk(I2S_NUM, SAMPLE_RATE, (I2S_BITS_PER_CHAN_32BIT << 16) | bits, CHANNEL_MASK);

    ESP_LOGI(TAG, "write data");
    i2s_write(I2S_NUM, samples_data, ((bits+8)/16)*SAMPLE_PER_CYCLE*CHANNEL_NUM*sizeof(int), &i2s_bytes_write, 100);

    free(samples_data);
}

void app_main(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,
        .chan_mask = CHANNEL_MASK,
        .channel_format = I2S_CHANNEL_FMT_MULTIPLE,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = 3,
        .dma_buf_len = 147,
//        .dma_desc_num = 3,
//        .dma_frame_num = 147,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
    };
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_MCK_IO,
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO                                               //Not used
    };
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);

    int test_bits = 24;
    while (1) {
        setup_triangle_sine_waves(test_bits);
        vTaskDelay(5000/portTICK_RATE_MS);
    }

}
