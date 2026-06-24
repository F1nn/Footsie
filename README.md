# Footsie

Footsie is a configurable 16-bit DAC foot pedal controller designed for smooth analog speed control.

It uses an analog slide potentiometer as the pedal position input, reads that position with an ADC, applies a configurable response curve, and outputs a filtered analog control voltage using a 16-bit I²C DAC.

**Footsie is currently a work-in-progress.**

## Why?

I originally built Footsie for a pottery / ceramics wheel I made for my wife. The commercially available foot pedals I tried were not quite right: low-speed control was poor, the pedal travel was not used effectively and the response did not feel natural.

Footsie exists to solve that problem.

The goal is to provide a foot pedal that can be tuned to behave exactly how you want, especially where fine low-speed control matters.

Yes, this is absolutely overkill.

Just the way i like it.

## Features

* Analog slide potentiometer pedal input
* Configurable input-to-output response curve
* Smooth filtered output voltage
* 16-bit DAC output controlled over I²C
* Return-to-zero DAC fail-safe behaviour
* Designed for applications where precise foot control is important

## How it works

1. The firmware reads the pedal position from the slide potentiometer using the ADC.
2. The raw input value is mapped through a configurable response curve.
3. The transformed value is filtered to produce a smooth output.
4. The result is sent to a 16-bit DAC over I²C.
5. The DAC outputs an analog control voltage for the connected motor controller or device.
6. If communication or control fails, the DAC output returns to zero as a fail-safe.

## Purpose

Footsie is not intended to be a universal motor controller. It is a configurable analog control pedal intended to replace or improve simple potentiometer-style foot pedals like you would find on an e-bike or sewing machine, particularly where the stock pedal response is too abrupt, too compressed, or poorly matched to the device being controlled.

**Possible Use Cases:**
- Pottery / ceramics wheels
- Small motor speed control systems
- Sewing machine-style speed pedals
- E-bike-style throttle signal replacement or testing
- Variable analog control inputs
- Custom machine controls
- Any project where a nicer foot-controlled analog voltage is useful

Check the requirements of the device being controlled before connecting Footsie. Not all pedal inputs or throttle inputs are electrically compatible.

## 🔌 Schematic

![Footsie Schematic V1.0](docs/schematic/Footsie_Schematic_V1.0.pdf)

## 🔩 Hardware Components

### Power

### ESP32-S3 Microprocessor

* Dual-core Xtensa LX7 @ up to 240 MHz.
* 512 KB SRAM + 384 KB ROM (on-chip) + 8MB PSRAM on module.
* Integrated Wi-Fi 4 (802.11 b/g/n) + Bluetooth 5 (LE).
* USB-C OTG support for power, firmware updates, etc.
* Deep sleep as low as ~5 µA.

### DAC

### Slide Potentiometer

## ✅ TODO

- [ ] Add web/app interface for configuration
- [ ] Add enclosure / pedal mechanical details

The intended configuration options include:

- Pedal minimum input
- Pedal maximum input
- Output minimum voltage
- Output maximum voltage
- Start dead zone
- End dead zone
- Response curve shape
- Filtering / smoothing amount
- Fail-safe behaviour
- Saved profiles

The long-term goal is to make these configurable through a web or app interface, rather than requiring firmware changes for every adjustment.

## ⚡ Building & Flashing

1. **Clone the repository**
```sh
git clone <repository-url>
cd Footsie
```
2. **Set-up ESP-IDF**

Follow the normal ESP-IDF setup process for your operating system and editor.

3. **Build / flash and monitor**

```sh
./run.sh
```

## Safety Disclaimer

Footsie is provided as an experimental DIY project with no warranty or guarantee of safety or fitness for any particular purpose. Use it at your own risk.

Footsie is intended to provide a control voltage only.

It does not provide:

- Electrical isolation
- Motor protection
- Over-current protection
- Emergency stop handling
- Mains safety features
- Mechanical guarding
- Certification for any particular machine or application

You are responsible for ensuring any system using Footsie is safe, suitable, and compliant with applicable laws and regulations.

## 📄 License

MIT License (see LICENSE file)
