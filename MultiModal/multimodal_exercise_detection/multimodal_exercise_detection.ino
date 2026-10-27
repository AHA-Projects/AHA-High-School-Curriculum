#include <Lab_7_motion_detection_v1_inferencing.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_ST7789.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// --- Hardware Pins ---
#define TFT_CS 33
#define TFT_DC 25
#define TFT_RST 26
const int buzzerPin = 12;

// --- Motion Parameters ---
Adafruit_MPU6050 mpu;         
#define FREQUENCY_HZ 60
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t feature_ix = 0;
unsigned long last_motion_sample = 0;
String currentMotion = "Idle";

// --- Heart Rate Parameters ---
MAX30105 particleSensor;
const byte RATE_SIZE = 4; 
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
int beatAvg = 0;

const long IR_THRESHOLD = 30000;

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

bool fingerPresent = false;

void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);
  
  // Initialize Display
  display.init(170, 320);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);

  // Initialize MPU6050
  if (!mpu.begin()) { Serial.println("MPU6050 Fail"); while(1); }
  
  // Initialize MAX30105
  if (!3.begin(Wire, I2C_SPEED_FAST)) { Serial.println("MAX30105 Fail"); while(1); }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
}

void loop() {
  long irValue = particleSensor.getIR();

  // --- 1. SENSOR & FINGER DETECTION ---
  if (irValue > IR_THRESHOLD) {
    fingerPresent = true;

    // Heart rate sensing (High Frequency)
    if (checkForBeat(irValue)) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      float bpm = 60 / (delta / 1000.0);
      if (bpm < 255 && bpm > 20) {
        rates[rateSpot++] = (byte)bpm;
        rateSpot %= RATE_SIZE;
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
        
        // Heartbeat Buzz (works well when a buzzer sensor is integrated)
        //digitalWrite(buzzerPin, HIGH); delay(2); digitalWrite(buzzerPin, LOW);
      }
    }

    // Motion Data Collection (Sampling)
    if (millis() - last_motion_sample > INTERVAL_MS) {
      last_motion_sample = millis();
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      features[feature_ix++] = a.acceleration.x;
      features[feature_ix++] = a.acceleration.y;
      features[feature_ix++] = a.acceleration.z;

      // Run Inference if buffer is full
      if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        signal_t signal;
        ei_impulse_result_t result;
        numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
        run_classifier(&signal, &result, false);

        float topVal = 0;
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
          if (result.classification[ix].value > topVal) {
            topVal = result.classification[ix].value;
            currentMotion = String(result.classification[ix].label);
          }
        }
        
        // ONLY update the display with stats if finger is present
        updateDisplay();
        feature_ix = 0;
      }
    }
  } 
  else {
    // --- 2. NO FINGER DETECTED ---
    if (fingerPresent) {
      fingerPresent = false;
      beatAvg = 0;
      currentMotion = "Idle";
      for(byte x=0; x < RATE_SIZE; x++) rates[x] = 0;
      
      // Clear screen and show prompt
      display.fillScreen(ST77XX_BLACK);
      display.setCursor(0, 40);
      display.setTextColor(ST77XX_WHITE);
      display.setTextSize(2);
      display.println("Finger Removed");
      display.println("Place finger to resume...");
    }
    digitalWrite(buzzerPin, LOW); // Ensure buzzer is off
  }
}

void updateDisplay() {
  display.fillScreen(ST77XX_BLACK);
  display.setCursor(0, 0);
  display.setTextSize(3);

// Logic for Status Classification
if (beatAvg+10 < 60 && currentMotion == "idle") {
  // Scenario 1: Low HR and no movement
  display.setTextColor(ST77XX_WHITE);
  display.println("RESTING STATE");
} 
else if (beatAvg+10 > 60 && currentMotion == "idle") {
  // Scenario 2: High HR but no movement (Warning)
  display.setTextColor(ST77XX_RED);
  display.println("ABNORMAL: HIGH HR/IDLE");
} 
else if (beatAvg+10 > 60 && currentMotion == "updown") {
  // Scenario 3: High HR with specific UpDown motion
  display.setTextColor(ST77XX_GREEN);
  display.println("EXERCISING");
} 
else {
  // Default: Handles Wave, Snake, or other combinations
  display.setTextColor(ST77XX_YELLOW);
  display.println("ACTIVE");
}

  display.setCursor(0, 80);
  display.setTextSize(4);
  display.print("BPM: "); display.println(beatAvg+10);
  display.setTextSize(2);
  display.print("Motion: "); display.println(currentMotion);
}