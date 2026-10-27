# CAN-Based DC Motor Health Monitor

A two-node embedded prototype that measures a geared DC motor's speed, current, driver-supply voltage and casing temperature, evaluates warning conditions on an STM32F103RB, and sends readings and status over CAN to an ESP32. The ESP32 displays live data and warnings on a 128 × 64 I²C OLED and in a serial monitor.

**Project status:** The assembled bench prototype and OLED were demonstrated. Load-and-release tests demonstrated overload warnings at 50% and 75% PWM duty and a near-stall warning at 50%. Battery and temperature warning paths were exercised with temporary test values; their physical operating thresholds remain provisional.

> **Repository snapshot:** The STM32 `Core/Src/main.c` currently committed here is an earlier development snapshot. Its `FaultTask` is still a placeholder and its CAN transmitter sends the `0x100` measurement frame only. The later fault-rule, `0x101` status-frame and ESP32 OLED demo firmware described below have **not yet been synchronized to this repository**. Do not treat the current commit as a buildable copy of the complete demonstrated system.

## Why monitor a motor?

Excess resistance can make a DC motor slow down while drawing more current. Speed and current considered together can reveal a developing load that either reading alone might not explain. The system also reports low driver-supply voltage, high measured casing temperature, invalid sensor data and loss of CAN updates.

An automotive power-window mechanism is a **possible application**: abnormal resistance in a guide or regulator could produce a falling-speed/rising-current pattern. This project was tested on a bench motor, **not** in a vehicle. It reports warnings; it does not stop or reverse a window and is not an anti-pinch safety device. Vehicle use would need controlled mechanism testing, a validated response strategy and appropriate safety engineering.

## System overview

```text
Two 18650 cells in series ── INA219 ── L298N ── geared DC motor + encoder
                              │         ▲                  │
                   bus V / current       │ PWM              └─ RPM
                              └─────────┴───── STM32F103RB
                                  NTC ────────┘  │
                                     casing temp  │ CAN 500 kbit/s
                                                  ▼
                                                ESP32
                                                ├─ I²C OLED
                                                └─ USB serial monitor / optional CSV capture
```

The INA219 is wired on the **high side of the motor-driver supply**, so its bus-voltage reading is near the L298N supply input. It is **not** a direct measurement across the motor terminals under PWM. The NTC is taped to the motor casing, not embedded in the winding.

### Hardware

- STM32F103RB development board and ESP32 development board
- CAN transceiver at each node, with shared ground and bus termination appropriate to the physical wiring
- Geared DC motor with encoder and L298N motor driver
- INA219 current/voltage sensor; NTC casing-temperature sensor
- Two 18650 cells in series and a 0.96-inch 128 × 64 I²C OLED

### FreeRTOS on the STM32

The demonstrated STM32 firmware separates acquisition (`SensorTask`), warning evaluation (`FaultTask`), CAN packing/transmission (`CANTask`) and motor PWM/direction control (`MotorTask`). Samples are passed between tasks through FreeRTOS queues. This keeps acquisition, decision rules and transmission logically separate; no benchmark claiming that FreeRTOS is faster than a single loop was performed. The committed `main.c` is older and should be updated before these task behaviors can be reproduced from the repository.

## CAN messages used in the demonstration

The STM32 sends standard CAN frames at **500 kbit/s**, with signed 16-bit measurement values in big-endian byte order.

| CAN ID | DLC | Bytes | Meaning |
|---|---:|---|---|
| `0x100` | 8 | 0–1 | RPM × 10 |
| `0x100` | 8 | 2–3 | Current in A × 1000 |
| `0x100` | 8 | 4–5 | INA219 bus voltage in V × 100 |
| `0x100` | 8 | 6–7 | NTC temperature in °C × 10 |
| `0x101` | 3 | 0 | Fault bit mask |
| `0x101` | 3 | 1 | Commanded PWM duty percent |
| `0x101` | 3 | 2 | Whether an overload rule is available at that duty |

Fault-mask bits: bit 0 overload, bit 1 near stall, bit 2 low supply voltage, bit 3 high casing temperature, bit 4 sensor error. The ESP32 prints status changes and reports stale measurement or status frames after approximately 1.5 seconds without updates.

## Warning logic in the demonstrated setup

| Warning | Trip condition | Clear condition | Evidence |
|---|---|---|---|
| Overload at 50% duty | RPM < 85 **and** current > 0.34 A for 1 s | RPM > 90 **and** current < 0.32 A for 1 s | Real load and release observed |
| Overload at 75% duty | RPM < 112 **and** current > 0.33 A for 1 s | RPM > 116 **and** current < 0.32 A for 1 s | Real load and release observed |
| Near stall at 50% duty | RPM < 20 **and** filtered current > 0.45 A for 0.5 s | RPM > 40 | Severe load and release observed; shaft was still turning |
| Low supply voltage | INA219 bus voltage < 6.4 V for 2 s | Voltage > 6.6 V | Warning path tested with temporary values; physical threshold provisional |
| High casing temperature | NTC > 60 °C for 2 s | NTC < 55 °C | Warning path tested with temporary values; physical threshold provisional |
| Sensor error | Invalid or missing sample | Valid-data recovery in later demo firmware | Earlier INA219 bad-read behavior investigated; confirm exact final flashed firmware |

Current is smoothed for the near-stall rule. Motor load rules wait about two seconds after a duty change. The paired RPM/current condition matters: a free-running current sample above 0.34 A **alone** is not an overload warning. Overload and near-stall thresholds were characterized for this motor and battery setup; no 100% duty overload threshold is claimed.

### Representative 50% duty observations

Selected readings from one reported load-and-release run, rather than repeated-trial statistics:

| Time | Motor condition | RPM | Current | INA219 bus voltage |
|---:|---|---:|---:|---:|
| 13 s | Free | 99.3 | 0.275 A | 7.58 V |
| 17 s | Loaded | 42.4 | 0.426 A | 7.09 V |
| 20 s | Loaded | 41.6 | 0.496 A | 7.20 V |
| 39 s | Severe restriction | 14.4 | 0.525 A | 7.08 V |
| 41 s | Severe restriction | 12.7 | 0.710 A | 7.18 V |
| 46 s | Released | 99.9 | 0.195 A | 7.54 V |

In one running comparison, the INA219 indicated about 7.50 V and a multimeter about 7.53 V at the driver supply. This is a single-point check, not a full calibration.

## INA219 troubleshooting and limitations

Earlier runs sometimes produced implausible current and voltage values. The original INA219 driver did not check whether HAL I²C reads succeeded, so a failed transfer could leave data bytes untrusted. The driver was revised to check transfers and return an invalid value on failure instead of treating the bytes as a normal measurement. The exact physical source of every disturbance was not proven.

The load was applied by hand and is **not** a repeatable torque measurement. Thresholds were tuned near a roughly 7.5 V supply and may need adjustment as the battery discharges. A casing NTC reading is not a winding temperature; 60 °C is a provisional warning threshold, not a verified safe limit from the motor manufacturer. A mechanically locked shaft, actual battery depletion and a motor heated to the warning threshold were not established as fully validated physical test cases.

## Repository and build notes

Open the STM32 `.ioc` project in STM32CubeIDE and build/flash with ST-LINK. The ESP32 demonstration was developed separately in ESP-IDF v6.1; **the ESP32 project and the latest STM32 fault logic still need to be committed here** for a reader to reproduce the complete system. Do not expect the current repository snapshot alone to produce the OLED warning demo.

For a live demonstration: show normal free-running readings at 50% duty, apply a brief practiced load to show RPM decrease/current increase and the overload warning, then release and observe recovery. Severe restriction should be brief and attempted only if the motor is secured and the test setup is safe.

## Next steps

1. Commit the latest STM32 fault-detection firmware and ESP32 receiver/OLED firmware, then verify a clean build from this repository.
2. Add dated test CSVs, hardware photos, wiring diagram and a short demo video.
3. Repeat load tests with a reproducible mechanism and evaluate false positives and warning latency at different battery voltages.
4. Validate low-voltage and casing-temperature limits on the actual hardware before treating them as protective limits.
5. Consider ML only after collecting sufficient labeled, repeatable data across operating conditions.

## Contributors

Mohammed Sameer and project teammates. Add each teammate's name and **verified actual contributions** before submission; the repository history alone does not establish who performed the hardware assembly, testing or documentation.
