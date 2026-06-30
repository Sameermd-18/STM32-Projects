# I2C Communication with STM32 — MPU6050 IMU + 16x2 LCD

A HAL-based STM32 project demonstrating **single-master, multi-slave I2C communication**. The STM32 Nucleo-F103RB reads accelerometer, gyroscope, and temperature data from an MPU6050 IMU and displays live values on a 16×2 LCD via a PCF8574 I2C backpack. Both peripherals share the same I2C bus and are accessed using their unique device addresses.

---

# Objective

- Configure I2C1 on the STM32F103RB.
- Interface multiple I2C slaves on a shared bus.
- Read raw sensor data from the MPU6050.
- Convert raw values into real-world units.
- Display live sensor values on a 16×2 LCD.

---

# Hardware

- STM32 Nucleo-F103RB
- MPU6050 (Accelerometer + Gyroscope)
- 16×2 LCD with PCF8574 I2C Backpack
- Breadboard
- Jumper wires

---

# Software

- STM32CubeIDE
- STM32CubeMX
- STM32 HAL Drivers

---

# I2C Addresses

| Device | 7-bit Address | HAL Address |
|---------|--------------:|------------:|
| MPU6050 | 0x68 | 0xD0 |
| LCD (PCF8574) | 0x27 | 0x4E |

STM32 HAL expects the device address to be shifted left by one bit before passing it to HAL I2C functions.

```c
#define MPU6050_ADDR (0x68 << 1)
#define LCD_ADDR     (0x27 << 1)
```

---

# Wiring

| Signal | Connects To |
|---------|-------------|
| MPU6050 VCC | 3.3V |
| MPU6050 GND | GND |
| MPU6050 SDA | PB7 (I2C1 SDA) |
| MPU6050 SCL | PB6 (I2C1 SCL) |
| MPU6050 AD0 | GND |
| LCD VCC | 5V |
| LCD GND | GND |
| LCD SDA | PB7 (Shared I2C Bus) |
| LCD SCL | PB6 (Shared I2C Bus) |

Both peripherals share the same SDA and SCL lines. The STM32 selects the desired slave using its I2C address.

---

# Project Flow

```
STM32
   │
   │
   ├───────────── SDA ─────────────┐
   │                               │
   ├───────────── SCL ─────────────┤
   │                               │
   ▼                               ▼
MPU6050 (0x68)             LCD + PCF8574 (0x27)
```

The STM32 acts as the **I2C Master**, while both the MPU6050 and LCD act as **I2C Slaves**.

---

# How It Works

## 1. Wake the MPU6050

The MPU6050 starts in Sleep Mode. Writing `0x00` to register `0x6B` enables normal operation.

```c
void wake_up_mpu(void)
{
    uint8_t wake = 0x00;

    HAL_I2C_Mem_Write(
        &hi2c1,
        MPU6050_ADDR,
        PWR_MGMT_1,
        1,
        &wake,
        1,
        100
    );
}
```

---

## 2. Read Raw Sensor Data

The MPU6050 stores Accelerometer, Temperature, and Gyroscope data in consecutive registers.

Instead of reading each register individually, all 14 bytes are read in a single I2C transaction.

```c
HAL_I2C_Mem_Read(
    &hi2c1,
    MPU6050_ADDR,
    ACCL_START,
    1,
    raw_data,
    14,
    100
);
```

---

## 3. Convert Bytes into 16-bit Values

Each sensor reading occupies two registers.

```
ACCEL_XOUT_H
ACCEL_XOUT_L
```

These bytes are combined into one signed 16-bit integer.

```c
ax = (raw_data[0] << 8) | raw_data[1];
ay = (raw_data[2] << 8) | raw_data[3];
az = (raw_data[4] << 8) | raw_data[5];

temp_raw = (raw_data[6] << 8) | raw_data[7];

gx = (raw_data[8]  << 8) | raw_data[9];
gy = (raw_data[10] << 8) | raw_data[11];
gz = (raw_data[12] << 8) | raw_data[13];
```

The high byte is shifted left by 8 bits before combining it with the low byte.

---

## 4. Convert Raw Data into Physical Units

The MPU6050 powers up with:

- Accelerometer: ±2g
- Gyroscope: ±250°/s

According to the datasheet:

```c
accel_x = ax / 16384.0f;
accel_y = ay / 16384.0f;
accel_z = az / 16384.0f;

gyro_x = gx / 131.0f;
gyro_y = gy / 131.0f;
gyro_z = gz / 131.0f;

temperature = (temp_raw / 340.0f) + 36.53f;
```

Resulting units:

- Accelerometer → g
- Gyroscope → °/s
- Temperature → °C

---

## 5. Display Data on the LCD

The LCD communicates through a PCF8574 I/O Expander over I2C.

Since the LCD driver accepts character strings, floating-point values are first converted into text.

```c
char buffer[16];

sprintf(buffer, "%.2f", accel_x);

lcd_put_cur(0,0);
lcd_send_string("AX:");
lcd_send_string(buffer);
```

---

# Learning Outcomes

This project helped me understand:

- STM32 HAL I2C communication
- Single-master, multi-slave I2C architecture
- 7-bit vs 8-bit I2C addressing
- Register-based communication
- Reading consecutive sensor registers
- Converting raw IMU data into engineering units
- Interfacing an I2C LCD
- Formatting floating-point numbers using `sprintf()`
- Debugging embedded systems using STM32CubeIDE
- Using Git and GitHub for version control

---

# Debugging Notes

During development several issues were encountered and resolved.

### Live Expressions not updating

Variables were optimized by the compiler and stored in CPU registers instead of RAM.

**Solution**

Declared variables as `volatile`.

---

### MPU6050 occasionally disappeared

The AD0 pin had an intermittent connection.

Because AD0 determines the I2C address, a floating connection caused the address to randomly switch between:

- 0x68
- 0x69

**Solution**

Securely connect AD0 to GND.

---

### Hardware verification

An ESP32 running a simple I2C scanner was used to verify that the issue was hardware-related rather than an STM32 software issue.

---

# Future Improvements

- Interrupt-driven data acquisition
- Sensor calibration
- Kalman / Complementary Filter
- OLED display support
- FreeRTOS implementation
- DMA-based I2C communication

---

# Repository Structure

```
01_I2C_MPU6050/
│
├── Core/
├── Drivers/
├── README.md
├── I2C_MPU6050.ioc
├── STM32F103RBTX_FLASH.ld
└── ...
```

---

# Author

**Sameer M**

B.Tech Electronics & Communication Engineering

Learning Embedded Systems with STM32.
