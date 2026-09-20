# STM32 Bluetooth-Controlled Modular Car

A modular differential-drive robotic car controlled wirelessly through a Bluetooth serial connection. The project uses an STM32F103RB to receive commands through USART1 and control four DC motors arranged as left-side and right-side pairs using GPIO direction signals and dual-channel PWM.

The firmware separates Bluetooth command handling from motor control, making the project easier to understand, test, and extend.

## Features

- STM32F103RB-based motor controller
- Wireless command input through a Bluetooth UART module
- Interrupt-driven, one-byte-at-a-time UART reception
- Four-motor differential drive with two motors connected as each side group
- Independent left- and right-side motor-pair direction control
- TIM3 PWM speed control on two channels
- Forward, backward, left, right, and stop motor-control functions
- Small command buffer with newline-based command processing
- Modular source files for Bluetooth and motor functionality
- Non-blocking main loop

## System architecture

```text
Phone / Bluetooth controller
            |
        Bluetooth UART
        9600 baud, 8-N-1
            |
          USART1
            |
      STM32F103RB
       /         \
 TIM3 CH3       TIM3 CH4
 Direction     Direction
   GPIO           GPIO
     |              |
     +---- Motor driver ----+
              |             |
      Two right motors  Two left motors
```

## Hardware

- STM32F103RB microcontroller or compatible development board
- Bluetooth UART module such as the HC-05
- Four DC geared motors: two on the left side and two on the right side
- Dual H-bridge motor driver suitable for driving two motors per channel
- Robot chassis and wheels
- Motor power supply
- Jumper wires
- Common ground between the STM32, Bluetooth module, motor driver, and power supply

> Confirm the Bluetooth module's logic-voltage requirements before connecting it to the STM32. Do not power the motors directly from the microcontroller.

## STM32 pin configuration

| Function | Peripheral | STM32 pin |
|---|---|---|
| Bluetooth TX to STM32 RX | USART1 RX | PA10 |
| STM32 TX to Bluetooth RX | USART1 TX | PA9 |
| Right-side motor-pair PWM | TIM3 channel 3 | PB0 |
| Left-side motor-pair PWM | TIM3 channel 4 | PB1 |
| Left-side motor-pair direction input 1 | GPIO output | PA7 |
| Left-side motor-pair direction input 2 | GPIO output | PA6 |
| Right-side motor-pair direction input 1 | GPIO output | PB4 |
| Right-side motor-pair direction input 2 | GPIO output | PB5 |

The project configures USART1 at **9600 baud**, with 8 data bits, no parity, and one stop bit.

## PWM configuration

TIM3 generates PWM on channels 3 and 4.

| Parameter | Value |
|---|---:|
| Timer input clock | 64 MHz |
| Prescaler | 19 |
| Auto-reload period | 209 |
| Approximate PWM frequency | 15.24 kHz |
| Valid compare range | 0-209 |

Both motor pairs currently use the same speed value. Each PWM channel controls the two motors on one side of the vehicle. A compare value of `0` stops PWM output, while `209` represents the maximum configured duty cycle.

## Firmware structure

```text
Modular_car/
├── Core/
│   ├── Inc/
│   │   ├── bt_uart.h
│   │   ├── motor.h
│   │   └── main.h
│   └── Src/
│       ├── bt_uart.c
│       ├── motor.c
│       └── main.c
├── Drivers/
├── Motor_Control2.ioc
└── README.md
```

### `main.c`

Initializes GPIO, TIM3, and USART1. It then starts the motor PWM channels and interrupt-driven Bluetooth reception. The main loop remains empty because received commands are handled through the UART interrupt callback.

### `motor.c`

Contains private direction-control helpers and the public vehicle-control API:

```c
void Motor_Init(void);
void Car_Forward(uint16_t speed);
void Car_Backward(uint16_t speed);
void Car_Left(uint16_t speed);
void Car_Right(uint16_t speed);
void Car_Stop(void);
```

### `bt_uart.c`

Receives one character at a time using `HAL_UART_Receive_IT()`. Characters are stored in a 10-byte buffer. When a carriage return or newline is received, the command is terminated and passed to `BT_ProcessCommand()`.

The receive interrupt is armed again at the end of every callback.

## Current Bluetooth commands

The table below describes the command mapping exactly as it is currently implemented.

| Received command | Function called | Vehicle action |
|---|---|---|
| `f` | `Car_Right(209)` | Rotate/turn right |
| `b` | `Car_Left(209)` | Rotate/turn left |
| `r` | `Car_Forward(209)` | Move forward |
| `l` | `Car_Backward(209)` | Move backward |
| `s` | `Car_Stop()` | Stop |

Commands must currently be followed by `\r` or `\n`. For example:

```text
r<newline>
s<newline>
```

> The command letters are presently mapped differently from their conventional meanings. The firmware can be updated so that `f`, `b`, `l`, and `r` directly represent forward, backward, left, and right.

## Motor direction logic

| Movement | Right-side motor pair | Left-side motor pair |
|---|---|---|
| Forward | Forward | Forward |
| Backward | Backward | Backward |
| Right | Backward | Forward |
| Left | Forward | Backward |
| Stop | PWM compare set to 0 | PWM compare set to 0 |

The current left/right functions perform an in-place turn by driving the two motors on one side opposite to the two motors on the other side.

## How the command flow works

1. `BT_UART_Init()` enables one-byte interrupt reception.
2. USART1 receives a character from the Bluetooth module.
3. `HAL_UART_RxCpltCallback()` stores the character in the receive buffer.
4. A carriage return or newline terminates the command.
5. `BT_ProcessCommand()` compares the command with the supported values.
6. The appropriate motor-control function updates GPIO direction pins and PWM duty.
7. UART reception is armed again for the next character.

## Building and flashing

1. Open `Motor_Control2.ioc` using STM32CubeMX or STM32CubeIDE.
2. Confirm that the STM32CubeF1 firmware package is installed.
3. Open or generate the STM32CubeIDE project.
4. Build the project.
5. Connect the board through ST-LINK.
6. Flash the firmware.
7. Pair the phone or computer with the Bluetooth module.
8. Open a Bluetooth terminal configured for 9600 baud.
9. Enable carriage-return or newline transmission.
10. Send one of the supported commands.

## Current limitations

- Commands require a carriage return or newline before they are executed.
- The command letters do not yet match the conventional direction initials.
- Motor speed is fixed at the maximum compare value for every movement command.
- Both side motor pairs always receive the same PWM compare value.
- Speed values are not clamped inside `set_speed()`.
- There is no communication-loss timeout; the car continues its last command if Bluetooth disconnects.
- Unknown commands do not automatically stop the motors.
- `Car_Stop()` sets PWM to zero but does not reset the direction pins.
- Return values from the HAL PWM-start and UART-receive functions are not checked.

## Planned improvements

- Correct the command mapping to `f`, `b`, `l`, `r`, and `s`
- Process recognized single-character commands without requiring a newline
- Add configurable speed commands
- Clamp PWM values to the valid 0-209 range
- Add a Bluetooth communication timeout that automatically stops the car
- Stop the car when an invalid command is received
- Add independent left/right motor-pair speed control
- Add acceleration and deceleration ramps
- Add obstacle detection using ultrasonic or time-of-flight sensors
- Add battery-voltage monitoring
- Add wiring diagrams, hardware photographs, and a demonstration video

## Safety notes

- Use a motor driver whose per-channel rating safely supports the combined normal and stall current of the two motors connected to that channel.
- Use a suitable motor power supply and fuse.
- Keep high-current motor wiring away from the STM32 signal wiring.
- Ensure that all connected modules share a common ground where required.
- Lift the wheels off the ground during initial firmware testing.
- Add a communication failsafe before operating the car at high speed.

## Learning outcomes

This project demonstrates:

- STM32 HAL-based embedded development
- GPIO-based H-bridge direction control
- Timer PWM generation
- Interrupt-driven UART communication
- Buffered command parsing
- Separation of hardware-control modules
- Four-motor differential-drive motion control

## Author

**Mohammed Sameer**

B.Tech Electronics and Communication Engineering  
Embedded systems and STM32 development
