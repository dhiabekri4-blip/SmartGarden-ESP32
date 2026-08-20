# 🌱 SmartGarden ESP32

An IoT-based smart irrigation system built with **ESP32**, **Wokwi**, and **Blynk**.

The project monitors environmental and soil conditions for two plant zones — **Tomatoes** and **Mint** — and automatically controls irrigation while also allowing remote manual control through a Blynk dashboard.

## 🚀 Features

* 🌱 Multi-zone soil moisture monitoring
* 🍅 Independent Tomato irrigation control
* 🌿 Independent Mint irrigation control
* 🌡️ Temperature monitoring with DHT22
* 💧 Humidity monitoring
* 🚰 Water tank level monitoring
* 🤖 Automatic irrigation mode
* 📱 Manual pump control from Blynk
* 🔄 AUTO / MANUAL mode switching
* 🛑 Low-water tank protection
* ⚠️ Soil sensor fault detection
* ⚠️ DHT22 fault detection
* ⏱️ Automatic 3-second pump shutoff
* 🔁 Irrigation cooldown protection
* 📊 Live IoT telemetry through Blynk
* 🖥️ Full simulation in Wokwi

## 🧠 System Architecture

```text
                       BLYNK CLOUD
                            │
                            │ Wi-Fi
                            ▼
                         ESP32
                           │
          ┌────────────────┼────────────────┐
          │                │                │
        DHT22         Water Tank       Soil Sensors
          │                │             │      │
    Temperature        Tank Level     Tomato   Mint
     Humidity                           │        │
                                       ▼        ▼
                                   Pump 1     Pump 2
```

## 🌱 Plant Zones

### Tomatoes

* Soil sensor: GPIO 34
* Pump control: GPIO 26
* Watering threshold: below 45%
* Healthy moisture level: 50%

### Mint

* Soil sensor: GPIO 32
* Pump control: GPIO 27
* Watering threshold: below 30%
* Healthy moisture level: 35%

## 🔌 Pin Mapping

| Component          | ESP32 Pin |
| ------------------ | --------- |
| Tomato Soil Sensor | GPIO 34   |
| Mint Soil Sensor   | GPIO 32   |
| DHT22              | GPIO 15   |
| Water Tank Sensor  | GPIO 35   |
| Tomato Pump        | GPIO 26   |
| Mint Pump          | GPIO 27   |

## 📱 Blynk Datastreams

| Virtual Pin | Function             |
| ----------- | -------------------- |
| V0          | Tomato soil moisture |
| V1          | Mint soil moisture   |
| V2          | Temperature          |
| V3          | Humidity             |
| V4          | Water tank level     |
| V5          | Tomato pump status   |
| V6          | Mint pump status     |
| V7          | System status        |
| V8          | AUTO / MANUAL mode   |
| V9          | Manual Tomato pump   |
| V10         | Manual Mint pump     |

## 🤖 Automatic Mode

When **AUTO mode** is enabled, the ESP32 decides when irrigation is required.

A pump starts only if:

* Soil moisture is below the configured threshold
* The soil sensor is working correctly
* The water tank is not low
* The pump is currently OFF
* The irrigation cooldown period has finished

The pump automatically stops after **3 seconds**.

## 🎮 Manual Mode

When **MANUAL mode** is enabled, the user can control each irrigation zone remotely through Blynk.

* V9 controls the Tomato pump
* V10 controls the Mint pump

Manual control still respects important safety protections such as low tank level and sensor faults.

## 🛡️ Safety & Fault Handling

The system includes several embedded safety mechanisms.

### Low Tank Protection

If the water tank falls below 10%, irrigation is blocked and any running pump is stopped.

### Soil Sensor Failure

Invalid soil sensor values are counted. After repeated invalid readings, the sensor is marked as faulty and irrigation for that zone is blocked.

### DHT22 Failure

Repeated invalid temperature or humidity readings trigger a DHT22 sensor fault.

### Pump Timeout

Each watering cycle is limited to 3 seconds to prevent a pump from remaining active indefinitely.

### Cooldown

After watering, the system waits before allowing another automatic irrigation cycle.

## 📊 System States

The controller can report the following states:

```text
NORMAL
WATERING
LOW_TANK
HIGH_TEMPERATURE
CRITICAL
SENSOR_FAULT
```

## 🧰 Technologies Used

* ESP32
* Arduino / C++
* Blynk IoT
* Wokwi
* DHT22
* Analog soil moisture sensors
* Water level sensing
* Git & GitHub

## ▶️ Running the Project

1. Open the project in Wokwi.
2. Create a Blynk template and device.
3. Configure virtual pins V0–V10.
4. Replace the placeholder Blynk credentials in `sketch.ino` with your own credentials.
5. Start the simulation.
6. Open the Blynk device dashboard to monitor and control the SmartGarden.

> Never commit your real Blynk authentication token to a public repository.

## 📸 Project Preview

### Wokwi Simulation

*Add Wokwi screenshot here.*

### Blynk Dashboard

*Add Blynk dashboard screenshot here.*

## 🔮 Future Improvements

* Physical hardware prototype
* PCB design
* Mobile notifications
* Historical sensor-data analysis
* Additional plant zones
* Weather-aware irrigation
* Improved water consumption monitoring

## 👨‍💻 Author

**Dhia Bekri**

Information Engineering student interested in embedded systems, IoT, software development, and intelligent connected devices.
