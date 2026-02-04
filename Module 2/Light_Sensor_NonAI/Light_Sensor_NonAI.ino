
// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.




// Importing Wire.h, it helps in communicating with I2C devices
// What is I2C?
// I@C is a communication protocol that allows a microcontroller to communicate with multiple peripherals, such as sensors or displays.
#include <Wire.h>

// Importing Sensor Libraries - Light Sensor
#include "Adafruit_APDS9960.h"
// --- Libraries for the TFT Display ---
#include <Adafruit_GFX.h>     // Core graphics library - helps draw shapes and text
#include <Adafruit_ST7789.h>  // Library for our ST7789 TFT screen
// These lines tell the Arduino which pins are connected to the screen.
// IMPORTANT: These pin numbers might be different for your specific Arduino board or wiring.
// These are common for ESP32 boards.
#define TFT_CS    33  // TFT Chip Select pin
#define TFT_DC    25  // TFT Data/Command pin
#define TFT_RST   26  // TFT Reset pin (can be -1 if not used and tied to Arduino RST)
// Instantiating our sensor object
Adafruit_APDS9960 apds;

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // Creates the screen object


// --- Color Definitions for TFT Display ---
// Easy names for colors the screen understands.
#define BLACK     0x0000  // No color
#define WHITE     0xFFFF  // All colors
#define RED       0xF800  // Red color for errors or temperature
#define ORANGE    0xFD20  // An orange color, good for temperature
#define CYAN      0x07FF  // A cyan (light blue) color, good for humidity

// Initializing all the sensor outputs variables
uint16_t ambient_light = 0;
uint16_t red_light = 0;
uint16_t green_light = 0;
uint16_t blue_light = 0;

 
void setup() {
  
  // Start the Serial Monitor (for printing results on the computer)
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("--------------------------------"));
  Serial.println(F("AdaFruit APDS-9960 - LightSensor"));
  Serial.println(F("--------------------------------"));
  

  // Start the light sensor

  if(!apds.begin()){
    Serial.println("failed to initialize device! Please check your wiring.");
  }
  else Serial.println("Device initialized!");

  // Turn on the color sensing mode
  apds.enableColor(true);

  tft.init(170, 320);     // Tell the screen to get ready.
  Serial.println(F("TFT Initialized"));
  tft.setRotation(3);     // Rotate screen (often 3 for landscape: 320 wide, 170 tall).
  tft.fillScreen(BLACK);  // Make the whole screen black.
  
  // These labels will stay on the screen.
    tft.setTextSize(3);             // Set a medium-large text size for labels.
    
    // Label for Red Channel
    tft.setCursor(10, 20);
    tft.setTextColor(RED);          
    tft.print(F("RED:"));
    // Label for Blue Channel
    tft.setCursor(10, 70);
    tft.setTextColor(ST77XX_GREEN);           
    tft.print(F("GREEN:"));
    // Label for Blue Channel
    tft.setCursor(10, 120);
    tft.setTextColor(CYAN);          
    tft.print(F("BLUE:"));
  delay(500);
}

void loop() {

  uint16_t red_light, green_light, blue_light, ambientLight;
  
  // Wait until the sensor has color data ready
  while(!apds.colorDataReady()){
    delay(5);
  }

  // Read color data from the sensor
  apds.getColorData(&red_light, &green_light, &blue_light, &ambientLight);
  
  // Print the values

  Serial.print("Ambient: ");
  Serial.print(ambientLight);

  Serial.print(" Red: ");
  float R = (255.*red_light/ambientLight);
  Serial.print(R);
  
  Serial.print(" Green: ");
  float G = (255.*green_light/ambientLight);
  Serial.print(G);
  
  Serial.print(" Blue: ");
  float B = (255.*blue_light/ambientLight);
  Serial.println(B);


  // --- Display Red Value on TFT ---
  // Clear the area where the old value was
  tft.fillRect(120, 10, 190, 40, BLACK); 
  tft.setTextSize(4);                    // Use a large text size for the value
  tft.setTextColor(RED);                 // Red color for the red value
  tft.setCursor(120, 15);                // Set cursor position (right of "RED:")
  tft.print(R);                  // Print the raw red value

  // --- Display Green Value on TFT ---
  tft.fillRect(140, 60, 170, 40, BLACK);
  tft.setTextSize(4);
  tft.setTextColor(ST77XX_GREEN);               // Green color for the green value
  tft.setCursor(140, 65);                // Set cursor position (right of "GREEN:")
  tft.print(G);                // Print the raw green value

  // --- Display Blue Value on TFT ---
  tft.fillRect(120, 110, 190, 40, BLACK);
  tft.setTextSize(4);
  tft.setTextColor(CYAN);                // Blue color for the blue value
  tft.setCursor(120, 115);               // Set cursor position (right of "BLUE:")
  tft.print(B);                 // Print the raw blue value
  
  
  // Wait 1 second before next reading
  delay(1000);
  

}
