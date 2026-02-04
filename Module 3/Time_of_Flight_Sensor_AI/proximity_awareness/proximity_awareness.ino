

// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



#include <Proximity_Motion_Detection_inferencing.h>                //Edge impulse trained model library
// #include <GyverOLED.h>


#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789 display

#include <Wire.h>
#include <VL53L0X.h>

VL53L0X sensor;  //Initializing the sensor object for its appropriate usage


// defining variables to store and calculate the heart rate sensed by the sensor.
float distanceCm; 
static unsigned long last_interval_ms = 0;    
unsigned long labelStartTime = 0;
String currentLabel = "";
String lastLabel = "";
const unsigned long thresholdTime = 500; 

// === Define Sampling Parameters ===
#define FREQUENCY_HZ 9                          // Sampling rate: 60 times per second
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))  // Time between samples in milliseconds

#define TFT_CS   33  // Chip Select control pin
#define TFT_DC    25  // Data/Command select pin
#define TFT_RST   26  // Reset pin 

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // Initializing the LED object for its appropriate usage.

float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];  // Buffer to store sampled features
size_t feature_ix = 0;                               // Current index in the features buffer


void setup() {
  Serial.begin(115200);  // Initialize serial communication at 115200 baud for debugging
  display.init(170, 320);  //Intializing and configuring the sensor using the object(tft).
  display.setRotation(3);  


  Wire.begin();

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) {}
  }
  sensor.startContinuous();
  
  // Show Edge Impulse model input size and label count
  Serial.print("Features: ");
  Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  Serial.print("Label count: ");
  Serial.println(EI_CLASSIFIER_LABEL_COUNT);
}

// === Main Loop: Runs Continuously ===
void loop() {

  // Check if enough time has passed to take a new sample
  if (millis() > last_interval_ms + INTERVAL_MS) {
    last_interval_ms = millis();  // Update timestamp
  
  // Calculate the distance
  distanceCm = sensor.readRangeContinuousMillimeters() / 100;

  // Store acceleration data into features buffer
  features[feature_ix++] = distanceCm;


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

        String newLabel = "STATIONED";
        float highestProbability = 0.0;

        for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            float conf = result.classification[i].value;
            if (conf > 0.6) {
                newLabel = String(result.classification[i].label);
                highestProbability = conf;
                break;  
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

    if (distanceCm < 3) {
      newLabel = "TOO CLOSE!";
    }

    // Display the current label on the OLED if it has been consistent for more than 2 seconds
    
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

