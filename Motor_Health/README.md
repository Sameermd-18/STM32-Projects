# CAN-Based DC Motor Health Monitoring System

> **Project status: Work in progress**

A real-time embedded system for monitoring the operating condition of a 12 V geared DC motor. An STM32F103RB measures motor current, supply voltage, rotational speed, direction and temperature, then transmits the measurements over a CAN bus.

The final system is intended to detect motor conditions such as overload, mechanical drag, stall/jam, thermal stress and abnormal supply voltage. An ESP32 monitor node will receive and display the measurements and diagnostic state.

## Why this project?

Automotive auxiliary systems use DC motors for power windows, wipers, seat adjustment, pumps and HVAC mechanisms. Changes in motor current, speed and temperature can indicate excessive mechanical load or an approaching failure.

This project demonstrates a small distributed embedded architecture similar to communication between automotive electronic control units:

- Real-time sensing on an STM32
- Concurrent processing with FreeRTOS
- Motor control using PWM
- CAN-based communication between two nodes
- Multi-sensor fault classification
- Remote monitoring using an ESP32

## Current progress

| Feature | Status |
|---|---|
| STM32CubeMX peripheral configuration | Implemented |
| FreeRTOS task structure | Implemented |
| INA219 voltage, current and power driver | Implemented |
| NTC thermistor temperature calculation | Implemented |
| Quadrature encoder RPM and direction measurement | Implemented |
| PWM motor control | Implemented |
| CAN telemetry frame transmission | Implemented |
| Fault classification logic | In progress |
| ESP32 CAN monitor node | Planned |
| Baseline characterization and validation | Planned |
| Final documentation and demonstration video | Planned |

## System architecture

```text
12 V DC motor
     |
     +-- INA219 current/voltage sensor -- I2C --+
     +-- Quadrature encoder ------------ TIM3 --+--> STM32F103RB
     +-- NTC thermistor ----------------- ADC ---+       |
     +-- Motor driver <------------------ PWM ---+       |
                                                         |
                                                    CAN transceiver
                                                         |
                                                       CAN bus
                                                         |
                                                    CAN transceiver
                                                         |
                                                   ESP32 monitor
                                                         |
                                              Serial monitor / display
```

## Hardware

### Sensor and control node

- STM32F103RB development board
- 12 V geared DC motor with quadrature encoder
- INA219 current and voltage sensor
- 10 kΩ NTC thermistor
- Motor driver suitable for the motor's stall current
- CAN transceiver
- 12 V current-limited power supply

### Monitor node

- ESP32 development board
- 3.3 V-compatible CAN transceiver
- Serial monitor or optional OLED/LCD

### CAN bus

- Two CAN transceivers
- Twisted-pair CANH and CANL wiring
- One 120 Ω termination resistor at each physical end of the bus
- Common ground between the prototype nodes

## STM32 peripheral configuration

| Function | STM32 peripheral | Pins |
|---|---|---|
| INA219 | I2C1 | PB8 SCL, PB9 SDA |
| NTC thermistor | ADC1 channel 0 | PA0 |
| Encoder | TIM3 encoder mode | PA6 CH1, PA7 CH2 |
| Motor PWM | TIM2 channel 1 | PA15 |
| Motor direction | GPIO | PB0 IN1, PC1 IN2 |
| CAN | CAN1 | PA11 RX, PA12 TX |

The STM32 runs at 72 MHz. CAN is currently configured for **500 kbit/s**.

## FreeRTOS design

The application is divided into four tasks:

| Task | Responsibility | Current period/behavior |
|---|---|---|
| SensorTask | Samples RPM, direction, voltage, current, power and temperature | 100 ms |
| FaultTask | Will compare measurements with healthy baselines and classify faults | In progress |
| CANTask | Packs the latest measurements and transmits CAN telemetry | Event-driven with 100 ms delay |
| MotorTask | Starts PWM and controls motor direction and duty cycle | Currently fixed at 75% duty |

A single-element queue currently transfers the latest `SensorData_t` sample from `SensorTask` to `CANTask`.

## CAN telemetry protocol

The STM32 transmits an 8-byte standard CAN data frame.

- **Standard identifier:** `0x100`
- **DLC:** 8 bytes
- **Byte order:** Most-significant byte first

| Bytes | Signal | Encoding |
|---|---|---|
| 0-1 | RPM | Signed 16-bit, RPM × 10 |
| 2-3 | Current | Signed 16-bit, amperes × 1000 |
| 4-5 | Supply voltage | Signed 16-bit, volts × 100 |
| 6-7 | Temperature | Signed 16-bit, °C × 10 |

Example decoding:

```c
int16_t rpm_raw     = (int16_t)((data[0] << 8) | data[1]);
int16_t current_raw = (int16_t)((data[2] << 8) | data[3]);
int16_t voltage_raw = (int16_t)((data[4] << 8) | data[5]);
int16_t temp_raw    = (int16_t)((data[6] << 8) | data[7]);

float rpm         = rpm_raw / 10.0f;
float current_a   = current_raw / 1000.0f;
float voltage_v   = voltage_raw / 100.0f;
float temperature = temp_raw / 10.0f;
```

A separate diagnostic CAN frame containing fault state, direction and node status is planned.

## Planned fault detection

Fault decisions will use combinations of current, RPM, temperature, voltage and commanded PWM duty rather than a single threshold.

| Observed behavior | Intended classification |
|---|---|
| Current rises sharply while RPM approaches zero | Stall or jam |
| Current remains above the healthy baseline while RPM decreases | Overload or mechanical drag |
| Motor temperature exceeds a calibrated safe limit | Thermal stress |
| RPM is lower than expected while supply voltage is abnormal | Undervoltage or abnormal speed |
| CAN telemetry stops for longer than the allowed timeout | Sensor node offline |

Thresholds will be determined experimentally by recording healthy measurements at several PWM duty cycles.

## Project workflow

1. Verify each sensor independently.
2. Measure healthy current, RPM and temperature at multiple PWM duty cycles.
3. Calculate normal operating ranges.
4. Implement time-qualified fault thresholds to avoid false alarms.
5. Transmit measurements and diagnostic states over CAN.
6. Decode and display data on the ESP32.
7. Test healthy, overload, short-duration stall, thermal and undervoltage conditions.
8. Record detection accuracy and latency.

## Building the STM32 firmware

1. Open `Motor_Health.ioc` in STM32CubeMX or STM32CubeIDE.
2. Confirm that STM32Cube FW F1 is installed.
3. Generate or import the project using the STM32CubeIDE toolchain.
4. Build the project.
5. Connect the STM32F103RB through ST-LINK.
6. Flash and debug the firmware.

The firmware currently uses STM32 HAL, CMSIS-RTOS v1 and FreeRTOS.

## Safety

- Confirm that the motor driver and current sensor support the motor's measured stall current.
- Use a current-limited supply and appropriate fuse.
- Keep stall tests brief to avoid damaging the motor, driver or wiring.
- Do not restrain the shaft by hand.
- Secure the motor mechanically before applying power.
- Keep motor power separate from logic power where appropriate, while maintaining the required common reference.
- Never connect a 5 V transceiver output directly to a non-5-V-tolerant ESP32 input.

## Planned improvements

- Complete the fault-detection state machine
- Add filtering and validation for all sensor readings
- Add FreeRTOS stack-overflow and runtime monitoring
- Add CAN transmission error and bus-off recovery
- Add a heartbeat and communication timeout
- Add an ESP32 TWAI receiver
- Define a separate diagnostic CAN frame
- Store baseline and test measurements as CSV files
- Add graphs for healthy and abnormal conditions
- Add circuit diagrams, hardware photographs and a demonstration video
- Add unit tests for CAN packing and fault-classification logic

## Repository notes

Generated build directories such as `Debug/` and `Release/` should not be committed. Recommended ignore rules:

```gitignore
**/Debug/
**/Release/
*.o
*.elf
*.map
*.list
*.d
*.su
*.cyclo
```

## Author

**Mohammed Sameer**

B.Tech Electronics and Communication Engineering  
Embedded systems and STM32 development
