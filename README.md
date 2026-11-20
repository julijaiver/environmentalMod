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

Software architecture follows decoupled **Publish/Subscribe** strategy, where data flows through dedicated topics enabling sensors, serializers and transport methods to be added independently. 

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

### Adding a new sensor:
Every sensor requires:
1.	A dedicated data struct in data_types.hpp.
2.	A corresponding **`Topic<NewSensorStruct>`** instantiated in main.cpp and passed to the sensor scanner and to serializer.
#### Case A: Sensor uses SDI-12 protocol: 
If the new sensor supports SDI-12 protocol, it should be integrated directly into **SDI12Scanner** (sensor_scan/sdi12_scan).
1.	Define the data type by adding the measurement struct to data_types.hpp and create the corresponding topic in main.cpp. 
2.	Pass the new topic to SDI12Scanner (update the SDI12Scanner class to accept the new sensor topic and to hold corresponding Publisher instance).
3.	Inside **SDI12Scanner::scan_sensor()** add new sensor entry to **`std::array<known_sensor>`** defining: 
   -sensor ID
   -sensor type
   -sn_reader function pointer for parsing the serial number from the command respond
4.	Implement measurement reading by adding a dedicated **`read_<newSensor>()`** method inside SDI12Scanner to get measurement values and publish them via Publisher. 
5.	Add a case in scanner’s state machine to invoke new read_<newSensor>() method during wakeup.
 
*(RuuviTag follows the same topic + publisher pattern inside the RuuviScanner class)* 
#### Case B: Sensor uses different protocol (I2C, SPI, etc):
1.	Define sensor’s data struct in data_types.hpp and instantiate its topic in main.cpp.
2.	Create a new sensor scanner class that takes the corresponding sensor topic and owns a Publisher instance. 
3.	Implement protocol specific polling/reading logic and publish measurement values to the topic. 
4.	Instantiate the new scanner in main.cpp. 

### Adding a new transport method: 
All transport methods must implement the **ITransport interface** (includes/interfaces/itransport.hpp).
```cpp
class NewTransport : public ITransport {
public:
        int connect() override;
        int disconnect() override;
        int send(const uint8_t *buf, int len) override;
        int send_log(const uint8_t *buf, int len) override; 
        int max_payload() override; 
} 
```
1.	Add a build selection macro in main.cpp (#ifdef USE_NEW_TRANSPORT) to create new transport instance pointer.
2.	Pass the instance pointer into the CloudSender, which uses it through ITransport interface. 

### Adding a new serialization method: 
All serializers inherit common **DataSubscriber class** to pack pending sensor data in CloudSender ready to be sent with corresponding transport method.
```cpp
class NewSerializer : public DataSubscriber {
        using DataSubscriber::DataSubscriber;
        int pack(uint8_t *buf, int max_len) override; 
}
```
1.	Inherit from DataSubscriber (includes/serialization/data_subscriber.hpp) which already inherits from **ISerializer** and implements its pending() method -> new serializer class only needs to reuse the DataSubscriber’s constructor and override pack() method. 
2.	In main.cpp create your new serializer instance pointer by passing all active sensor topics into the inherited DataSubscriber constructor and then pass the new serializer instance pointer to the CloudSender.

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

## Flash via nRF Connect for Desktop

If you are not using a J-Link debugger, flash the firmware using the nRF Connect for Desktop tool.

**Requirements:**
- [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop)
- Programmer app (install inside nRF Connect for Desktop)
- nRF52840 Dongle
- Compiled `.hex` file

**Steps:**

1. **Enter DFU mode** — insert the dongle into USB while holding the reset button. The RGB LED should pulse red.
2. **Open Programmer** — launch nRF Connect for Desktop and open the Programmer application.
3. **Select device** — choose the dongle from the left sidebar (e.g. `nRF52840 USB DFU`).
4. **Load firmware** — click **"Add HEX file"** and select `build/merged.hex`.
5. **Write** — click **"Write"** and wait for "Successfully written" in the log.
6. The device will reboot automatically and run the new firmware.

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

