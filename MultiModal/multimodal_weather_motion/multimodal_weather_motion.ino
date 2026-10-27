// Acknowledgments
// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373
// This material is based upon work supported by the National Science Foundation under Grant No. 2405373.
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.

#include <weather_conditions_inferencing.h>        // Weather Model
#include <Lab_7_motion_detection_v1_inferencing.h> // Motion Model
#include <Adafruit_MPU6050.h>
#include <Adafruit_BME280.h>
#include <Adafruit_ST7789.h>

// === Configuration ===
#define MOTION_HZ 60
#define WEATHER_UPDATE_MS 10000 // Update weather every 10 seconds
#define MOTION_INTERVAL_MS (1000 / (MOTION_HZ + 1))

// === Hardware Initialization ===
Adafruit_MPU6050 mpu;
Adafruit_BME280 bme;
#define TFT_CS 33
#define TFT_DC 25
#define TFT_RST 26
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// === Global State ===
String currentWeather = "SUNNY"; // Default context
unsigned long lastWeatherUpdate = 0;
unsigned long lastMotionSample = 0;

float motion_features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE]; // Must match motion model size
size_t motion_ix = 0;

void setup() {
    Serial.begin(115200);
    display.init(170, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    if (!mpu.begin() || !bme.begin(0x76)) {
        Serial.println("Sensor Init Failed!");
        while (1);
    }

    // Set MPU6050 for tree sway (low G range for sensitivity)
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
    unsigned long now = millis();

    // 1. ASYNC WEATHER UPDATE (Context)
    if (now - lastWeatherUpdate > WEATHER_UPDATE_MS) {
        updateWeatherContext();
        lastWeatherUpdate = now;
    }

    // 2. MOTION SAMPLING (Continuous)
    if (now - lastMotionSample > MOTION_INTERVAL_MS) {
        lastMotionSample = now;
        
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        motion_features[motion_ix++] = a.acceleration.x;
        motion_features[motion_ix++] = a.acceleration.y;
        motion_features[motion_ix++] = a.acceleration.z;

        if (motion_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            runMotionInference();
            motion_ix = 0;
        }
    }
}

void updateWeatherContext() {
    float features[3] = { bme.readTemperature(), bme.readPressure()/100.0F, bme.readHumidity() };
    
    signal_t signal;
    numpy::signal_from_buffer(features, 3, &signal);
    
    ei_impulse_result_t result;
    // Note: ensure you call the correct weather namespace if conflicts occur
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res == EI_IMPULSE_OK) {
        float max_val = 0;
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            if (result.classification[i].value > max_val) {
                max_val = result.classification[i].value;
                currentWeather = String(result.classification[i].label);
            }
        }
    }
}

void runMotionInference() {
    signal_t signal;
    numpy::signal_from_buffer(motion_features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    
    ei_impulse_result_t result;
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res == EI_IMPULSE_OK) {
        String motion = "";
        float motion_conf = 0;
        
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            if (result.classification[i].value > motion_conf) {
                motion_conf = result.classification[i].value;
                motion = String(result.classification[i].label);
            }
        }

        processMultimodalLogic(motion, motion_conf);
    }
}

void processMultimodalLogic(String motion, float confidence) {
    String finalAlert = "";
    uint16_t color = ST77XX_WHITE;

    // Multimodal Logic Table
    if (motion == "Wave") {
        if (currentWeather == "Cloudy") { 
            finalAlert = "STORM INCOMING"; 
            color = ST77XX_ORANGE; 
        }
        else if (currentWeather == "Clear / Partly Cloudy (125)") { 
            finalAlert = "WINDY"; 
        }
        else if (currentWeather == "Rainy") { 
            finalAlert = "STORM"; 
            color = ST77XX_RED; 
        }
    } 
    else if (motion == "Idle") {
        if (currentWeather == "SUNNY") { 
            finalAlert = "PLEASANT"; 
            color = ST77XX_GREEN; 
        }
        else { 
            finalAlert = currentWeather + " (STILL)"; 
        }
    }

    // Update Screen
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(0, 0);
    display.setTextSize(3);
    display.setTextColor(color);
    display.println(finalAlert);
    
    display.setTextSize(2);
    display.setCursor(0, 80);
    display.setTextColor(ST77XX_WHITE);
    display.print("Context: "); display.println(currentWeather);
    display.print("Motion: "); display.println(motion);
}