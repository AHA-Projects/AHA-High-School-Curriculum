// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



// This program is designed to detect water leaks by detecting red color strips, using a machine learning model.
// It uses a light sensor to analyze the color of a surface and an OLED screen to display if a leak is detected.

// Edge Impulse Library : Need to import this as a ZIP after downloading from Edge Impulse/Github
#include <Water_Leak_Detection_inferencing.h>


//Default Installed Libraries - No Installation Required
#include <Wire.h> // - Required for communication 
#include <SPI.h> // - Required for SPI communication for the screen


//Sensor Specific Libraries - Installation Required from IDE library manager
#include "Adafruit_APDS9960.h" // - The specific library for our light sensor
#include <Adafruit_GFX.h> // - Core graphics library for shapes and text
#include <Adafruit_ST7789.h> // -  Library for our ST7789 TFT screen

  

// We are setting up how often the sensor will take a reading.
#define FREQUENCY_HZ 43                          // Sampling rate: 43 times per second
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))  // Time between samples in milliseconds

// Creating an object to control the APDS9960 sensor.
Adafruit_APDS9960 apds;

// Defining the physical pins on the microcontroller that are connected to the OLED screen.
#define TFT_CS   33  // Chip Select control pin
#define TFT_DC    25  // Data/Command select pin
#define TFT_RST   26  // Reset pin (or connect to RST, see below)


// Creating an object to control the ST7789 display, using the pins defined above.
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);


// This array will temporarily store the color data collected from the sensor before it is sent to the model.
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];  
// This variable keeps track of the current position in the `features` array.
size_t feature_ix = 0;                               
// This variable stores the last time we took a sample.
static unsigned long last_interval_ms = 0;    
// This variable stores the time when a new label was first detected.
unsigned long labelStartTime = 0;
// These strings will hold the current and previous classification results (e.g., "Leak", "No Leak").
String currentLabel = "";
String lastLabel = "";
// This is the duration (in milliseconds) that a classification result must be stable before it is displayed.
const unsigned long thresholdTime = 1000; 



// The `setup` function runs a single time when the microcontroller starts up.
void setup()
{
  // Start serial communication at a speed of 115200 bits per second, so we can see messages on a computer.
  Serial.begin(115200);
  // Initialize the display with its specific dimensions.
  display.init(170, 320);
  // Rotate the display for the correct viewing orientation.
  display.setRotation(3);  

  // Check if the light sensor is connected properly.
  if(!apds.begin()){
    // If it fails, print an error message.
    Serial.println("failed to initialize device! Please check your wiring.");
  }
  else Serial.println("Device initialized!");
  // Tell the sensor to start detecting color.
  apds.enableColor(true);
}



// The `loop` function runs continuously after the `setup` function is complete.
void loop()
{
  // These variables will store the raw Red, Green, Blue, and a 'Clear' (overall brightness) value from the sensor.
  uint16_t r, g, b, c;

  // This checks if enough time has passed since the last sample was taken.
  if (millis() > last_interval_ms + INTERVAL_MS) {
    last_interval_ms = millis();  // Update the time of the last sample.

  // Get the color data from the sensor.
  apds.getColorData(&r, &g, &b, &c);

  // This is a safety check to prevent a mathematical error (division by zero).
  if (c == 0) {
    return; // If the brightness is zero, skip this cycle.
  }

  // Normalize the RGB values. This converts the raw readings into a ratio (0.0 to 1.0)
  // which makes them easier for the machine learning model to use.
  float r_norm = (float)r / c;
  float g_norm = (float)g / c;
  float b_norm = (float)b / c;

    // Add the normalized color values to our `features` array.
    features[feature_ix++] = r_norm;
    features[feature_ix++] = g_norm;
    features[feature_ix++] = b_norm;

    // Check if the `features` array is full.
    if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      Serial.println("Running the inference...");

      // These are data structures needed by the machine learning model.
      signal_t signal;               
      ei_impulse_result_t result;   

      // Convert our `features` array into a "signal" format that the model can process.
      int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
      if (err != 0) {
        ei_printf("Failed to create signal from buffer (%d)\n", err);
        return;  // Stop if there's an error.
      }

      // Run the machine learning model on the signal.
      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, true);
      if (res != 0) return;  // Stop if there's an error.

      // Print the time it took for the model to process the data.
      ei_printf("Predictions ");
      ei_printf("(DSP: %d ms., Classification: %d ms.)",
                result.timing.dsp, result.timing.classification);
      ei_printf(": \n");

      // Loop through each possible classification and print its confidence score.
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);

        // (Optional) This part shows how you could store the results in a text buffer.
        char buffer[50];
        sprintf(buffer, "    %s: %.5f", result.classification[ix].label, result.classification[ix].value);
      }

        // Find the label (e.g., "Leak", "No Leak") with the highest confidence.
        String newLabel = "No Red";
        float highestProbability = 0.0;

        for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            float conf = result.classification[i].value;
            // Only consider the label if its confidence is greater than 60%.
            if (conf > 0.6) {
                newLabel = String(result.classification[i].label);
                highestProbability = conf;
                break;  
            }
        }

    // This section implements a "stability check". We only want to display a result if it's consistent.
    // Check if the new label is the same as the last one.
    if (newLabel == lastLabel) {
        // If it's the same, and it has been for more than `thresholdTime`, then we update our `currentLabel`.
        if (millis() - labelStartTime > thresholdTime) {
            currentLabel = newLabel;
        }
    } else {
        // If the label changes, we reset the timer and store the new label as the `lastLabel`.
        lastLabel = newLabel;
        labelStartTime = millis();
    }

    String mappedLabel;
    uint16_t textColor;

    // If the model predicts the color is "Red,"
    // we assume this means the sensor has detected our water-sensitive red strip.
    // Therefore, we "map" the color "Red" to the message "Water Leak Detected."    
    if (newLabel == "Red") {
        mappedLabel = "Water Leaking";
        textColor = ST77XX_RED;
    } else {
        mappedLabel = "No Water Leak";
        textColor = ST77XX_GREEN;
    }

    // Display the stable label on the OLED screen.
    String label = mappedLabel;   
    // Clear the screen to draw the new information.
    display.fillScreen(ST77XX_BLACK); 

    // Set the cursor position, text size, and color for the main label.
    display.setCursor(0, 0);       
    display.setTextSize(4);        
    display.setTextColor(textColor); 
    // Print the most probable label.
    display.println(label);       

    // Set the cursor and text properties for the confidence score.
    display.setCursor(0, 50);      
    display.setTextSize(3);        
    display.print("Confidence:"); 
    // Print the confidence score as a percentage.
    display.print((highestProbability) * 100); 
    display.println("%");   

    // Print debugging information to the Serial Monitor.
    Serial.print("Current Label: ");
    Serial.print(currentLabel);
    Serial.print(" | Most probable: ");
    Serial.println(newLabel);

      // Reset the buffer index to start collecting a new set of data for the next classification.
      feature_ix = 0;  
    }
  }
}


// This function is a utility that allows us to print formatted text to the Serial Monitor.
void ei_printf(const char* format, ...) {
  static char print_buf[1024] = { 0 };  // Create a buffer for the formatted text.

  va_list args;
  va_start(args, format);  // Begin processing the variable arguments.
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);  // Format the string.
  va_end(args);

  if (r > 0) {
    Serial.write(print_buf);  // Write the formatted string to the serial port.
  }
}
