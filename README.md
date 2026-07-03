# Footsie

Footsie is a configurable foot pedal controller built around an ESP32-S3, a slide potentiometer input, and a 16-bit DAC80501 output over I2C.

It reads pedal position, applies a tunable response curve, smooths the input, and outputs an analog control voltage for a connected machine or controller.

## Screenshots

<a href="images/Footsie_App.jpg"><img src="images/Footsie_App.jpg" width="300" alt="Footsie App"></a>
<a href="images/Footsie_ADC_Calibration.jpg"><img src="images/Footsie_ADC_Calibration.jpg" width="300" alt="Footsie ADC Calibration"></a>

## Why?

I originally built Footsie for a pottery / ceramics wheel I made for my wife. The commercially available foot pedals I tried were not quite right: low-speed control was poor, the pedal travel was not used effectively and the response did not feel natural.

Footsie exists to solve that problem.

The goal is to provide a foot pedal that can be tuned to behave exactly how you want, especially where fine low-speed control matters.

Yes, this is absolutely overkill.

Just the way i like it.

## Features

- Analog slide potentiometer pedal input on ADC channel 2 (GPIO3)
- Continuous ADC sampling with averaging and clamped calibration range
- Configurable gamma response curve, persisted in NVS
- Configurable ADC calibration min/max (mV), persisted in NVS
- Configurable output scaling min/max (mV), persisted in NVS
- 16-bit DAC80501 I2C output stage
- Fail-safe zero-output behavior when ADC/DAC mapping is invalid
- WS2812 status LED feedback
- Custom BLE GATT service for live telemetry and configuration

## Purpose

Footsie is not a full motor controller. It is a configurable analog control pedal intended to replace or improve simple potentiometer-style pedals where default response is poorly matched to the application.

**Possible Use Cases:**
- Pottery / ceramics wheels
- Small motor speed control systems
- Sewing machine-style speed pedals
- E-bike-style throttle signal replacement or testing
- Variable analog control inputs
- Custom machine controls
- Any project where a nicer foot-controlled analog voltage is useful

Check the requirements of the device being controlled before connecting Footsie. Not all pedal inputs or throttle inputs are electrically compatible.

## Hardware

Footsie currently uses these major components:

* ESP32-S3 microcontroller
* DAC80501 16-bit DAC over I2C
* Slide potentiometer as the pedal input
* WS2812B RGB LED for status indication

### Pin Map

The current firmware pin assignments are:

| Function | GPIO |
| --- | --- |
| I2C SDA | 1 |
| I2C SCL | 2 |
| ADC input | 3 |
| RGB LED | 21 |
| UART0 TX | 43 |
| UART0 RX | 44 |

## Firmware Defaults

| Setting | Value |
| --- | --- |
| ADC reference voltage | 3300 mV |
| DAC full-scale output target | 5000 mV |
| DAC update period | 20 ms |
| ADC task period | 10 ms |
| BLE poll period | 1000 ms |
| Debug log period | 1000 ms |
| Default gamma | 2.2 |
| Gamma range | 0.50 to 5.00 |
| Default ADC calibrated minimum | 139 mV |
| Default ADC calibrated maximum | 3181 mV |
| Default output minimum | 0 mV |
| Default output maximum | 5000 mV |

## Schematic

<a href="docs/schematic/Footsie_Schematic_V1.0.pdf"><img src="images/footsie_schematic_v1.0.png" width="100%" alt="Footsie Schematic"></a>

![Footsie Schematic V1.0](docs/schematic/Footsie_Schematic_V1.0.pdf)

## BLE Interface

Footsie advertises with device name Footsie and exposes one custom 128-bit service.

Service UUID:

- 4a8c0000-6d79-879d-184b-4b769c5b1e50

Characteristics:

| Characteristic | UUID | Access | Payload |
| --- | --- | --- | --- |
| Gamma | 4a8c0001-6d79-879d-184b-4b769c5b1e50 | Read/Write | u16, gamma x100 (220 = 2.20) |
| ADC calibration | 4a8c0003-6d79-879d-184b-4b769c5b1e50 | Read/Write | two u16 values: min_mV, max_mV |
| Output scaling | 4a8c0004-6d79-879d-184b-4b769c5b1e50 | Read/Write | two u16 values: min_mV, max_mV |
| ADC mV | 4a8c0002-6d79-879d-184b-4b769c5b1e50 | Read/Notify | u16 latest averaged ADC mV |

Behavior notes:

- Gamma write range: 0.50 to 5.00
- ADC calibration requires min < max and max <= 3300
- Output scaling requires min < max and max <= 5000
- Advertising restarts automatically after disconnect/adv-complete
- If a client disables ADC notifications, firmware terminates that BLE connection

## Configuration and Persistence

The following values are stored in NVS and survive reboot:

- Gamma
- ADC calibrated min and max (mV)
- Output scaling min and max (mV)

If values are missing or invalid in NVS, firmware falls back to compile-time defaults.

## Status LED

The WS2812 LED indicates runtime state:

- Green intensity tracks applied DAC output level
- Blue is lit while BLE is connected
- Startup briefly shows green when initialization completes

## Build, Flash, Monitor

### Prerequisites

- ESP-IDF installed (project currently tested with ESP-IDF 6.x toolchains)
- Target set to ESP32-S3
- Serial access to your board (Linux users may need dialout/uucp group membership)

### Fast Path (script)

From repository root:

./run.sh

The script:

- Initializes ESP-IDF environment if needed
- Runs idf.py build
- Runs idf.py -p <PORT> flash monitor

Defaults and overrides:

- Default port: /dev/ttyACM0
- Override port: PORT=/dev/ttyUSB0 ./run.sh
- Default ESP-IDF export path: $HOME/esp/esp-idf/export.sh
- Override export path: ESP_IDF_EXPORT_SH=$HOME/.espressif/v6.0.1/esp-idf/export.sh ./run.sh

### Manual Path

If you prefer direct commands:

1. Source your ESP-IDF environment.
2. From repository root, run:

idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor

## Development Notes

- ADC sampling and control output run in separate FreeRTOS tasks
- Control loop drives DAC to zero when input is invalid
- Mapping telemetry is rate-limited and change-triggered
- BLE housekeeping runs periodically and handles stale connection behavior

## Troubleshooting

- ESP-IDF not found:
	- Set ESP_IDF_EXPORT_SH to your local export.sh path, or initialize IDF in your shell before running the script.
- Serial port busy or missing:
	- Verify the board port and update PORT.
- Permission denied on serial device:
	- Add your user to the proper serial-access group and re-login.

## Safety Disclaimer

Footsie is provided as an experimental DIY project with no warranty or guarantee of safety or fitness for any particular purpose. Use it at your own risk.

Footsie provides a control voltage only. It does not provide:

- Electrical isolation
- Motor protection
- Over-current protection
- Emergency stop handling
- Mains safety features
- Mechanical guarding
- Certification for any particular machine or application

You are responsible for ensuring any system using Footsie is safe, suitable, and compliant with applicable laws and regulations.

## License

MIT License. See LICENSE.
