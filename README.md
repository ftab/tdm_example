| Supported Targets | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- |

# TDM Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

In this example, we generate a 100Hz triangle and sine wave and send it to 8 channels at a sample rate of 44.1kHz through the I2S bus in TDM mode.

## How to Use Example

### Hardware Required

* A development board with ESP32-S3 or ESP32-C3 SoC (e.g., ESP32-S3-DevKitC, ESP32-C3-DevKitM, etc.)
* A USB cable for power supply and programming

### Configure the Project

```
idf.py set-target esp32s3
idf.py menuconfig
```
or
```
idf.py set-target esp32c3
idf.py menuconfig
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

```

Test bits=24 free mem=361704, written data=14112
I (128) tdm_example: set clock
I (128) tdm_example: write data

Test bits=24 free mem=362816, written data=14112
I (5178) tdm_example: set clock
I (5178) tdm_example: write data

Test bits=24 free mem=362816, written data=14112
I (10228) tdm_example: set clock
I (10228) tdm_example: write data
```

If you have a logic analyzer, you can use a logic analyzer to grab online data. The following table describes the pins we use by default (Note that you can also use other pins for the same purpose).

| pin name| function        | gpio_num   |
|:-------:|:---------------:|:----------:|
| WS      |word select      | GPIO_NUM_4 |
| MCK     |main system clock| GPIO_NUM_5 |
| SCK/BCK |shift/"bit" clock| GPIO_NUM_6 |
| SD      |serial data      | GPIO_NUM_7 |

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
