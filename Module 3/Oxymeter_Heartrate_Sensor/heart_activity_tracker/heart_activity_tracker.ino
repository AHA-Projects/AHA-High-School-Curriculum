// Acknowledgments
// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373
// This material is based upon work supported by the National Science Foundation under Grant No. 2405373.
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.

// --- Include Libraries ---
#include <Wire.h> // Required for communication (MAX30105)
#include "MAX30105.h" // SparkFun MAX30105 sensor library
#include "heartRate.h" // SparkFun Heart Rate algorithm


// --- Adafruit Display Libraries ---
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>             // Required for SPI communication for the OLED(ST7789)

// --- Display Pin Definitions ---
#define TFT_CS   33  // Chip Select control pin
#define TFT_DC    25  // Data/Command select pin
#define TFT_RST    26  // Reset pin 


// --- Buzzer Pin Definition --- (Check only if buzzer used)
const int buzzerPin = 12; // Buzzer connected to GPIO 12

// --- Display Object ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// defining variables for color channels to be used later. (Variables are temporary storage location for repetitive usage of certain information, it makes the code more readable)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define BLUE    0x001F
#define GREEN   0x07E0
#define RED     0xF800
#define YELLOW  0xFFE0 // Adding Yellow for 'Excited'

// --- MAX30105 Sensor Object ---
MAX30105 particleSensor;

// --- Heart Rate Variables ---
const byte RATE_SIZE = 4; // Number of readings to average. Increase for more averaging.
byte rates[RATE_SIZE];    // Array of heart rates readings
byte rateSpot = 0;        // Index for the rates array
long lastBeat = 0;        // Time (millis()) of the last detected beat



float beatsPerMinute; // Current instantaneous BPM from last two beats
int beatAvg = 0;      // Averaged BPM



// --- Finger Detection Threshold ---
const long IR_DETECTION_THRESHOLD = 30000; // Threshold for considering a finger is present



// --- State Variable for Display ---
bool fingerPresent = false; // Track if finger was detected in the previous loop iteration




void setup() {
  Serial.begin(115200); // Start serial for debugging
  Serial.println("Initializing...");

  // --- Configure Buzzer Pin ---
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // Ensure buzzer is off initially

  // --- Display Initialization ---
  // For a 320x170 display, init with these dimensions first
  tft.init(170, 320);
  // Set rotation
  tft.setRotation(3); // Using rotation 3

  tft.fillScreen(BLACK); // Clear the entire screen

  // --- Initial Display Message ---
  displayNoFinger();


  // --- Sensor Initialization ---
  Serial.println("Initializing MAX30105...");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    // Display sensor error on screen
    tft.fillScreen(BLACK); // Clear screen
    tft.setCursor(0,0); // Overwrite initial message
    tft.setTextColor(RED);
    tft.setTextSize(2);
    tft.println("Sensor Error!");
    tft.setCursor(0,20);
    tft.println("Check Wiring!");
    while (1); // Halt if sensor not found
  }
  Serial.println("MAX30105 initialized.");

  // Configure sensor with default settings
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x1F); // Medium Red LED
  particleSensor.setPulseAmplitudeIR(0x1F); // Medium IR LED
  particleSensor.setPulseAmplitudeGreen(0); // Green LED off

  Serial.println("Place your index finger on the sensor with steady pressure.");
}

void loop() {
  // Read the IR value (this is the most reliable for pulse detection)
  long irValue = particleSensor.getIR();

  // --- Finger Detection Logic ---
  if (irValue > IR_DETECTION_THRESHOLD) {
    if (!fingerPresent) {
      // Finger just placed, clear "No finger?" message and prepare for BPM display
      tft.fillScreen(BLACK);
      tft.setCursor(0,0);
      tft.setTextColor(WHITE);
      tft.setTextSize(2);
      tft.println("Processing..."); // Indicate that BPM calculation is starting
      fingerPresent = true;
    }

    // --- Heart Rate Detection ---
    // checkForBeat returns true when a pulse is detected
    if (checkForBeat(irValue) == true) {
      // We sensed a beat!
      long delta = millis() - lastBeat; // Time since last beat
      lastBeat = millis(); // Update last beat time

      beatsPerMinute = 60 / (delta / 1000.0); // Calculate instantaneous BPM

      // Store and average BPM if it's within a reasonable range (20-255 BPM)
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the array
        rateSpot %= RATE_SIZE; // Wrap variable (circular buffer)

        // Take average of readings
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;

        // --- Display Update for BPM and Status ---

        tft.fillRect(0, 40, tft.width(), tft.height() - 40, BLACK);

        // Display Average BPM
        tft.setCursor(0, 45); // Start below initial message area
        tft.setTextColor(WHITE); // Set text color for BPM value
        tft.setTextSize(2);     // Size 2 for BPM value
        tft.print("Avg BPM: ");
        tft.println(beatAvg);

        // Display Status Label based on Avg BPM
        tft.setCursor(0, 70); // Move down for the status label (size 2 is 16px high, size 3 is 24px high)
        tft.setTextSize(3);     // Size 3 for status label
        tft.print("Status: ");

        // Set color and print status label based on ranges
        if (beatAvg < 80) {
            tft.setTextColor(GREEN); // Resting in Green
            tft.println("Resting");
        } else if (beatAvg >= 80 && beatAvg <= 100) {
            tft.setTextColor(YELLOW); // Excited in Yellow
            tft.println("Excited");
        } else { // beatAvg > 100
            tft.setTextColor(RED); // Worked Up in Red
            tft.println("Worked Up");
        }
        tft.setTextColor(WHITE); // Reset text color for next time if needed

        // --- Buzzer Logic: Mimic Heartbeat ---
        buzzHeartbeat(delta); // Trigger a heartbeat buzz based on the time since the last beat

        // Serial debug output whenever average is updated for debugging purposes
        Serial.print("IR="); Serial.print(irValue);
        Serial.print(", instant BPM="); Serial.print(beatsPerMinute);
        Serial.print(", Avg BPM="); Serial.println(beatAvg);

      }
    }
  } else {
    // --- No finger detected ---
    if (fingerPresent) {
      // Finger just removed, display "No finger?" message
      displayNoFinger();
      fingerPresent = false;

      // Reset BPM and rates when finger is removed
      beatsPerMinute = 0;
      beatAvg = 0;
      for(byte x=0; x < RATE_SIZE; x++) rates[x] = 0; // Clear rates array
      rateSpot = 0;
      lastBeat = 0; // Reset last beat time
    }
     // Ensure buzzer is off when no finger is detected
     digitalWrite(buzzerPin, LOW);
  }
  delay(10);
}


// --- Display "Place finger on sensor..." ---
void displayNoFinger() {
  tft.fillScreen(BLACK); // Clear the entire screen
  tft.setCursor(0,0);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println("Please place your finger ");
  tft.setCursor(0, 20);
  tft.print("on the sensor...");
}


// --- Buzzer function to mimic heartbeat rhythm ---
void buzzHeartbeat(long beatInterval_ms) {


  if (beatInterval_ms > 0) {
  for (int i =0 ; i < 10; i++)
  {
    digitalWrite(buzzerPin, HIGH);
    delay(2);
    digitalWrite(buzzerPin, LOW);
    delay(2);
  } 

  } else {
    // If somehow beatInterval_ms is not positive, ensure buzzer is off.
     digitalWrite(buzzerPin, LOW);
  }
}