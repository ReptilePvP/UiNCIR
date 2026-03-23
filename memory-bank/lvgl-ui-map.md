# LVGL UI Map

## UI architecture
The current working UI is a **tab-based dashboard** controlled by the Joystick2.

### Navigation model
- **Left / Right:** switch tabs
- **Up / Down:** select rows or adjust edit values on the active settings-style tab
- **Joystick press:** apply/toggle selected row action

## Root screen
- One active LVGL screen
- Main widget: `lv_tabview`

## Tabs

### 1. Live tab
Purpose: show the current sensor reading as fast as practical.

#### Elements
- **Object temperature label**
  - Large primary reading
  - Displays object temp in current units
- **Ambient temperature label**
  - Secondary reading
- **Temperature bar**
  - Visual magnitude indicator
- **Zone label**
  - COLD / GOOD / TOO HOT
- **Battery label**
  - battery percent and charging indicator
- **Alert visual state**
  - live tab background accents when threshold alert is active

#### Data sources
- Object temperature from MLX90614
- Ambient temperature from MLX90614
- Zone derived from object temperature in Fahrenheit

### 2. Stats tab
Purpose: show accumulated reading stats for the current session.

#### Elements
- **Min**
- **Max**
- **Last**
- **Reads**

#### Data sources
- Session min object temp
- Session max object temp
- Last object temp
- Total successful sensor reads

### 3. Settings tab
Purpose: allow joystick-controlled runtime settings.

#### Elements
- **Units**
  - Fahrenheit / Celsius
- **Refresh**
  - sensor polling interval
- **Emissivity**
  - read/adjust/apply flow with slider
- **Debug**
  - toggle serial debug logging
- **Hint text**
  - reminds user of joystick controls
- **Notice text**
  - reports apply/save status and next actions

#### Current settings
- Units toggle by press on selected row
- Refresh interval cycles by press on selected row
- Emissivity enters edit mode, then saves on press (restart required)
- Debug toggles by press on selected row

#### Refresh options
- `35 ms`
- `60 ms`
- `100 ms`
- `150 ms`

### 4. Alerts tab
Purpose: configure threshold alerts.

#### Elements
- **Alerts enabled**
  - ON / OFF
- **Threshold**
  - adjustable Fahrenheit threshold with display converted to active unit
- **Threshold slider**
- **Notice + hint text**

### 5. Calibration tab
Purpose: apply runtime offset correction.

#### Elements
- **Offset**
  - signed correction value
- **Offset slider**
- **Notice + hint text**

## Display update strategy
To keep the UI responsive:
- LVGL tick runs continuously
- Temperature reads run on a timed schedule
- UI redraw/update runs on a separate timed schedule
- Joystick reads run on a separate timed schedule

## Styling direction
Current style:
- Dark dashboard
- Simple labels
- Functional tab layout

Suggested next style upgrade:
1. Larger center temp card on Live tab
2. Horizontal color band for zone state
3. Chart widget for recent history
4. Status icons for sensor OK / error
5. Better tab theming for a more polished “device” look

## Future LVGL additions
### Recommended
- `lv_chart` for live graph
- large numeric widget/card
- color-coded status chip
- settings rows with highlighted selection state
- optional touch interaction as a secondary navigation path

## UI state dependencies
The UI depends on:
- `use_fahrenheit`
- current object and ambient readings
- min/max stats
- read count
- refresh interval index
- current active tab
- `current_emissivity`
- alert enable state and threshold
- calibration offset
- debug mode flag
