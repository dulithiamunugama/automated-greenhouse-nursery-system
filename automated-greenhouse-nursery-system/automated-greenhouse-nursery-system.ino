#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP32Servo.h>

// Screen Dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// Initialize I2C SSD1306 Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Hardware Pin Definitions
#define DHTPIN        4       // DHT22 Data
#define DHTTYPE       DHT22
#define LDRPIN        34      // LDR Analog Input
#define LEDPIN        16      // Grow Light LED
#define SERVOPIN      13      // Vent Servo Motor
#define BUTTON_PIN    25      // Override Button Pin

// Thresholds
#define TEMP_THRESHOLD  28.0  // Vent opens if Temp > 28.0°C
#define LIGHT_THRESHOLD 400  // Grow light turns ON if Light < 400

DHT dht(DHTPIN, DHTTYPE);
Servo ventServo;

// State Variables
bool manualMode = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastSensorReadTime = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Hardware I2C Pins (SDA = 21, SCL = 22)
  Wire.begin(21, 22);

  // Peripherals
  pinMode(LEDPIN, OUTPUT);
  pinMode(LDRPIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button reads LOW when pressed
  
  dht.begin();

  // Attach Servo with pulse width calibration specifically for SG90
  ESP32PWM::allocateTimer(0);
  ventServo.setPeriodHertz(50);          // Standard 50Hz PWM frequency
  ventServo.attach(SERVOPIN, 500, 2400); // 500us - 2400us range for SG90
  ventServo.write(0);                    // Initial position (Closed)

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("OLED initialization failed!"));
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(" Greenhouse Nursery System ");
  display.println(" Mode: AUTO ");
  display.display();
  delay(1500);
}

void loop() {
  // 1. Instant Button Check (Debounced)
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      
      // Button press detected (Transition from HIGH to LOW)
      if (buttonState == LOW) {
        manualMode = !manualMode; // Toggle mode
        Serial.println("====================================");
        Serial.print("MODE CHANGED TO: ");
        Serial.println(manualMode ? "MANUAL OVERRIDE" : "AUTOMATIC");
        Serial.println("====================================");
      }
    }
  }
  lastButtonState = reading;

  // 2. Control Logic & Display Update (Runs smoothly every 500ms)
  if (millis() - lastSensorReadTime > 500) {
    lastSensorReadTime = millis();

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int lightVal = analogRead(LDRPIN);

    if (isnan(temp) || isnan(hum)) {
      temp = 25.0;
      hum = 50.0;
    }

    bool ventOpen = false;
    bool growLightOn = false;

    if (manualMode) {
      // --- MANUAL OVERRIDE MODE ---
      digitalWrite(LEDPIN, HIGH);
      growLightOn = true;

      ventServo.write(90);
      ventOpen = true;

    } else {
      // --- AUTOMATIC MODE ---
      if (temp > TEMP_THRESHOLD) {
        ventServo.write(90);
        ventOpen = true;
      } else {
        ventServo.write(0);
        ventOpen = false;
      }

      if (lightVal < LIGHT_THRESHOLD) {
        digitalWrite(LEDPIN, HIGH);
        growLightOn = true;
      } else {
        digitalWrite(LEDPIN, LOW);
        growLightOn = false;
      }
    }

    // Serial Diagnostics
    Serial.print("Mode: "); Serial.print(manualMode ? "[MANUAL]" : "[AUTO]"); Serial.print(" | ");
    Serial.print("Temp: "); Serial.print(temp, 1); Serial.print("C | ");
    Serial.print("Light: "); Serial.print(lightVal); Serial.print(" | ");
    Serial.print("Vent: "); Serial.print(ventOpen ? "OPEN" : "CLOSED"); Serial.print(" | ");
    Serial.print("Light: "); Serial.println(growLightOn ? "ON" : "OFF");

    // OLED Screen Update
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    
    if (manualMode) {
      display.println("MODE: MANUAL OVERRIDE");
    } else {
      display.println("MODE: AUTOMATIC");
    }
    display.println("---------------------");
    display.print("Temp:  "); display.print(temp, 1); display.println(" C");
    display.print("Hum:   "); display.print(hum, 1); display.println(" %");
    display.print("Light: "); display.println(lightVal);

    // Dynamic Display Logic for Vent Status
    display.print("Vent:  "); 
    if (manualMode) {
      display.println("OPEN (MANUAL)");
    } else {
      display.println(ventOpen ? "OPEN" : "CLOSED");
    }

    // Dynamic Display Logic for Light Status
    display.print("Light: "); 
    if (manualMode) {
      display.println("ON (MANUAL)");
    } else {
      display.println(growLightOn ? "ON" : "OFF");
    }
    
    display.display();
  }
}