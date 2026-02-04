// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_ST7789.h>   // Library for the ST7789 TFT screen
#include <SPI.h>             // SPI library needed for ST7789 TFT


#define TFT_CS    33  // TFT Chip Select pin
#define TFT_DC    25  // TFT Data/Command pin
#define TFT_RST   26  // TFT Reset pin

// --- Color Definitions 
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GREEN   0x07E0
#define RED     0xF800
#define BLUE    0x001F
#define YELLOW  0xFFE0

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_MPU6050 mpu;

// Define a threshold for motion detection based on acceleration magnitude
const float STILL_THRESHOLD_LOW = 9.0;
const float STILL_THRESHOLD_HIGH = 10.5;

const int DEBOUNCE_COUNT_MOVING = 5; // How many consecutive 'moving' readings to confirm motion
const int DEBOUNCE_COUNT_STILL = 10; // How many consecutive 'still' readings to confirm stillness

int movingCounter = 0;
int stillCounter = 0;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10); 

  // Initialize TFT
  tft.init(170, 320);     // Tell the screen to get ready, specifying its pixel width and height.
  Serial.println("TFT initialized");
  tft.setRotation(3);     // Rotate the screen (e.g., to landscape).
  tft.fillScreen(BLACK);  // Make the whole screen black to start with.
  delay(500);

  Serial.println("Adafruit MPU6050 test!");

  // Try to initialize MPU6050!
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    tft.setTextSize(2);             
    tft.setTextColor(RED);        
    tft.setCursor(5, 5);            
    tft.println("MPU6050 Init Failed!"); 
    while (1) { // Halt if MPU fails to initialize
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
  tft.fillScreen(BLACK);          // Clear screen
  tft.setTextSize(2);             
  tft.setTextColor(GREEN);        
  tft.setCursor(5, 5);            
  tft.println("MPU6050 Found!"); 
  delay(1000);

  Serial.println("MPU6050 initialized for magnitude detection.");
  tft.fillScreen(BLACK);          // Clear screen
  tft.setTextSize(2);             
  tft.setTextColor(WHITE);        
  tft.setCursor(5, 5);            
  tft.println("Ready for motion!");
  delay(1000);

  // Initial display when still
  tft.fillScreen(BLACK);          // Clear the screen
  tft.setTextSize(2);             // Set text size to 2
  tft.setTextColor(WHITE);        // Set text color
  tft.setCursor(5, 5);            // Position text
  tft.print("Still");             // Print "Still"
  Serial.println("Initial state: Still"); // Print initial state to Serial
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp); // Get accelerometer and gyroscope data

  // Calculate the vector magnitude of acceleration
  float accelMagnitude = sqrt(
    (a.acceleration.x * a.acceleration.x) +
    (a.acceleration.y * a.acceleration.y) +
    (a.acceleration.z * a.acceleration.z)
  );

  // Determine if raw reading indicates moving or still based on the magnitude
  bool rawMotionDetected = (accelMagnitude < STILL_THRESHOLD_LOW || accelMagnitude > STILL_THRESHOLD_HIGH);

  // Debouncing logic
  if (rawMotionDetected) {
    movingCounter++;
    stillCounter = 0; // Reset still counter while the sensor is moving
  } else {
    stillCounter++;
    movingCounter = 0; // Reset moving counter while sensor is still
  }

  static bool wasMoving = false; // Tracks the previous debounced state
  bool currentStateMoving;       // Current determined debounced state

  if (movingCounter >= DEBOUNCE_COUNT_MOVING) {
    currentStateMoving = true;
  } else if (stillCounter >= DEBOUNCE_COUNT_STILL) {
    currentStateMoving = false;
  } else {
    // If neither threshold is met, maintain the previous debounced state
    currentStateMoving = wasMoving;
  }

  // Update LED
  if (currentStateMoving && !wasMoving) { // Transition from Still to Moving
    tft.fillScreen(BLACK); 
    tft.setTextSize(2);    
    tft.setTextColor(RED); 
    tft.setCursor(5, 5);   
    tft.print("Moving");   
    wasMoving = true;
  } else if (!currentStateMoving && wasMoving) { // Transition from Moving to Still
    tft.fillScreen(BLACK); 
    tft.setTextSize(2);    
    tft.setTextColor(GREEN); 
    tft.setCursor(5, 5);   
    tft.print("Still");   
    Serial.println("\n--- Still ---");
    wasMoving = false;
  }

  // Print sensor data to Serial Monitor only if currently in the 'Moving' state
  if (currentStateMoving) {
    Serial.print("AccelX:"); Serial.print(a.acceleration.x, 2);
    Serial.print(", AccelY:"); Serial.print(a.acceleration.y, 2);
    Serial.print(", AccelZ:"); Serial.print(a.acceleration.z, 2);
    Serial.print(" | Magnitude:"); Serial.print(accelMagnitude, 2);
    Serial.print(" | GyroX:"); Serial.print(g.gyro.x, 2);
    Serial.print(", GyroY:"); Serial.print(g.gyro.y, 2);
    Serial.print(", GyroZ:"); Serial.print(g.gyro.z, 2);
    Serial.println("");
  }

  delay(20); // Small delay to allow more frequent sampling for debounce
}