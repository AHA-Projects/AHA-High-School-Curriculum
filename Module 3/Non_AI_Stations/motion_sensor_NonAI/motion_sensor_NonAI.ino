// Acknowledgments
// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373
// This material is based upon work supported by the National Science Foundation under Grant No. 2405373.
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors
// and do not necessarily reflect the views of the National Science Foundation.

// --- Default Installed Libraries ---
#include <Wire.h>
#include <SPI.h>

// --- Sensor Specific Libraries ---
#include <Adafruit_MPU6050.h>
#include <Adafruit_ST7789.h>

// --- Pin Definitions ---
#define TFT_CS    33
#define TFT_DC    25
#define TFT_RST   26

// --- Color Definitions ---
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GREEN   0x07E0
#define RED     0xF800
#define BLUE    0x001F
#define YELLOW  0xFFE0

// --- Objects ---
Adafruit_ST7789  tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_MPU6050 mpu;

// --- Motion Thresholds ---
const float STILL_THRESHOLD_LOW  = 8.0;
const float STILL_THRESHOLD_HIGH = 11.0;

// --- Debounce Counts ---
const int DEBOUNCE_COUNT_MOVING = 5;
const int DEBOUNCE_COUNT_STILL  = 10;

// --- Globals ---
int  movingCounter = 0;
int  stillCounter  = 0;
bool wasMoving     = false; // Global so setup() can seed it correctly

// =====================
// Helper: draw moving screen
// =====================
void drawMoving() {
  tft.fillScreen(BLACK);
  tft.setTextSize(4);
  tft.setTextColor(RED);
  tft.setCursor(5, 20);
  tft.println("Weeee!");
  tft.setCursor(5, 60);
  tft.print("On the Go!");
}

// =====================
// Helper: draw still screen
// =====================
void drawStill() {
  tft.fillScreen(BLACK);
  tft.setTextSize(4);
  tft.setTextColor(GREEN);
  tft.setCursor(5, 20);
  tft.println("Shake me");
  tft.setCursor(5, 60);
  tft.print("to see magic!");
}

// =====================
// Helper: read acceleration magnitude
// =====================
float readAccelMagnitude() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  return sqrt(
    (a.acceleration.x * a.acceleration.x) +
    (a.acceleration.y * a.acceleration.y) +
    (a.acceleration.z * a.acceleration.z)
  );
}

// =====================
// setup()
// =====================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // --- TFT Init ---
  tft.init(170, 320);
  tft.setRotation(3);
  tft.fillScreen(BLACK);
  Serial.println("TFT initialized");
  delay(500);

  // --- MPU Init with MPU6500 fallback ---
  Serial.println("Initializing MPU6050...");

  if (!mpu.begin()) {
    // Standard init failed — check if it's an MPU6500 (WHO_AM_I = 0x70)
    Wire.beginTransmission(MPU6050_I2CADDR_DEFAULT);
    Wire.write(MPU6050_WHO_AM_I);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_I2CADDR_DEFAULT, 1);
    uint8_t whoami = Wire.read();

    if (whoami == 0x70) {
      // MPU6500 confirmed — register-compatible, safe to proceed
      Serial.println("MPU6500 detected (WHO_AM_I=0x70), continuing...");
    } else {
      // Truly unrecognized — halt
      Serial.print("MPU init failed. Unknown WHO_AM_I: 0x");
      Serial.println(whoami, HEX);
      tft.setTextSize(2);
      tft.setTextColor(RED);
      tft.setCursor(5, 5);
      tft.println("MPU Init Failed!");
      while (1) delay(10);
    }
  }

  Serial.println("MPU Found!");
  tft.fillScreen(BLACK);
  tft.setTextSize(2);
  tft.setTextColor(GREEN);
  tft.setCursor(5, 5);
  tft.println("MPU Found!");
  delay(1000);

  tft.fillScreen(BLACK);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(5, 5);
  tft.println("Ready for motion!");
  delay(1000);

  // --- Seed wasMoving from actual sensor state at boot ---
  float initialMag = readAccelMagnitude();
  wasMoving = (initialMag < STILL_THRESHOLD_LOW || initialMag > STILL_THRESHOLD_HIGH);

  if (wasMoving) {
    drawMoving();
    Serial.println("Initial state: Moving");
  } else {
    drawStill();
    Serial.println("Initial state: Still");
  }
}

// =====================
// loop()
// =====================
void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Calculate acceleration magnitude
  float accelMagnitude = sqrt(
    (a.acceleration.x * a.acceleration.x) +
    (a.acceleration.y * a.acceleration.y) +
    (a.acceleration.z * a.acceleration.z)
  );

  // Raw motion detection
  bool rawMotionDetected = (accelMagnitude < STILL_THRESHOLD_LOW || accelMagnitude > STILL_THRESHOLD_HIGH);

  // Debounce counters
  if (rawMotionDetected) {
    movingCounter++;
    stillCounter = 0;
  } else {
    stillCounter++;
    movingCounter = 0;
  }

  // Determine debounced state
  bool currentStateMoving;
  if (movingCounter >= DEBOUNCE_COUNT_MOVING) {
    currentStateMoving = true;
  } else if (stillCounter >= DEBOUNCE_COUNT_STILL) {
    currentStateMoving = false;
  } else {
    currentStateMoving = wasMoving; // Hold previous state until threshold met
  }

  // Handle state transitions
  if (currentStateMoving && !wasMoving) {
    drawMoving();
    Serial.println("\n--- Moving ---");
    wasMoving = true;
  } else if (!currentStateMoving && wasMoving) {
    drawStill();
    Serial.println("\n--- Still ---");
    wasMoving = false;
  }

  // Print sensor data to Serial only while moving
  if (currentStateMoving) {
    Serial.print("AccelX:"); Serial.print(a.acceleration.x, 2);
    Serial.print(", AccelY:"); Serial.print(a.acceleration.y, 2);
    Serial.print(", AccelZ:"); Serial.print(a.acceleration.z, 2);
    Serial.print(" | Magnitude:"); Serial.print(accelMagnitude, 2);
    Serial.print(" | GyroX:"); Serial.print(g.gyro.x, 2);
    Serial.print(", GyroY:"); Serial.print(g.gyro.y, 2);
    Serial.print(", GyroZ:"); Serial.println(g.gyro.z, 2);
  }

  delay(20);
}

