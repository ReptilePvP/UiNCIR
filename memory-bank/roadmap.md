# Project Roadmap

## Current stable milestone
### Milestone A: working sensor dashboard
Completed:
- PlatformIO build compiles and runs
- CoreS3 display works
- Pa.HUB channel switching works
- Raw I2C Joystick2 navigation works
- Adafruit MLX90614 works on channel 5
- LVGL 5-tab UI and joystick navigation work
- Persistent settings storage works (units, refresh, emissivity, alerts, calibration offset, debug)
- Alerts threshold flow works (visual + tone)
- Calibration offset flow works

This is the current baseline and should be preserved.

---

## Next recommended milestones

### Milestone B: polish the live dashboard
Goals:
- Improve visuals on the Live tab
- Make the device feel more like a finished terp meter

Tasks:
- Add large centered object temp card
- Add temperature history graph
- Add stronger zone colors and status chip
- Add explicit sensor status text/icon
- Smooth UI updates

Definition of done:
- Live tab looks polished and remains responsive
- No regression in sensor reliability

---

### Milestone C: improve settings UX
Goals:
- Make settings easier to navigate with joystick only

Tasks:
- Improve readability of selected rows and edit state
- Reduce friction in edit/apply flows (especially emissivity restart path)
- Add optional quality-of-life settings:
  - temperature smoothing on/off
  - graph time window
  - zone profile presets

Definition of done:
- User can fully control settings without serial monitor or code changes

---

### Milestone D: real-time graphing
Goals:
- Add a proper temperature trend view

Tasks:
- Add `lv_chart`
- Store rolling buffer of recent readings
- Graph object temp over time
- Optional ambient overlay

Definition of done:
- Temperature history is visible and updates in real time

---

### Milestone E: servo reintegration
Status: deferred until dashboard baseline stays stable

Goals:
- Reintroduce the 360 servo in a controlled way

Tasks:
- Use separate test build for servo only
- Decide between angle-based and pulse-based control
- Calibrate neutral / stop point
- Add software deadband
- Reintroduce only after stable isolated test

Definition of done:
- Servo can be enabled without breaking sensor reliability
- Servo stays stopped when commanded off
- Servo control does not interfere with MLX reads

---

### Milestone F: terp-meter specific behavior
Goals:
- Turn the project into a purpose-built dab temp device

Tasks:
- Align and tune current zones (`COLD/GOOD/TOO HOT`) for real use, or migrate to desired named bands
- Add “ready” cue when entering desired range
- Extend existing alert cue behavior (tone + visual) with optional haptic/screen effects
- Hold peak value for a short period
- Refine existing emissivity and calibration flows only if needed

Definition of done:
- Device supports real-use workflow instead of just raw temperature viewing

---

## Anti-regression rules
Before making major changes:
1. Keep a known-good branch or copy of the working dashboard build.
2. Test MLX sensor alone if full app behavior becomes unstable.
3. Reintroduce servo only from a separate branch.
4. Prefer one new subsystem at a time.

---

## Priority order
1. Keep MLX + joystick + LVGL stable
2. Improve dashboard polish
3. Add chart/history
4. Improve settings UX
5. Revisit servo
