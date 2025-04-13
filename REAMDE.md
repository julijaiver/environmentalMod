# 🌱 Energy-Efficient IoT module with nRF52840

This is a cloud-connected, ultra-low-power embedded system designed to collect environmental data from **RuuviTags (via BLE)** and **TEROS 12 soil sensors (via SDI-12)**. It transmits the data securely over a **4G CAT-1 LTE module (A7670G)** to **Google Cloud Pub/Sub**, using embedded **RSA SHA-256 JWT signing** for authentication.

---

## 🚀 Features

- 🔗 **BLE scanner for RuuviTags** (temperature, humidity, pressure, etc.)
- 🌱 **SDI-12 communication** with TEROS 12 soil sensors (moisture, temperature, EC)
- 🔐 **RSA SHA-256 JWT signing** directly on-device for Google Cloud authentication
- ☁️ **Data publishing to Google Cloud Pub/Sub** over HTTPS (via AT commands)
- 📡 **4G connectivity** using A7670G module with AT+HTTP commands
- ⚡ **Energy-optimized design** for remote, battery-powered deployments
- 🛠️ Runs on **Zephyr RTOS** for stability and real-time control

---

## 📡 Hardware

| Component        | Description                                    |
|------------------|-------------------------------------------------|
| nRF52840 Dongle  | BLE-capable MCU with USB                        |
| A7670G Module    | 4G LTE CAT-1 modem, AT-command interface        |
| TEROS 12         | Soil sensor using SDI-12 protocol               |
| RuuviTags        | BLE beacons for environmental sensing           |
| Power Source     | Powerbank                                       |

---

## 🧠 Architecture

```text
┌────────────┐
│  RuuviTags │ ◀── BLE Scan ──┐
└────────────┘                 │
                               ▼
                         ┌────────────┐        ┌────────────────────┐
TEROS 12 ◀── SDI-12 ───▶│  nRF52840  │ ────▶ │    A7670G Modem    │
                         │  (Zephyr)  │        └────────┬───────────┘
                         └─────┬──────┘                 ▼
                         JWT, AT+HTTP           Google Cloud Pub/Sub
```
---

## 📦 Dependencies

Nordic Semiconductor
- nRF Connect SDK v2.9.0
- nRF Connect SDK Toolchain V2.9.0

## 🛠️ Build in Visual Studio Code

To build this project in VScode you need nRF Connect extension. After installing the extension it will ask to install toolchain and SDK, both versions need to be min 2.9.0.

Steps to build:
1. Install nRF Connect extension in VScode
2. install trough extension nRF toolchain and SDK, both min 2.9.0
3. Click open existing application and open the folder you cloned from git
4. From extension window select the application and click "Add build configuration"
5. In build configuration select
    - Board target: nrf52840dongle/nrf52840
    - Base configuration files: prj.conf & prj_extended.conf
6. Then click "Generate and Build"

---

## 🔌 Flashing via nRF Connect for Desktop

If you are not using J-Link debugger to flash then you flash the firmware using the **nRF Connect for Desktop** tool.

### 🧰 Requirements

- **nRF Connect for Desktop**: [Download here](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-desktop)
- **Programmer App**: Install this inside nRF Connect for Desktop
- **nRF52840 Dongle**
- Compiled `.hex` file

---

### 🔧 Steps

1. **Plug in the Dongle**
   - Insert your nRF52840 Dongle into a USB port.
   - Press the **reset button** while inserting the dongle to enter **DFU (bootloader) mode**.
   - The RGB LED should start **pulsing red** to indicate DFU mode.

2. **Open Programmer App**
   - Launch **nRF Connect for Desktop**
   - Open the **Programmer** application.

3. **Select Device**
   - In the left sidebar, you should see the dongle listed (e.g., `nRF52840 USB DFU`).
   - Select it.

4. **Load Firmware**
   - Click **“Add HEX file”** or **“Browse”** to locate and select your compiled firmware file:
   - Use the `merged.hex` file from the `build/` directory.

5. **Write to Device**
   - Click **“Write”** to flash the firmware.
   - Wait for the progress bar to complete. You’ll see “Successfully written” in the log.

6. **Reset**
   - After flashing, the device will automatically reboot and run your firmware.

---

