/* "fake" TDM Example

    This example code will output 100Hz sine wave and triangle wave to 4, 8, or 16 channels in a "fake" TDM mode (running i2s sample rate at 2x, 4x, or 8x)

    This application requires a receiving device to understand the data format and convert to the correct TDM signal

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

/* Define IOs and peripheral number */

#define I2S_NUM         (0)
#define I2S_MCK_IO      (GPIO_NUM_0)
#define I2S_BCK_IO      (GPIO_NUM_4)
#define I2S_WS_IO       (GPIO_NUM_5)
#define I2S_DO_IO       (GPIO_NUM_18)
#define I2S_DI_IO       (-1)

/* Begin modifiable parameters */

#define SAMPLE_RATE     (44100) // number of audio frames per second
#define SAMPLE_WIDTH    (32)    // width in bits of each sample (16, 24, 32)
#define CHANNEL_WIDTH   (32)    // width in bits of each channel (16, 24, 32); typically "24-bit" TDM is still 32 bits wide
#define CHANNEL_NUM     (8)     // number of channels in each frame (4, 8, 16)

#define TEST_DATA       (1)     // 0: triangle/sine wave in L/R pairs
                                // 1: sample data is hardcoded to a specific value per channel

#define WAVE_FREQ_HZ    (100)

/* End modifiable parameters. The rest of the defines are derived from the above */

#define FRAMES_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)
#define BYTES_PER_FRAME (CHANNEL_NUM * CHANNEL_WIDTH/8)

#if CHANNEL_NUM == 4
#define FAKE_SAMPLE_RATE (SAMPLE_RATE * 2)
#elif CHANNEL_NUM == 8
#define FAKE_SAMPLE_RATE (SAMPLE_RATE * 4)
#elif CHANNEL_NUM == 16
#define FAKE_SAMPLE_RATE (SAMPLE_RATE * 8)
#else
#error Channel numbers supported: 4, 8, 16
#endif

#define PI              (3.14159265)

static const char* TAG = "tdm_example";

static void setup_channel_test_values(int bits)
{
    int *samples_data = malloc(FRAMES_PER_CYCLE*BYTES_PER_FRAME);
    unsigned int i;
    double sin_float, triangle_float, triangle_step = (double) pow(2, bits) / FRAMES_PER_CYCLE;
    size_t i2s_bytes_write = 0;

    printf("\r\nTest bits=%d free mem=%d, written data=%d\n", bits, esp_get_free_heap_size(), FRAMES_PER_CYCLE*BYTES_PER_FRAME);

    triangle_float = -(pow(2, bits)/2 - 1);

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
            /* our fake TDM signal uses the lower 8 bits for channel number so the receiving device will know how to align the channels and produce the correct TDM signal */
            for (int j = 0; j < CHANNEL_NUM; j += 2)
            {
                samples_data[i*CHANNEL_NUM + j] = ~(0xFFFFFFFF >> (j+1));
                samples_data[i*CHANNEL_NUM + j] |= j;
                samples_data[i*CHANNEL_NUM + j + 1] = ~(0xFFFFFFFF >> (j+2));
                samples_data[i*CHANNEL_NUM + j + 1] |= j + 1;
            }
        }
    }
    ESP_LOGI(TAG, "set clock");
    i2s_set_clk(I2S_NUM, FAKE_SAMPLE_RATE, (CHANNEL_WIDTH << 16) | bits, I2S_CHANNEL_STEREO);

    ESP_LOGI(TAG, "write data");
    i2s_write(I2S_NUM, samples_data, FRAMES_PER_CYCLE*BYTES_PER_FRAME, &i2s_bytes_write, 100);

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
    i2s_set_clk(I2S_NUM, FAKE_SAMPLE_RATE, (CHANNEL_WIDTH << 16) | bits, I2S_CHANNEL_STEREO);

    ESP_LOGI(TAG, "write data");
    i2s_write(I2S_NUM, samples_data, FRAMES_PER_CYCLE*BYTES_PER_FRAME, &i2s_bytes_write, 100);

    free(samples_data);
}

void app_main(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = FAKE_SAMPLE_RATE,
        .bits_per_sample = SAMPLE_WIDTH,
        .bits_per_chan = CHANNEL_WIDTH,
//        .chan_mask = CHANNEL_MASK,
//        .channel_format = I2S_CHANNEL_FMT_MULTIPLE,
//        .communication_format = I2S_COMM_FORMAT_PCM_SHORT, // must be PCM_SHORT now, i guess?
        .mclk_multiple = 512 / (FAKE_SAMPLE_RATE / SAMPLE_RATE), // force mclk to 22.5mhz
        .dma_buf_count = 3,
        .dma_buf_len = 147,
//        .dma_desc_num = 3,
//        .dma_frame_num = 147,
//        .use_apll = false,
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

    int test_bits = SAMPLE_WIDTH;
    while (1)
    {
#if TEST_DATA == 0
        setup_triangle_sine_waves(test_bits);
#else
        setup_channel_test_values(test_bits);
#endif
        vTaskDelay(5000/portTICK_RATE_MS);
    }

}
