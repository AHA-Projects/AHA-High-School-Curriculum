// Acknowledgments
// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373
// This material is based upon work supported by the National Science Foundation under Grant No. 2405373.
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.

#include <Lab_7_motion_detection_v1_inferencing.h> // Motion EI model
#include <Adafruit_MPU6050.h>
#include <Adafruit_BME280.h>
#include <Adafruit_ST7789.h>

// ================== CONFIG ==================
#define MOTION_HZ             62.5
#define WEATHER_UPDATE_MS     10000   // 10 seconds
#define MOTION_INTERVAL_MS    (1000 / (MOTION_HZ + 1))

// ================== HARDWARE INITIALIZATION==================
Adafruit_MPU6050 mpu;
Adafruit_BME280  bme;

#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  26
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ================== GLOBAL STATE ==================
String currentWeather = "SUNNY";

unsigned long lastWeatherUpdate = 0;
unsigned long lastMotionSample  = 0;

float motion_features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t motion_ix = 0;

// =================================================
//                WEATHER LOGIC
// =================================================
String classifyWeather(float temp, float humidity, float pressure_hPa) {

    // Strong rain indicator
    if (pressure_hPa < 1005.0 && humidity > 80.0) {
        return "RAINY";
    }

    // Cloudy / unstable
    if (pressure_hPa >= 1005.0 && pressure_hPa <= 1015.0) {
        return "CLOUDY";
    }

    // Clear and stable
    if (humidity < 60.0 && temp > 24.0) {
        return "SUNNY";
    }

    return "CLOUDY"; // safe fallback
}

void updateWeatherContext() {

    float temp     = bme.readTemperature();
    float humidity = bme.readHumidity();
    float pressure = bme.readPressure() / 100.0F;

    currentWeather = classifyWeather(temp, humidity, pressure);

    Serial.print("[Weather] ");
    Serial.print(currentWeather);
    Serial.print(" | T=");
    Serial.print(temp, 1);
    Serial.print("C  H=");
    Serial.print(humidity, 1);
    Serial.print("%  P=");
    Serial.print(pressure, 1);
    Serial.println(" hPa");
}

// =================================================
//                MOTION INFERENCE
// =================================================
void runMotionInference() {

    signal_t signal;
    numpy::signal_from_buffer(
        motion_features,
        EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
        &signal
    );

    ei_impulse_result_t result;
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK) {
        Serial.println("Motion inference failed!");
        return;
    }

    String motion = "Idle";
    float  conf   = 0.0f;

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > conf) {
            conf   = result.classification[i].value;
            motion = String(result.classification[i].label);
        }
    }

    // Confidence gate
    if (conf < 0.6) {
        motion = "Idle";
    }

    processMultimodalLogic(motion, conf);
}

// =================================================
//            MULTIMODAL FUSION LOGIC
// =================================================
void processMultimodalLogic(String motion, float confidence) {

    String finalAlert = "";
    uint16_t color   = ST77XX_WHITE;

    if (motion == "wave" || motion == "updown") {

        if (currentWeather == "SUNNY") {
            finalAlert = "WINDY";
            color = ST77XX_YELLOW;
        }
        else if (currentWeather == "CLOUDY") {
            finalAlert = "STORM INCOMING";
            color = ST77XX_ORANGE;
        }
        else if (currentWeather == "RAINY") {
            finalAlert = "STORM";
            color = ST77XX_RED;
        }
    }
    else { // Idle

        if (currentWeather == "SUNNY") {
            finalAlert = "PLEASANT";
            color = ST77XX_GREEN;
        }
        else if (currentWeather == "CLOUDY") {
            finalAlert = "OVERCAST";
        }
        else if (currentWeather == "RAINY") {
            finalAlert = "LIGHT RAIN";
            color = ST77XX_BLUE;
        }
    }

    // ================= DISPLAY =================
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(0, 0);
    display.setTextSize(3);
    display.setTextColor(color);
    display.println(finalAlert);

    display.setTextSize(2);
    display.setCursor(0, 80);
    display.setTextColor(ST77XX_WHITE);
    display.print("Weather: ");
    display.println(currentWeather);

    display.print("Motion: ");
    if (motion == "wave" || motion == "snake" || motion == "updown")
    {
        display.println("Active");
    }
    else{
        display.println("Stationary");
    }
    
}

// =================================================
//                    SETUP
// =================================================
void setup() {

    Serial.begin(115200);

    display.init(170, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    if (!mpu.begin()) {
        Serial.println("MPU6050 init failed!");
        while (1);
    }

    if (!bme.begin(0x76)) {
        Serial.println("BME280 init failed!");
        while (1);
    }

    // Motion sensitivity tuned for sway/wave
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("System Initialized");
}

// =================================================
//                     LOOP
// =================================================
void loop() {

    unsigned long now = millis();

    // Weather update (slow)
    if (now - lastWeatherUpdate > WEATHER_UPDATE_MS) {
        updateWeatherContext();
        lastWeatherUpdate = now;
    }

    // Motion sampling (fast)
    if (now - lastMotionSample > MOTION_INTERVAL_MS) {

        lastMotionSample = now;

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        motion_features[motion_ix++] = a.acceleration.x;
        motion_features[motion_ix++] = a.acceleration.y;
        motion_features[motion_ix++] = a.acceleration.z;

        if (motion_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            runMotionInference();
            motion_ix = 0;
        }
    }
}
