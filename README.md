# BLE-Beacon-Screen-Locker
ESP32-based BLE Proximity Tracker acting as a Bluetooth HID. Automatically locks the workstation via 'Windows + L' when the beacon's RSSI drops below a defined threshold.

## Functional Description & Configuration

This project utilizes an ESP32 to monitor the proximity of a specific BLE Beacon based on its signal strength (RSSI). Acting as a Bluetooth HID (Human Interface Device), the ESP32 simulates a keyboard to automatically lock your Windows PC (`Win + L`) when you move away.

### 1. Logic & Visual Feedback
The system uses a state machine visualized by an RGB LED:

* **🟢 Green (Near):** The beacon is within the allowed range. The PC remains unlocked.
* **🔴 Red (Far/Locked):** The beacon has crossed the distance threshold. The `Win + L` key combination is sent **once** at the moment the state changes from Green to Red.
* **⚫ Off (Missing):** No signal is detected from the specific beacon.

### 2. Signal Smoothing (Error Correction)
To prevent accidental locking due to signal fluctuation, the RSSI value is not evaluated instantly. The code requires a configurable number of consecutive readings (samples) above or below the threshold before changing the state. This "counter" mechanism ensures stability.

### 3. Hardware Calibration
You can adjust the sensitivity (distance) at runtime without re-flashing the code using two physical buttons connected to the ESP32:

* **Button A:** Increases the RSSI threshold (shorter distance).
* **Button B:** Decreases the RSSI threshold (longer distance).

> **⚠️ Important Calibration Note:**
> Since the ESP32 acts as a keyboard, it will **type out the new RSSI threshold value** whenever a button is pressed. Please ensure a text editor (like Notepad) is open and active while calibrating to see the current values.

### 4. Code Configuration
Before flashing, you must update the `main.cpp` with your specific hardware details:

* **Target MAC Address:** The unique address of the BLE Beacon you want to track.
* **Pin Definitions:** Define the GPIO pins for the two buttons and the RGB LED connections.


![Groß (IMG_0443)](https://github.com/user-attachments/assets/8953d35f-7f18-43f7-a4a9-3c36fea3120a)
