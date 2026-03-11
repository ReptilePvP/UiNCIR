#include <Arduino.h>
#include <M5CoreS3.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <M5_UNIT_8SERVO.h>  // Updated include (library header is uppercase)

#define PA_HUB_ADDR 0x70
#define SERVO_ADDR  0x25 

// --- HARDWARE CONFIGURATION ---
// 1. PaHub Config
#define PAHUB_CH_SERVO_UNIT 0  // The 8Servo UNIT is plugged into PaHub Channel 0
#define PAHUB_CH_NCIR       5  // The NCIR UNIT is plugged into PaHub Channel 5

// 2. Servo Config
#define SERVO_PORT_INDEX    0  // The MOTOR is plugged into Port 0 on the Servo Unit

// 3. Button Config (Port C)
#define BTN_PIN_1     18    // Button 1 (Blue Button) - G18
#define BTN_PIN_2     17    // Button 2 (Red Button) - G17

// --- OBJECTS ---
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
M5_UNIT_8SERVO servoUnit; 
M5Canvas canvas(&M5.Display);

// --- VARIABLES ---
unsigned long previousMillis = 0;
const long interval = 100; // Screen refresh rate
int currentServoAngle = 90; 
String statusMsg = "Waiting for input...";
bool servoBrainConnected = false;

// Button edge detection
bool lastBtn1State = HIGH;
bool lastBtn2State = HIGH;

// --- PAHUB SWITCHER ---
void selectPaHubChannel(uint8_t channel) {
  Wire.beginTransmission(PA_HUB_ADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

// --- SERVO SWEEP FUNCTION (Fixed: from current to target) ---
void moveServo(int targetAngle) {
  if (!servoBrainConnected) {
    statusMsg = "ERR: Servo Disconnected";
    return;
  }

  if (targetAngle == currentServoAngle) {
    statusMsg = "Already at " + String(targetAngle) + "°";
    return;
  }

  int startAngle = currentServoAngle;
  int endAngle = targetAngle;
  statusMsg = "Moving: " + String(startAngle) + "° -> " + String(endAngle) + "°";
  
  // 1. Select PaHub Channel 0 (To reach the Unit)
  selectPaHubChannel(PAHUB_CH_SERVO_UNIT);

  int step = (startAngle < endAngle) ? 1 : -1;

  for (int i = startAngle; i != endAngle; i += step) {
    // 2. Set Servo Port 0 angle (To move the Motor)
    if (!servoUnit.setServoAngle(SERVO_PORT_INDEX, (uint8_t)i)) {
      Serial.println("Failed to set servo angle: " + String(i));
    }
    
    currentServoAngle = i;
    delay(15); // Adjust speed here
  }
  
  // Final position
  servoUnit.setServoAngle(SERVO_PORT_INDEX, (uint8_t)endAngle);
  currentServoAngle = endAngle;
  statusMsg = "Move Complete: " + String(endAngle) + "°";
  Serial.println(statusMsg);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  // 1. Init I2C
  Wire.begin(2, 1);

  // 2. Init Buttons (INPUT_PULLUP)
  pinMode(BTN_PIN_1, INPUT_PULLUP);
  pinMode(BTN_PIN_2, INPUT_PULLUP);

  // 3. Setup Display
  M5.Display.setBrightness(128);
  canvas.createSprite(320, 240);
  canvas.setTextSize(2);
  canvas.setTextColor(WHITE);
  canvas.setTextDatum(middle_center);

  Serial.begin(115200);
  Serial.println("System Start");

  // 4. Init NCIR
  selectPaHubChannel(PAHUB_CH_NCIR);
  if(!mlx.begin(0x5A, &Wire)) Serial.println("NCIR Missing");

  // 5. Init 8Servo Unit Logic
  selectPaHubChannel(PAHUB_CH_SERVO_UNIT);
  
  // Check if the "Brain" of the servo unit answers on I2C
  if (servoUnit.begin(&Wire, 2, 1, SERVO_ADDR)) {
    servoBrainConnected = true;
    // CRITICAL FIX: Set pin mode to SERVO_CTL_MODE before using angles
    servoUnit.setOnePinMode(SERVO_PORT_INDEX, SERVO_CTL_MODE);
    // Set initial center
    servoUnit.setServoAngle(SERVO_PORT_INDEX, 90); 
    Serial.println("Servo Unit initialized and centered at 90°");
  } else {
    servoBrainConnected = false;
    Serial.println("Servo Unit init failed - check wiring/PaHub Ch0");
  }
}

void loop() {
  M5.update();
  
  // Read Buttons (LOW = Pressed)
  bool btn1Pressed = (digitalRead(BTN_PIN_1) == LOW);
  bool btn2Pressed = (digitalRead(BTN_PIN_2) == LOW);
  
  // --- BUTTON LOGIC (with edge detection) ---
  if (btn1Pressed && lastBtn1State == HIGH) {
    moveServo(180);  // Sweep to 180° on press
    delay(50);  // Simple debounce
  }
  else if (btn2Pressed && lastBtn2State == HIGH) {
    moveServo(0);   // Sweep to 0° on press
    delay(50);  // Simple debounce
  }
  
  // Update last states
  lastBtn1State = btn1Pressed ? LOW : HIGH;
  lastBtn2State = btn2Pressed ? LOW : HIGH;

  // --- UPDATE DISPLAY ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read Temp
    selectPaHubChannel(PAHUB_CH_NCIR);
    float objTempC = mlx.readObjectTempC();
    
    // Draw UI
    canvas.fillSprite(BLACK);
    
    // 1. Connection Debug
    canvas.setTextSize(2);
    if (servoBrainConnected) {
      canvas.setTextColor(GREEN);
      canvas.drawString("Servo Logic: CONNECTED", 160, 20);
    } else {
      canvas.setTextColor(RED);
      canvas.drawString("Servo Logic: NOT FOUND", 160, 20);
      canvas.setTextSize(1);
      canvas.drawString("Check PaHub Ch0 & Grove Cable", 160, 40);
    }

    // 2. Button Debug (Live State)
    canvas.setTextSize(2);
    canvas.setTextColor(WHITE);
    
    // Draw Box for Button 18
    if (btn1Pressed) canvas.fillCircle(60, 80, 20, GREEN);
    else canvas.drawCircle(60, 80, 20, DARKGREY); // Grey circle when not pressed
    canvas.drawString("Btn 18", 60, 110);

    // Draw Box for Button 17
    if (btn2Pressed) canvas.fillCircle(260, 80, 20, RED);
    else canvas.drawCircle(260, 80, 20, DARKGREY); // Grey circle when not pressed
    canvas.drawString("Btn 17", 260, 110);

    // 3. Status Message
    canvas.setTextColor(YELLOW);
    canvas.drawString(statusMsg, 160, 140);

    // 4. Current Servo Angle (Software State)
    canvas.setTextColor(WHITE);
    canvas.drawString("Servo Ch0 Angle: " + String(currentServoAngle), 160, 170);

    // 5. Temp
    canvas.setTextColor(CYAN);
    canvas.drawString("Temp: " + String(objTempC) + "C", 160, 210);

    canvas.pushSprite(0, 0);
  }
}