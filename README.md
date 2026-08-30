# Energy-Efficient IoT module with nRF52840

**LoRa / SDI-12 + BLE version (C++)**

Embedded firmware for an **Environmental monitoring system**. 
An IoT device for measuring **Soil and water parameters**: volumetric water content, electrical conductivity, water level, and sending the measurements to the cloud for further processing of the data. 

---

## Overview

- MCU: **nRF52840 Dongle** running **Zephyr RTOS** (tested with nRF Connect SDK 3.2.1)
- Sensors: **RuuviTags** (BLE, temperature/humidity/pressure) + **SDI-12 sensors**: Teros12, Solyx14, Solinst (Measuring soil/water parameters)
- Connectivity: **LoRa** (current target) — A7670G 4G LTE modem (alternative path)
- Data encoding: **Nano Protobuf** for binary data encoding/decoding and **JSON** for 4G modem path
- Cloud: **Google Cloud Pub/Sub** via Digita LoRaWAN network (alternatively JWT for 4G modem)

---

## Hardware

| Component         | Description                                      |
|-------------------|--------------------------------------------------|
| nRF52840 Dongle   | BLE-capable MCU with USB                         |
| LoRa E5 module    | LoRa E5 module, communication via UART "AT"      |
| RuuviTags         | BLE beacons — temperature, humidity, pressure    |
| Solyx 14          | SDI-12 soil sensor — VWC, temperature, EC        |
| Solinst           | SDI-12 water sensor - temperature, level         |
| Power Source      | 12V battery                                      |

### Wiring — SDI-12

| nRF52840 pin  | Signal        |
|---------------|---------------|
| P0.09         | SDI12 TX      |
| P0.10         | SDI12 RX      |
| P1.00         | Break (GPIO)  | 

### Wiring — LoRa module

| nRF52840 pin  | Signal  | LoRa module pin |
|---------------|---------|-----------------|
| 0.20          | UART TX | LoRa RX         |
| 0.24          | UART RX | LoRa Tx         |

---

## Architecture

```
RuuviTag (BLE)    --> ruuvi_scanner   --> transmit queue
Solyx14, Solinst (SDI-12) --> sdi12_scanner   --> transmit queue

transmit queue --> CloudSender --> [LoRaTransport | ModemTransport]
                                                  |
                                        Google Cloud Pub/Sub
```

### Key classes

| Class / File              | Responsibility                                      |
|---------------------------|-----------------------------------------------------|
| `LoRaTransport`           | LoRaWAN state machine, join, clock sync, send       |
| `ModemTransport`          | 4G path                                             |
| `CloudSender`             | Wakes periodically, batches queue items, calls transport |
| `BinarySerializer`        | Encodes sensor_data structs for LoRa payload (nanopb) |
| `JsonSerializer`          | JSON encoding for 4G / Pub/Sub path                 |
| `RuuviScanner`            | BLE passive scan thread                             |
| `Sdi12Scanner`            | SDI-12 poll thread                                  |
| `Sdi12Bus`                | SDI-12 physical layer (UART + break signal)         |
| `JwtBuilder`              | RS256 JWT via mbedTLS (4G OAuth2)                   |
| `NvStorage`               | NVS flash — RuuviTag MACs, LoRa AppKey              |

---

## Source Layout

```
src/
  main.cpp
  sensors/
    ruuvi_scanner.cpp
    sdi12_scanner.cpp
  hardware/
    sdi12_bus.cpp
  transport/
    jwt_builder.cpp
    lora_statemachine.cpp
    lora_transport.cpp
    modem_transport.cpp
  d_pipeline/
    cloud_sender.cpp
  serialization/
    binary_serializer.cpp
    json_serializer.cpp
  storage/
    nv_storage.cpp
  utils/
    shell.cpp
    system_clock.cpp

```

---

## Configuration

Key Kconfig flags in `prj.conf`:

| Flag                    | Effect                                    |
|-------------------------|-------------------------------------------|
| `CONFIG_CLOUD_SEND_LORA=y`| Active transport: LoRaWAN (default)     |
| `CONFIG_CLOUD_SEND_4G=y`  | Inactive transport: 4G modem path       |
| `CONFIG_SDI12=y`          | Enable SDI-12 thread                    |

---

## Dependencies

- nRF Connect SDK v3.2.1
- nRF Connect SDK Toolchain v3.2.1

---

## Build in Visual Studio Code

1. Install the **nRF Connect** extension in VS Code.
2. Through the extension, install the nRF toolchain and SDK (both v3.2.1+).
3. Click **"Open Existing Application"** and open the cloned folder.
4. Select the application and click **"Add Build Configuration"**:
   - Board target: `nrf52840dongle/nrf52840`
   - Base configuration files: `prj.conf` & `prj_extended.conf`
5. Click **"Generate and Build"**.

---

## Shell Commands

Connect via USB serial (115200 baud) to access the runtime shell:

| Command               | Description                                       |
|-----------------------|---------------------------------------------------|
| `tag add <MAC>`       | Add a RuuviTag MAC address                        |
| `tag write`           | Save tag list to NVS flash (needs to be called after 'tag add')   |
| `tag list`            | List stored RuuviTag MACs                         |
| `tag clear`           | Clear all stored tags                             |
| `transmit`            | Trigger immediate cloud send                      |
| `sample`              | Trigger immediate sensor measurement              |

---
