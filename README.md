# ESP32-S3 Native USB Capacitive Touch Injector

## Project Overview
This project transforms an ESP32-S3 development board into a native USB Human Interface Device (HID) keyboard. Utilizing a single jumper wire as a capacitive touch sensor, the board acts as a hardware-level keystroke injector (similar to a "USB Rubber Ducky"). When a user taps the exposed wire, the ESP32-S3 instantly types out a pre-programmed string of text or executes macro commands directly into the host operating system.

This project strictly leverages the unique architectural features of the ESP32-S3, specifically its built-in USB-OTG PHY and its advanced capacitive touch peripheral, requiring **zero external hardware components**.

---

## Hardware Setup
The beauty of this project lies in its minimal hardware footprint.

**Requirements:**
* 1x ESP32-S3 Development Board
* 1x Standard Male-to-Male Jumper Wire
* 2x USB Data Cables (Ensure they are data-capable, not charge-only)

**Connections:**
1. **The Sensor:** Strip one end of the jumper wire to expose the bare metal. Plug the other end into **GPIO 1** (mapped to `TOUCH_PAD_NUM1`).
2. **The Debug Pipeline (Cable 1):** Connect the port labeled `UART` or `COM` to your computer. This is used for flashing firmware and viewing live serial logs for calibration.
3. **The Injection Pipeline (Cable 2):** Connect the port labeled `USB` or `OTG` to your computer. This connects directly to the S3's D+/D- pins, allowing the TinyUSB stack to mount the board as a standard keyboard.

---

## System Architecture
The firmware is built on the **ESP-IDF v5.x** framework and relies on three core subsystems working synchronously:

1. **Capacitive Touch FSM:** The internal touch peripheral runs on a hardware timer, continuously measuring the charge/discharge cycles of the GPIO pin. When a human touches the wire, the capacitance increases, causing the cycle count to **drop**.
2. **FreeRTOS Task Scheduler:** A dedicated background task (`touch_keyboard_task`) polls the touch peripheral at 100ms intervals. It handles debouncing, applies the threshold logic, and translates the touch event into a string payload.
3. **TinyUSB Stack (esp_tinyusb v2.x):** The ESP32-S3 bypasses standard UART communication on its OTG port and utilizes the TinyUSB driver. It provides manual Device, Configuration, and HID Report descriptors to the host OS, establishing an explicit contract that the device is a standard keyboard.

---

## Technical Challenges & Debugging
Building a native USB device from scratch presents several low-level challenges. Here are the specific hurdles overcome in this architecture:

* **ESP32-S3 Capacitive Touch Inversion:** Older ESP32 chips register a touch as an *increase* in the raw touch value. The ESP32-S3's upgraded peripheral measures cycle counts, meaning a touch causes the raw value to *decrease*. The threshold logic was inverted (`touch_value < TOUCH_THRESHOLD`), and environmental baseline calibration was implemented to prevent false positives.
* **TinyUSB v5.x Descriptor Panics:** Transitioning to ESP-IDF v5 meant TinyUSB was moved to a component registry. Passing custom String Descriptors caused a `Guru Meditation Error (LoadProhibited)` due to a Null Pointer Dereference in the driver's internal `memcpy_aux` function. Custom string descriptors were abandoned in favor of the framework's highly stable default strings, using the nested `.descriptor.` pointers.
* **Host OS USB Polling Desynchronization:** The ESP32-S3 processes instructions magnitudes faster than the host OS polls the USB port. Blasting signals consecutively resulted in dropped characters. A synchronization loop utilizing `tud_hid_ready()` was injected between every single keystroke, forcing the board to wait for the host OS.

---

## Project Structure
```text
touch_injector/
├── CMakeLists.txt             # Project-level CMake configuration
├── sdkconfig                  # Generated config (contains HID/TinyUSB configs)
├── main/
│   ├── CMakeLists.txt         # Main component CMake configuration
│   └── main.c                 # Application entry point, FreeRTOS tasks, and USB descriptors
└── managed_components/
    └── espressif__esp_tinyusb # Downloaded TinyUSB stack via IDF Component Manager

## How to Test Safely
USB Keystroke injectors are powerful and can accidentally execute dangerous terminal commands if not tested properly.

### Dual-Cable Connection: Ensure both the UART and USB-OTG cables are plugged into the host machine simultaneously.

### Flash and Monitor: Run idf.py -p <UART_PORT> flash monitor to boot the device and view the live baseline capacitance logs.

### The Sandbox: Open a completely blank text document (e.g., Notepad, TextEdit, or a new VS Code tab).

### Arm the Device: Click your mouse inside the blank document so the blinking cursor is active.

### Execute: Pinch the bare metal of the jumper wire. The string will instantly type out into the document, and the serial monitor will log "Touch Detected! Injecting String...".

## Real-World Applications
While this project types a simple "Hello" message, the underlying architecture serves as the foundation for highly practical applications:

### Assistive Technology: Creating ultra-low-cost, custom adaptive switches for individuals with limited mobility.

### Stream Deck / Productivity Macropads: Expanding the code to monitor multiple touch pins to trigger IDE shortcuts or video editing macros.

### Penetration Testing (Authorized): Demonstrating "BadUSB" vulnerabilities to IT departments.

### Interactive Art Installations: Hiding jumper wires inside conductive paint to trigger media changes when attendees interact with the exhibit.