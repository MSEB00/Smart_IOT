# Smart_IOT

ESP32 Smart IoT relay control project using a 74HC595 shift register.

## Overview

This repository contains the `Smart_IOT.ino` Arduino sketch for an ESP32-based relay control board. The sketch hosts a simple web server and renders a web UI for toggling 26 relay channels.

## Features

- ESP32 WiFi web server
- Web UI with toggle switches for each relay
- Shift register control over 26 relay outputs
- Dynamic state updates and JSON response endpoints

## Hardware

- ESP32 development board
- 74HC595 shift register (or daisy-chained registers if needed)
- Relays driven from the shift register outputs
- `dataPin = 13`, `clockPin = 14`, `latchPin = 15`

## Usage

1. Open `Smart_IOT/Smart_IOT.ino` in the Arduino IDE.
2. Configure the ESP32 board and serial port.
3. Update `ssid` and `password` to your WiFi credentials.
4. Upload the sketch to the ESP32.
5. Open the serial monitor at `115200` baud to see the IP address.
6. Visit `https://smart_iot.local` or the ESP32 IP address in a browser.

## Notes

- The project currently hardcodes WiFi credentials in source for quick testing.
- The web server now serves HTTPS with a self-signed certificate; browsers will warn on first visit, but the connection is encrypted.
- The shift register update logic assumes 26 relay channels and sends 4 bytes to the register.
- This repository is intended for development and hardware verification with ESP32.
