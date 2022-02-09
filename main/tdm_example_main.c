/* TDM8 Example

    This example code will output 100Hz sine wave and triangle wave to 4, 8, or 16 channels in TDM mode

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
#include "esp_rom_gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "hal/i2s_hal.h"
#include <math.h>

#if !SOC_I2S_SUPPORTS_TDM
#error Must build for a target that supports TDM (e.g. ESP32S3)
#else

/* Define IOs and peripheral number */

#define I2S0_MCK_IO     (GPIO_NUM_5)
#define I2S0_BCK_IO     (GPIO_NUM_6)
#define I2S0_WS_IO      (GPIO_NUM_4)
#define I2S0_DO_IO      (GPIO_NUM_7)
#define I2S0_DI_IO      (-1)

/* BCK and WS use the same pins as I2S0 and are hooked up internally to the other I2S peripheral through the matrix after setting the pin config */
#define I2S1_BCK_IO     (GPIO_NUM_6)
#define I2S1_WS_IO      (GPIO_NUM_4)
#define I2S1_DO_IO      (GPIO_NUM_40)
#define I2S1_DI_IO      (-1)

/* Begin modifiable parameters */

#define SAMPLE_RATE     (44100) // number of audio frames per second
#define SAMPLE_WIDTH    (32)    // width in bits of each sample (16, 24, 32)
#define CHANNEL_WIDTH   (32)    // width in bits of each channel (16, 24, 32); typically "24-bit" TDM is still 32 bits wide
#define CHANNEL_NUM     (4)     // number of channels in each frame (4, 8, 16)

#define TEST_DATA       (1)     // 0: triangle/sine wave in L/R pairs
                                // 1: sample data is hardcoded to a specific value per channel

#define WAVE_FREQ_HZ    (100)   // if TEST_DATA is 0

/* End modifiable parameters. The rest of the defines are derived from the above */

#define FRAMES_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)
#define BYTES_PER_FRAME (CHANNEL_NUM * CHANNEL_WIDTH/8)

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

#define PI              (3.14159265)

static const char* TAG = "tdm_example";

static void setup_channel_test_values(int bits)
{
    int *samples_data = malloc(FRAMES_PER_CYCLE*BYTES_PER_FRAME);
    unsigned int i;
    size_t i2s_bytes_write = 0;

    printf("\r\nTest bits=%d free mem=%d, written data=%d\n", bits, esp_get_free_heap_size(), FRAMES_PER_CYCLE*BYTES_PER_FRAME);

    for(i = 0; i < FRAMES_PER_CYCLE; i++)
    {
        /* align so that channel 1 has the leftmost bit high and the rest low, channel 2 has the second two bits from the left high and the rest low, etc. */
        if (bits == 16)
        {
            /* stuff two samples into a single 32-bit int */
            for (int j = 0; j < CHANNEL_NUM; j += 2)
            {
                unsigned int sample_val = ~(0xFFFF >> (j+1));
                sample_val = sample_val << 16;
                sample_val += ~(0xFFFF >> (j+2));
                samples_data[i*CHANNEL_NUM + j] = sample_val;
            }
        }
        else
        {
            /* lower 8 bits are unused for 24-bit, but it's still the same 32 bits of data being written to i2s driver either way */
            for (int j = 0; j < CHANNEL_NUM; j += 2)
            {
                samples_data[i*CHANNEL_NUM + j] = ~(0xFFFFFFFF >> (j+1));
                samples_data[i*CHANNEL_NUM + j + 1] = ~(0xFFFFFFFF >> (j+2));
            }
        }
    }
    ESP_LOGI(TAG, "set clock");
    i2s_set_clk(0, SAMPLE_RATE, (CHANNEL_WIDTH << 16) | bits, CHANNEL_MASK);

    ESP_LOGI(TAG, "write data");
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_write(0, samples_data, FRAMES_PER_CYCLE*BYTES_PER_FRAME, &i2s_bytes_write, 100));
    ESP_LOGI(TAG, "wrote %d to i2s 0", i2s_bytes_write);
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_write(1, samples_data, FRAMES_PER_CYCLE*BYTES_PER_FRAME, &i2s_bytes_write, 100));
    ESP_LOGI(TAG, "wrote %d to i2s 1", i2s_bytes_write);

    free(samples_data);
}

static void setup_triangle_sine_waves(int bits)
{
    int *samples_data = malloc(FRAMES_PER_CYCLE*BYTES_PER_FRAME);
    unsigned int i;
    double sin_float, triangle_float, triangle_step = (double) pow(2, bits) / FRAMES_PER_CYCLE;
    size_t i2s_bytes_write = 0;

    printf("\r\nTest bits=%d free mem=%d, written data=%d\n", bits, esp_get_free_heap_size(), FRAMES_PER_CYCLE*BYTES_PER_FRAME);

    triangle_float = -(pow(2, bits)/2 - 1);

    for(i = 0; i < FRAMES_PER_CYCLE; i++)
    {
        sin_float = sin(i * 2 * PI / FRAMES_PER_CYCLE);
        if(sin_float >= 0)
            triangle_float += triangle_step;
        else
            triangle_float -= triangle_step;

        sin_float *= (pow(2, bits)/2 - 1);

        if (bits == 16)
        {
            /* stuff left (triangle wave) and right (sine wave) into a single 32-bit int */
            unsigned int sample_val = (short)triangle_float;
            sample_val = sample_val << 16;
            sample_val += (short) sin_float;
            for (int j = 0; j < CHANNEL_NUM/2; j++)
            {
                samples_data[i*CHANNEL_NUM + j] = sample_val;
            }
        }
        else if (bits == 24)
        {
            /* lowest 8 bits unused */
            for (int j = 0; j < CHANNEL_NUM; j += 2)
            {
                samples_data[i*CHANNEL_NUM + j] = ((int) triangle_float) << 8;
                samples_data[i*CHANNEL_NUM + j + 1] = ((int) sin_float) << 8;
            }
        }
        else
        {
            for (int j = 0; j < CHANNEL_NUM; j += 2)
            {
                samples_data[i*CHANNEL_NUM + j] = ((int) triangle_float);
                samples_data[i*CHANNEL_NUM + j + 1] = ((int) sin_float);
            }
        }

    }
    ESP_LOGI(TAG, "set clock");
    i2s_set_clk(0, SAMPLE_RATE, (CHANNEL_WIDTH << 16) | bits, CHANNEL_MASK);

    ESP_LOGI(TAG, "write data");
    i2s_write(0, samples_data, FRAMES_PER_CYCLE*BYTES_PER_FRAME, &i2s_bytes_write, 100);
    ESP_LOGI(TAG, "wrote %d to i2s 0", i2s_bytes_write);
    i2s_write(1, samples_data, FRAMES_PER_CYCLE*BYTES_PER_FRAME, &i2s_bytes_write, 100);
    ESP_LOGI(TAG, "wrote %d to i2s 1", i2s_bytes_write);

    free(samples_data);
}

void app_main(void)
{
    i2s_config_t i2s0_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = SAMPLE_WIDTH,
        .bits_per_chan = CHANNEL_WIDTH,
        .chan_mask = CHANNEL_MASK,
        .channel_format = I2S_CHANNEL_FMT_MULTIPLE,
        .communication_format = I2S_COMM_FORMAT_PCM_SHORT, // must be PCM_SHORT now, i guess?
        .mclk_multiple = 512, // must be at least 512 so bck divider is at least 2
        .dma_buf_count = 3,
        .dma_buf_len = 147,
//        .dma_desc_num = 3,
//        .dma_frame_num = 147,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
    };
    i2s_config_t i2s1_config = {
        .mode = I2S_MODE_SLAVE | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = SAMPLE_WIDTH,
        .bits_per_chan = CHANNEL_WIDTH,
        .chan_mask = CHANNEL_MASK,
        .channel_format = I2S_CHANNEL_FMT_MULTIPLE,
        .communication_format = I2S_COMM_FORMAT_PCM_SHORT, // must be PCM_SHORT now, i guess?
        .mclk_multiple = 512, // must be at least 512 so bck divider is at least 2
        .dma_buf_count = 3,
        .dma_buf_len = 147,
//        .dma_desc_num = 3,
//        .dma_frame_num = 147,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
    };
    i2s_pin_config_t i2s0_pin_config = {
        .mck_io_num = I2S0_MCK_IO,
        .bck_io_num = I2S0_BCK_IO,
        .ws_io_num = I2S0_WS_IO,
        .data_out_num = I2S0_DO_IO,
        .data_in_num = I2S0_DI_IO                                               //Not used
    };
    i2s_pin_config_t i2s1_pin_config = {
        .bck_io_num = I2S1_BCK_IO,
        .ws_io_num = I2S1_WS_IO,
        .data_out_num = I2S1_DO_IO,
        .data_in_num = I2S1_DI_IO                                               //Not used
    };

    i2s_driver_install(0, &i2s0_config, 0, NULL);
    i2s_set_pin(0, &i2s0_pin_config);
    i2s_driver_install(1, &i2s1_config, 0, NULL);
    i2s_set_pin(1, &i2s1_pin_config);

    /* Connnect BCLK signals together */
    esp_rom_gpio_connect_out_signal(I2S0_BCK_IO, i2s_periph_signal[0].m_tx_bck_sig, 0, 0);
    esp_rom_gpio_connect_in_signal(I2S1_BCK_IO, i2s_periph_signal[1].s_rx_bck_sig, 0);
    /* Connect WS signals together */
    esp_rom_gpio_connect_out_signal(I2S0_WS_IO, i2s_periph_signal[0].m_tx_ws_sig, 0, 0);
    esp_rom_gpio_connect_in_signal(I2S1_WS_IO, i2s_periph_signal[1].s_rx_ws_sig, 0);

    int test_bits = SAMPLE_WIDTH;
    while (1)
    {
#if TEST_DATA == 0
        setup_triangle_sine_waves(test_bits);
#else
        setup_channel_test_values(test_bits);
#endif
        ESP_LOGI(TAG, "See you again in 5 seconds");
        vTaskDelay(5000/portTICK_RATE_MS);
    }

}

#endif
