// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.


#include <Lab_7_motion_detection_v1_inferencing.h>
#include <Adafruit_MPU6050.h>                 // Library to interact with the MPU6050 motion sensor
#include <Adafruit_Sensor.h>                  // Unified sensor interface used by Adafruit libraries
#include <Wire.h>                             // Library for I2C communication
#include <WiFi.h>                             // WiFi library for network capability (not used in this code)
// #include <GyverOLED.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 


// === Define Sampling Parameters ===
#define FREQUENCY_HZ 60                          // Sampling rate: 60 times per second
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))  // Time between samples in milliseconds


// === Initialize Sensor and Buffer Variables ===
Adafruit_MPU6050 mpu;          


#define TFT_CS   33  // Chip Select control pin
#define TFT_DC    25  // Data/Command select pin
#define TFT_RST   26  // Reset pin (or connect to RST, see below)
//GyverOLED<SSH1106_128x64> display;
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);



float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];  // Buffer to store sampled features
size_t feature_ix = 0;                               // Current index in the features buffer
static unsigned long last_interval_ms = 0;    
unsigned long labelStartTime = 0;
String currentLabel = "";
String lastLabel = "";
const unsigned long thresholdTime = 500; 

// === Setup Function: Runs Once ===
void setup() {
  Serial.begin(115200);  // Initialize serial communication at 115200 baud

  display.init(170, 320);
  display.setRotation(3);
  // Try to initialize the MPU6050 sensor
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");  // Error message if sensor not found
    while (1) {
      delay(10);  // Halt the program in an infinite loop
    }
  }
  Serial.println("MPU6050 Found!");  // Success message

  // Set accelerometer sensitivity to ±8G
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G: Serial.println("+-2G"); break;
    case MPU6050_RANGE_4_G: Serial.println("+-4G"); break;
    case MPU6050_RANGE_8_G: Serial.println("+-8G"); break;
    case MPU6050_RANGE_16_G: Serial.println("+-16G"); break;
  }

  // Set gyroscope sensitivity to ±500 degrees/second
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG: Serial.println("+- 250 deg/s"); break;
    case MPU6050_RANGE_500_DEG: Serial.println("+- 500 deg/s"); break;
    case MPU6050_RANGE_1000_DEG: Serial.println("+- 1000 deg/s"); break;
    case MPU6050_RANGE_2000_DEG: Serial.println("+- 2000 deg/s"); break;
  }

  // Apply digital low-pass filter to reduce noise
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ: Serial.println("260 Hz"); break;
    case MPU6050_BAND_184_HZ: Serial.println("184 Hz"); break;
    case MPU6050_BAND_94_HZ: Serial.println("94 Hz"); break;
    case MPU6050_BAND_44_HZ: Serial.println("44 Hz"); break;
    case MPU6050_BAND_21_HZ: Serial.println("21 Hz"); break;
    case MPU6050_BAND_10_HZ: Serial.println("10 Hz"); break;
    case MPU6050_BAND_5_HZ: Serial.println("5 Hz"); break;
  }

  // Show Edge Impulse model input size and label count
  Serial.print("Features: ");
  Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  Serial.print("Label count: ");
  Serial.println(EI_CLASSIFIER_LABEL_COUNT);
}

// === Main Loop: Runs Continuously ===
void loop() {
  sensors_event_t a, g, temp;  // Structures to hold sensor data (acceleration, gyro, temperature)

  // Check if enough time has passed to take a new sample
  if (millis() > last_interval_ms + INTERVAL_MS) {
    last_interval_ms = millis();  // Update timestamp

    mpu.getEvent(&a, &g, &temp);  // Read values from the sensor

    // Store acceleration data into features buffer
    features[feature_ix++] = a.acceleration.x;
    features[feature_ix++] = a.acceleration.y;
    features[feature_ix++] = a.acceleration.z;

    // If buffer is full, it's time to classify the motion
    if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      Serial.println("Running the inference...");

      signal_t signal;               // Structure for the input signal to the model
      ei_impulse_result_t result;   // Structure for the classification result

      // Convert features buffer to signal format
      int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
      if (err != 0) {
        ei_printf("Failed to create signal from buffer (%d)\n", err);
        return;  // Abort inference if signal couldn't be created
      }

      // Run the classifier with the prepared signal
      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);
      if (res != 0) return;  // Abort if inference fails

      // Display timing and prediction results
      ei_printf("Predictions ");
      ei_printf("(DSP: %d ms., Classification: %d ms.)",
                result.timing.dsp, result.timing.classification);
      ei_printf(": \n");

      // Print the score for each possible class label
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);

        // (Optional) Store results in a char buffer if needed
        char buffer[50];
        sprintf(buffer, "    %s: %.5f", result.classification[ix].label, result.classification[ix].value);
        // You could send this buffer over WiFi or log it if needed
      }

    String newLabel = "";
    float highestProbability = 0.0;

    // Analyze the inference results to find the most probable label
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > highestProbability) {
            highestProbability = result.classification[i].value;
            newLabel = String(result.classification[i].label);
        }
    }

    // Check if the new label is the same as the last label
    if (newLabel == lastLabel) {
        if (millis() - labelStartTime > thresholdTime) {
            currentLabel = newLabel;
        }
    } else {
        lastLabel = newLabel;
        labelStartTime = millis();
    }
    String label = newLabel;
    label.toUpperCase();    // Display the current label on the OLED if it has been consistent for more than 2 seconds
    display.fillScreen(ST77XX_BLACK); 

    display.setCursor(0, 0);       
    display.setTextSize(4);        
    display.setTextColor(ST77XX_WHITE); 
    display.println(label);       

    display.setCursor(0, 50);      
    display.setTextSize(3);        
    display.print("Confidence:"); 
    display.print((highestProbability) * 100); 
    display.println("%");   

    // Debug: Print the current label and the status
    Serial.print("Current Label: ");
    Serial.print(currentLabel);
    Serial.print(" | Most probable: ");
    Serial.println(newLabel);

      feature_ix = 0;  // Reset the buffer index for the next cycle
    }
  }
}

// === Helper Function to Print Formatted Output ===
void ei_printf(const char* format, ...) {
  static char print_buf[1024] = { 0 };  // Buffer for formatted output

  va_list args;
  va_start(args, format);  // Start capturing variable arguments
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);  // Format the string
  va_end(args);

  if (r > 0) {
    Serial.write(print_buf);  // Send output to the Serial Monitor
  }
}


// // Acknowledgments

// // Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// // This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// // Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.


// #include <Lab_7_motion_detection_v1_inferencing.h>
// // #include <Adafruit_MPU6050.h> 
// #include <MPU6050.h>
// #include <Adafruit_ST7789.h>                 // Library to interact with the MPU6050 motion sensor
// #include <Adafruit_Sensor.h>                  // Unified sensor interface used by Adafruit libraries
// #include <Wire.h>                             // Library for I2C communication
// #include <WiFi.h>                             // WiFi library for network capability (not used in this code)
// #include <GyverOLED.h>

// // === Define Sampling Parameters ===
// #define FREQUENCY_HZ 60                          // Sampling rate: 60 times per second
// #define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))  // Time between samples in milliseconds

// // === Initialize Sensor and Buffer Variables ===
// MPU6050 mpu;

// #define TFT_CS   33  // Chip Select control pin
// #define TFT_DC    25  // Data/Command select pin
// #define TFT_RST   26  // Reset pin (or connect to RST, see below)
// //GyverOLED<SSH1106_128x64> display;

// Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);



// float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];  // Buffer to store sampled features
// size_t feature_ix = 0;                               // Current index in the features buffer
// static unsigned long last_interval_ms = 0;    
// unsigned long labelStartTime = 0;
// String currentLabel = "";
// String lastLabel = "";
// const unsigned long thresholdTime = 500; 

// // === Setup Function: Runs Once ===
// void setup() {
//   Serial.begin(115200);  // Initialize serial communication at 115200 baud

//   display.init(170, 320);
//   display.setRotation(3);
//   mpu.initialize();
//   // Try to initialize the MPU6050 sensor
//   if (mpu.testConnection() == false) {
//     Serial.println("Failed to find MPU6050 chip");  // Error message if sensor not found
//     while (1) {
//       delay(10);  // Halt the program in an infinite loop
//     }
//   }
//   mpu.setXAccelOffset(0); //Set your accelerometer offset for axis X
//   mpu.setYAccelOffset(0); //Set your accelerometer offset for axis Y
//   mpu.setZAccelOffset(0); //Set your accelerometer offset for axis Z
//   mpu.setXGyroOffset(0);  //Set your gyro offset for axis X
//   mpu.setYGyroOffset(0);  //Set your gyro offset for axis Y
//   mpu.setZGyroOffset(0);  //Set your gyro offset for axis Z
//   Serial.println("MPU6050 Found!");  // Success message
//   // Show Edge Impulse model input size and label count
//   Serial.print("Features: ");
//   Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
//   Serial.print("Label count: ");
//   Serial.println(EI_CLASSIFIER_LABEL_COUNT);
// }

// // === Main Loop: Runs Continuously ===
// void loop() {
//   int16_t ax, ay, az;  // Structures to hold sensor data (acceleration, gyro, temperature)

//   // Check if enough time has passed to take a new sample
//   if (millis() > last_interval_ms + INTERVAL_MS) {
//     last_interval_ms = millis();  // Update timestamp

//     mpu.getAcceleration(&ax, &ay, &az);  // Read values from the sensor

//     // Store acceleration data into features buffer
//     features[feature_ix++] = ax;
//     features[feature_ix++] = ay;
//     features[feature_ix++] = az;

//     // If buffer is full, it's time to classify the motion
//     if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
//       Serial.println("Running the inference...");

//       signal_t signal;               // Structure for the input signal to the model
//       ei_impulse_result_t result;   // Structure for the classification result

//       // Convert features buffer to signal format
//       int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
//       if (err != 0) {
//         ei_printf("Failed to create signal from buffer (%d)\n", err);
//         return;  // Abort inference if signal couldn't be created
//       }

//       // Run the classifier with the prepared signal
//       EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);
//       if (res != 0) return;  // Abort if inference fails

//       // Display timing and prediction results
//       ei_printf("Predictions ");
//       ei_printf("(DSP: %d ms., Classification: %d ms.)",
//                 result.timing.dsp, result.timing.classification);
//       ei_printf(": \n");

//       // Print the score for each possible class label
//       for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
//         ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);

//         // (Optional) Store results in a char buffer if needed
//         char buffer[50];
//         sprintf(buffer, "    %s: %.5f", result.classification[ix].label, result.classification[ix].value);
//         // You could send this buffer over WiFi or log it if needed
//       }

//     String newLabel = "";
//     float highestProbability = 0.0;

//     // Analyze the inference results to find the most probable label
//     for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
//         if (result.classification[i].value > highestProbability) {
//             highestProbability = result.classification[i].value;
//             newLabel = String(result.classification[i].label);
//         }
//     }

//     // Check if the new label is the same as the last label
//     if (newLabel == lastLabel) {
//         if (millis() - labelStartTime > thresholdTime) {
//             currentLabel = newLabel;
//         }
//     } else {
//         lastLabel = newLabel;
//         labelStartTime = millis();
//     }
//     String label = newLabel;
//     label.toUpperCase();    // Display the current label on the OLED if it has been consistent for more than 2 seconds
//     display.fillScreen(ST77XX_BLACK); 

//     display.setCursor(0, 0);       
//     display.setTextSize(4);        
//     display.setTextColor(ST77XX_WHITE); 
//     display.println(label);       

//     display.setCursor(0, 50);      
//     display.setTextSize(3);        
//     display.print("Confidence:"); 
//     display.print((highestProbability) * 100); 
//     display.println("%");   

//     // Debug: Print the current label and the status
//     Serial.print("Current Label: ");
//     Serial.print(currentLabel);
//     Serial.print(" | Most probable: ");
//     Serial.println(newLabel);

//       feature_ix = 0;  // Reset the buffer index for the next cycle
//     }
//   }
// }

// // === Helper Function to Print Formatted Output ===
// void ei_printf(const char* format, ...) {
//   static char print_buf[1024] = { 0 };  // Buffer for formatted output

//   va_list args;
//   va_start(args, format);  // Start capturing variable arguments
//   int r = vsnprintf(print_buf, sizeof(print_buf), format, args);  // Format the string
//   va_end(args);

//   if (r > 0) {
//     Serial.write(print_buf);  // Send output to the Serial Monitor
//   }
// }

