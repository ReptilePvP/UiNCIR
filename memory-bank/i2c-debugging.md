# I2C Debugging Guide

## Bus topology
CoreS3 Port A -> Pa.HUB (`0x70`) -> selected channel -> device

## Known addresses
- Pa.HUB: `0x70`
- Joystick2: `0x63`
- MLX90614: `0x5A`
- 8Servo Unit: `0x25`

## Core rule
Always select the Pa.HUB channel **before every transaction** to a device behind it.

## Reference selector
```cpp
static bool pahub_select(uint8_t ch) {
  Wire.beginTransmission(0x70);
  Wire.write(1 << ch);
  bool ok = (Wire.endTransmission() == 0);
  delay(2);
  return ok;
}
```

## Recommended debug sequence

### 1. Confirm Pa.HUB exists
Expected:
- `0x70` visible on Port A

### 2. Scan joystick channel
Select channel 1, then scan.
Expected:
- `0x63`

### 3. Scan MLX channel
Select channel 5, then scan.
Expected:
- `0x5A`

### 4. Minimal MLX test
If full app fails, run a minimal sensor-only sketch first.

### 5. Compare minimal test vs full app
If minimal test works and full app fails:
- issue is scheduling, timing, or bus contention
- not basic wiring

## Known-good troubleshooting pattern
- Initialize MLX before starting other heavy activity
- Add tiny delay after channel selection
- Avoid unnecessary duplicate joystick reads
- Keep sensor reads on a controlled schedule

## Channel scan helper
```cpp
static void scan_channel(uint8_t ch) {
  Serial.printf("Scanning channel %u...\n", ch);
  if (!pahub_select(ch)) {
    Serial.println("Channel select failed");
    return;
  }
  delay(10);

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("Found: 0x%02X\n", addr);
    }
  }
}
```

## Minimal MLX test status
Confirmed working:
- Adafruit_MLX90614
- Pa.HUB channel 5
- correct sensor wiring

That minimal test should be treated as the fallback reference whenever the larger app regresses.

## Joystick read notes
Joystick is read via raw registers:
- `0x10` = X
- `0x11` = Y
- `0x20` = button

Button logic:
- `0` = pressed

## Regression checklist
If sensor stops reading in the full app:
1. Run channel scan again.
2. Run minimal MLX test.
3. Confirm channel 5 still shows `0x5A`.
4. Check whether a new feature increased bus traffic.
5. Remove newest subsystem and re-test.

## Future servo caution
When servo comes back:
- isolate it first
- confirm it does not affect channel 5 reads
- prefer a dedicated calibration sketch before reintegration
