
// Acknowledgments
// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373
// This material is based upon work supported by the National Science Foundation under Grant No. 2405373.

#include <Phone_Lift_Detection_inferencing.h>
#include <MPU6050.h>
#include <VL53L0X.h>
#include <Adafruit_ST7789.h>

// ================== CONFIG ==================
#define MOTION_HZ             50
#define MOTION_INTERVAL_MS    (2000 / (MOTION_HZ + 1))
#define PROXIMITY_THRESH_MM   400     // 40cm (Adjust based on how close user holds device)
#define SCREEN_TIMEOUT_MS     5000    // Screen turns off after 5 seconds of idle

// ================== HARDWARE INITIALIZATION ==================
MPU6050 mpu;
VL53L0X sensor; // ToF Sensor

#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  26
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Buffer for Edge Impulse
float motion_features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t motion_ix = 0;
unsigned long lastMotionSample = 0;

// State Tracking
bool isScreenOn = true; 
unsigned long lastWakeTime = 0;

// =================================================
//                HELPER: SCREEN CONTROL
// =================================================
void wakeScreen() {
    if (!isScreenOn) {
        // In a real phone, this would enable the backlight
        // For ST7789, we just draw the UI
        isScreenOn = true;
        display.enableDisplay(true); // Some ST7789 libs support sleep/wake
        display.fillScreen(ST77XX_BLACK); // Clear previous state
        Serial.println("STATE: WAKE");
    }
    lastWakeTime = millis(); // Reset timeout timer
}

void sleepScreen() {
    if (isScreenOn) {
        display.fillScreen(ST77XX_BLACK);
        isScreenOn = false;
        Serial.println("STATE: SLEEP");
    }
}

// =================================================
//            WAKE LOGIC (FUSION)
// =================================================
void processWakeLogic(String motion, float confidence) {
    
    // 1. Read ToF Distance (Proximity)
    uint16_t distance = sensor.readRangeContinuousMillimeters();
    if (sensor.timeoutOccurred()) { Serial.print(" TIMEOUT"); }

    // 2. Define Criteria
    // We wake if: 
    //    a) Motion is a "Lift" gesture
    //    b) AND Distance is within range (e.g., user looking at phone)
    bool validGesture = (motion == "lift_rotate" || motion == "lift_rotate_y" || motion == "lift");
    bool validProximity = (distance > 10 && distance < PROXIMITY_THRESH_MM);

    // 3. State Machine
    if (validGesture && validProximity && confidence > 0.9) {
        wakeScreen();
        
        // Draw Interface
        display.setCursor(20, 50);
        display.setTextSize(3);
        display.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
        display.println("HELLO!");
        
        display.setCursor(20, 100);
        display.setTextSize(2);
        display.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        display.println("UNLOCKED");
    }
    else if (isScreenOn && (millis() - lastWakeTime > SCREEN_TIMEOUT_MS)) {
        // If screen is on, but no valid activity for X seconds -> Sleep
        sleepScreen();
    }
}

// =================================================
//                MOTION INFERENCE
// =================================================
void runMotionInference() {

    signal_t signal;
    numpy::signal_from_buffer(motion_features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    ei_impulse_result_t result;
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK) {
        Serial.println("Motion inference failed!");
        return;
    }

    String motion = "idle";
    float  conf   = 0.0f;

    // Find highest confidence label
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > conf) {
            conf   = result.classification[i].value;
            motion = String(result.classification[i].label);
            Serial.println(motion);
        }
    }

    processWakeLogic(motion, conf);
}

// =================================================
//                    SETUP
// =================================================
void setup() {
    Serial.begin(115200);

    // Init Display
    display.init(170, 320); 
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    // Init MPU6050

    mpu.initialize();

    // Init ToF Sensor (VL53L0X)
    Wire.begin(); // Make sure I2C is started
    sensor.setTimeout(500);
    if (!sensor.init()) {
        Serial.println("Failed to detect and initialize sensor!");
        while (1) {}
    }
    // Start continuous back-to-back mode
    sensor.startContinuous();

    Serial.println("System Initialized: Lift to Wake Active");
    lastWakeTime = millis(); // Start timer
}

// =================================================
//                     LOOP
// =================================================
void loop() {
    unsigned long now = millis();

    // Motion sampling (fast)
    if (now - lastMotionSample > MOTION_INTERVAL_MS) {
        lastMotionSample = now;

        int16_t ax, ay, az;
        int16_t gx, gy, gz;
        mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
        
        motion_features[motion_ix++] = gx;
        motion_features[motion_ix++] = gy;
        motion_features[motion_ix++] = gz;

        // If buffer full, run inference
        if (motion_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            runMotionInference();
            motion_ix = 0; // Reset buffer
        }
    }
}