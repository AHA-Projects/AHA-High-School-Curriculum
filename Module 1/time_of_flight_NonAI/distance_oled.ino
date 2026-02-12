// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



// --- Libraries for Sensor and I2C Communication ---
#include <Wire.h>     // For I2C communication (how the sensor talks to Arduino)
#include <VL53L0X.h>  // For the VL53L0X Time-of-Flight distance sensor

// --- Libraries for Adafruit ST7789 TFT Display ---
#include <Adafruit_GFX.h>     // Core graphics library for drawing
#include <Adafruit_ST7789.h>  // Specific library for ST7789 TFT displays
#include <SPI.h>              // Required for ST7789 even if using I2C sensor

// --- TFT Display Pin Definitions ---

#define TFT_CS    33  // Chip Select
#define TFT_DC    25  // Data/Command
#define TFT_RST   26  // Reset (can be -1 if tied to Arduino's RST pin)


// --- Color Definitions for TFT Display (using ST77XX standard color names) ---
// These are just easy names for color numbers the screen understands.
#define COLOR_BACKGROUND ST77XX_BLACK
#define COLOR_TITLE      ST77XX_WHITE
#define COLOR_NORMAL_FG  ST77XX_GREEN  // Foreground color for normal readings
#define COLOR_WARN_FG    ST77XX_YELLOW // Foreground color for warnings (e.g., out of range)
#define COLOR_ERROR_FG   ST77XX_RED    // Foreground color for errors (e.g., timeout)

// --- Create our sensor and display objects ---
VL53L0X sensor; // This is our distance sensor object

// Create an ST7789 TFT display object.

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup()
{
  Serial.begin(9600); // Start talking to the computer (Serial Monitor) for messages.
  Wire.begin();       // Initialize communication (for the VL53L0X sensor).

  // --- Initialize ST7789 TFT Display ---
  tft.init(170, 320);   // Initialize ST7789 screen.
  Serial.println("ST7789 TFT Initialized (or attempted)");
  tft.setRotation(3);     // Rotate the screen. '3' often makes 170x320 display as 320 wide, 170 tall.
  tft.fillScreen(COLOR_BACKGROUND); // Fill the screen with the background color.

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) {}
  }
  // Start continuous back-to-back mode (take readings as fast as possible).
  sensor.startContinuous();

  // Clear "Initializing Sensor..." message and prepare for readings.
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE);
  tft.println("Distance (cm)"); // Main title
}

void loop()
{
  // Read the distance from the sensor in millimeters (mm).
  uint16_t distance_mm = sensor.readRangeContinuousMillimeters();
  
  int value_area_y = 50;
  int value_area_height = 80; 
  tft.fillRect(0, value_area_y, tft.width(), value_area_height, COLOR_BACKGROUND);

  tft.setTextSize(5);
  tft.setCursor(0, value_area_y + (value_area_height / 2) - (40 / 2) ); // Roughly center vertically in the value area.

  String displayText = "";
  uint16_t textColor = COLOR_NORMAL_FG;

  if (distance_mm >= 300) { 
    // The sensor often returns a high value like 8190 if no object is found or it's too far.
    displayText = ">30 cm";
    textColor = COLOR_WARN_FG;
  } else {
    // Convert millimeters to centimeters.
    float distance_cm = distance_mm / 10.0;
    
    // Using dtostrf for more control or simple String conversion.
    char buffer[10]; 
    dtostrf(distance_cm, 4, 1, buffer); // (floatVar, minStringWidthIncDecimal, numVarsAfterDecimal, charBuffer)
    sprintf(buffer, "%.1f", distance_cm); // Simpler way using sprintf to get one decimal place.
    displayText = String(buffer) + " cm";
    textColor = COLOR_NORMAL_FG;
  }

  // Set text color
  tft.setTextColor(textColor);

  // Calculate x position to center the text
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(displayText, tft.getCursorX(), tft.getCursorY(), &x1, &y1, &w, &h); // Get width of current text
  tft.setCursor((tft.width() - w) / 2, tft.getCursorY()); // Center horizontally

  tft.print(displayText);


  delay(5); 
}