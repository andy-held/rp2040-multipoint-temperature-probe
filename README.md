# Pico 1-Wire Temperature sensor

This project uses the [RP2040](https://www.raspberrypi.com/products/raspberry-pi-pico/), [DS18B20](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf) and MQTT to create a multi-point temperature probe.

It uses MQTT to send temperature data and features a C++ implementation of the 1-Wire algorithm based on stefanalt's [RP2040-PIO-1-Wire-Master](https://github.com/stefanalt/RP2040-PIO-1-Wire-Master).

## Dependencies

- [Pico SDK](https://github.com/raspberrypi/pico-sdk)

## Building

Clone this repository:

```bash
git clone  https://github.com/andy-held/RP2040-PIO-1-Wire-Master.git
```

Set `PICO_SDK_PATH` to the Pico SDK path.

Execute CMake & build.
