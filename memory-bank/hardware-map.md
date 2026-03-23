# Hardware Map

## Core controller
- **Board:** M5Stack CoreS3
- **Framework:** Arduino via PlatformIO
- **UI:** LVGL 9.5.0
- **Primary external bus:** Port A I2C

## Port A wiring
- **SDA:** GPIO 2
- **SCL:** GPIO 1
- **Bus speed:** 100 kHz

## Pa.HUB v2.1
- **Connected to:** CoreS3 Port A
- **I2C address:** `0x70`

### Channel assignments
| Pa.HUB Channel | Device | Status |
|---|---|---|
| 0 | M5 8Servo Unit | reserved / previously tested |
| 1 | Joystick2 | active |
| 5 | MLX90614 / NCIR sensor path | active |

## Joystick2
- **Connection:** Pa.HUB channel 1
- **I2C address:** `0x63`
- **Access style:** raw I2C, no dedicated library

### Registers
| Register | Meaning |
|---|---|
| `0x10` | X axis |
| `0x11` | Y axis |
| `0x20` | Button |

### Button logic
- `0` = pressed
- `1` = released

### Raw value behavior
- Typical raw range: `0..255`
- Center point: about `128`
- Signed conversion: `signed = raw - 128`

## MLX90614 temperature sensor
- **Connection:** Pa.HUB channel 5
- **I2C address:** `0x5A`
- **Library:** `Adafruit_MLX90614`
- **Confirmed working:** yes, including minimal sensor-only PlatformIO test

### Functions in use
- `begin(addr, &Wire)`
- `readObjectTempF()`
- `readAmbientTempF()`

## 8Servo Unit
- **Connection:** Pa.HUB channel 0
- **Library/header:** `#include <M5_UNIT_8SERVO.h>`
- **Current project status:** removed from the working LVGL dashboard build for now
- **Reason removed:** servo behavior was buggy / always running / needed separate calibration work

## Known-good current wiring
- CoreS3 Port A -> Pa.HUB
- Pa.HUB channel 1 -> Joystick2
- Pa.HUB channel 5 -> MLX90614 / NCIR
- Servo disconnected from active app logic for current stable build

## Bring-up checklist
1. Confirm CoreS3 boots and display initializes.
2. Confirm Pa.HUB responds on `0x70`.
3. Confirm channel 1 shows joystick on `0x63`.
4. Confirm channel 5 shows MLX on `0x5A`.
5. Confirm minimal MLX test reads valid object temperature.
6. Confirm full LVGL app updates temperature and joystick tab navigation.

## Risk notes
- Pa.HUB channel switching must occur before every device transaction.
- MLX reads are stable in the minimal test and should be treated as the reference pattern.
- Servo should stay out of the main build until reintroduced with separate calibration and timing isolation.
