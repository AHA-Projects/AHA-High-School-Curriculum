

// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373.
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



// This program classifies the ripeness of fruit using a machine learning model.
// It uses a light sensor to measure the color of the fruit and an OLED screen to display the result.

// Importing Edge Impulse Library
// This library contains the pre-trained machine learning model for fruit ripeness classification.
// #include <Fruit_Ripeness_Classfication_inferencing.h>
#include <Fruit_Ripeness-v2_inferencing.h>

// Importing Sensor specific library
// This library allows us to communicate with the APDS9960 sensor, which can detect color.
#include "Adafruit_APDS9960.h"

// Importing OLED librarries
// These libraries are used to control the OLED screen, which displays the results.
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Library for I2C communication, which is a way for devices to talk to each other.
#include <Wire.h>
// Library for Wi-Fi communication, not used in this specific program but included in the original.
#include <WiFi.h>


// We define how often we want to take a measurement from the sensor.
#define FREQUENCY_HZ 43                          // Sampling rate: 43 times per second
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))  // Time between samples in milliseconds

// Importing Light Sensor Library
// This creates an object to represent our light sensor.
Adafruit_APDS9960 apds;


// Defining OLED pins for I2C communication
// These lines tell the microcontroller which physical pins are connected to the OLED screen.
#define TFT_CS 33   // Chip Select control pin
#define TFT_DC 25   // Data/Command select pin
#define TFT_RST 26  // Reset pin (or connect to RST, see below)


// Instantiating Sensor Objects
// This line creates an object for the OLED display, using the pins we defined earlier.
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);



// Defining variables for model inference and processing

// This array will hold the color data we collect from the sensor.
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
// This is a counter to keep track of where we are in the `features` array.
size_t feature_ix = 0;
// This variable stores the time of the last sample.
static unsigned long last_interval_ms = 0;
// This variable stores the time when the current label was first detected.
unsigned long labelStartTime = 0;
// These variables store the classification results (e.g., "Ripe", "Unripe").
String currentLabel = "";
String lastLabel = "";
// This is the amount of time a label must be consistent before we display it.
const unsigned long thresholdTime = 1000;



// The `setup` function runs once when the program starts.
void setup() {
  // Start the serial communication so we can see messages on the computer.
  Serial.begin(115200);
  // Initialize the display with its dimensions.
  display.init(170, 320);
  // Rotate the screen to the correct orientation.
  display.setRotation(3);

  // Check if the sensor is connected and working.
  if (!apds.begin()) {
    Serial.println("failed to initialize device! Please check your wiring.");
  } else Serial.println("Device initialized!");
  // Enable the color detection feature of the sensor.
  apds.enableColor(true);
}



// The `loop` function runs over and over again.
void loop() {
  // These variables will hold the raw color data from the sensor (Red, Green, Blue, and a clear value).
  uint16_t r, g, b, c;

  // Check if enough time has passed to take a new sample.
  if (millis() > last_interval_ms + INTERVAL_MS) {
    last_interval_ms = millis();  // Update the time of the last sample.

    // Read the color data from the sensor.
    apds.getColorData(&r, &g, &b, &c);

    // Avoid division by zero, which would cause an error.
    if (c == 0) {
      return;  // Skip this round if the clear value is zero.
    }

    // Normalize the RGB values. This means we convert them to a scale between 0 and 1,
    // which helps the machine learning model work better.
    float r_norm = (float)r / c;
    float g_norm = (float)g / c;
    float b_norm = (float)b / c;


    // Add the normalized color values to our `features` array.
    features[feature_ix++] = r_norm;
    features[feature_ix++] = g_norm;
    features[feature_ix++] = b_norm;

    // If the `features` array is full, it's time to run the classification model.
    if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      Serial.println("Running the inference...");

      // These are objects needed to run the machine learning model.
      signal_t signal;
      ei_impulse_result_t result;

      // Convert our `features` array into a format the model can understand.
      int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
      if (err != 0) {
        ei_printf("Failed to create signal from buffer (%d)\n", err);
        return;  // Stop if there's an error.
      }

      // Run the machine learning model on the color data.
      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);
      if (res != 0) return;  // Stop if there's an error.

      // Display how long the classification took.
      ei_printf("Predictions ");
      ei_printf("(DSP: %d ms., Classification: %d ms.)",
                result.timing.dsp, result.timing.classification);
      ei_printf(": \n");

      // Print the confidence score for each possible fruit ripeness category.
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);

        // (Optional) Store results in a text buffer if needed.
        char buffer[50];
        sprintf(buffer, "    %s: %.5f", result.classification[ix].label, result.classification[ix].value);
      }


      // Find the most likely fruit ripeness label.
      String newLabel = "No Fruit";
      float highestProbability = 0.0;

      for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        float conf = result.classification[i].value;
        // We only consider a label if the confidence is greater than 60%.
        if (conf > 0.7) {
          newLabel = String(result.classification[i].label);
          highestProbability = conf;
          break;
        }
        else {
          newLabel = "No Fruit";
        }
      }


      // This section makes sure the result is stable before we display it.
      // It checks if the new label is the same as the previous one.
      if (newLabel == lastLabel) {
        // If it's the same, we check if it's been consistent for `thresholdTime`.
        if (millis() - labelStartTime > thresholdTime) {
          currentLabel = newLabel;
        }
      } else {
        // If the label changes, we reset the timer.
        lastLabel = newLabel;
        labelStartTime = millis();
      }

      // MAPPER AND COLOR LOGIC
      String mappedLabel;
      uint16_t textColor;  // Variable to hold the color value for the text

    // If the model predicts the color is "Orange,"
    // we assume this means the sensor has detected our ripened orange fruit.
    // Therefore, we "map" the color "Orange" to the message "Ripened Fruit and same with other labels (Green -> UnderRipe Fruit,  Brown -> OverRipe Fruit)"   
      if (newLabel == "Orange") {
        mappedLabel = "Ripened Fruit";
        textColor = ST77XX_ORANGE;  // Set text color to Orange
      } else if (newLabel == "Green") {
        mappedLabel = "UnderRipe Fruit";
        textColor = ST77XX_GREEN;  // Set text color to Green
      } else if (newLabel == "Brown") {
        mappedLabel = "OverRipe Fruit";
        textColor = 0x4000;  // Custom hex code for Brown
      } else {
        mappedLabel = "No Fruit";
        textColor = ST77XX_WHITE;  // Default text color to White
      }

      // Clear the screen to a consistent background color (e.g., black).
      display.fillScreen(ST77XX_BLACK);

      // Set the position and size for the main label text.
      display.setCursor(0, 0);
      display.setTextSize(3);
      // Use the determined color for the text.
      display.setTextColor(textColor);
      display.println(mappedLabel);


      // Set the position and size for the confidence text.
      display.setCursor(0, 50);
      display.setTextSize(2);
      display.print("Confidence:");
      // Print the confidence score as a percentage.
      display.print((highestProbability)*100);
      display.println("%");

      // Debugging information to print to the Serial Monitor.
      Serial.print("Current Label: ");
      Serial.print(currentLabel);
      Serial.print(" | Most probable: ");
      Serial.println(newLabel);

      // Reset the counter to start collecting a new set of data.
      feature_ix = 0;
    }
  }
}




// This function helps us print messages to the Serial Monitor in a nice way.
void ei_printf(const char* format, ...) {
  static char print_buf[1024] = { 0 };  // Buffer for formatted output

  va_list args;
  va_start(args, format);                                         // Start capturing variable arguments
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);  // Format the string
  va_end(args);

  if (r > 0) {
    Serial.write(print_buf);  // Send the formatted output to the Serial Monitor.
  }
}