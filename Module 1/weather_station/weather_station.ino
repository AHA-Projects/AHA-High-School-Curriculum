// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



// --- Original Libraries for BME280 Sensor ---
#include <Wire.h>             // Needed for I2C communication (how the sensor talks to Arduino)
#include <SPI.h>              // Needed if you were using SPI communication (another way to talk)
#include <Adafruit_Sensor.h>  // Adafruit's basic sensor library
#include <Adafruit_BME280.h>  // The specific library for our BME280 sensor

// --- Libraries for the TFT Display ---
#include <Adafruit_GFX.h>     // Core graphics library - helps draw shapes and text
#include <Adafruit_ST7789.h>  // Library for our ST7789 TFT screen

// --- Screen Pin Definitions (for ST7789 TFT Display) ---
// These lines tell the Arduino which pins are connected to the screen.
// IMPORTANT: These pin numbers might be different for your specific Arduino board or wiring.
// These are common for ESP32 boards.
#define TFT_CS    33  // TFT Chip Select pin
#define TFT_DC    25  // TFT Data/Command pin
#define TFT_RST   26  // TFT Reset pin (can be -1 if not used and tied to Arduino RST)

// --- Constants ---
#define SEALEVELPRESSURE_HPA (1013.25) // Standard atmospheric pressure at sea level, for altitude calculation

// --- Color Definitions for TFT Display ---
// Easy names for colors the screen understands.
#define BLACK     0x0000  // No color
#define WHITE     0xFFFF  // All colors
#define RED       0xF800  // Red color for errors or temperature
#define ORANGE    0xFD20  // An orange color, good for temperature
#define CYAN      0x07FF  // A cyan (light blue) color, good for humidity

// --- Create our sensor and screen objects ---
Adafruit_BME280 bme; // Creates the BME280 sensor object using I2C communication (default)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // Creates the screen object

unsigned long delayTime; // How long to wait between readings (in milliseconds)

void setup() {
    Serial.begin(9600);    // Starts talking to your computer's Serial Monitor.
    while (!Serial && millis() < 3000) ; // Wait a bit for Serial to connect (especially for some Arduinos).
                                      // The millis() < 3000 is a timeout so it doesn't hang forever.
    Serial.println(F("BME280 Test with TFT Display")); // F() saves memory on some Arduinos.

    // --- Initialize the TFT Display ---
    // For a 1.9" 170x320 TFT. If your screen is different, these numbers might change.
    tft.init(170, 320);     // Tell the screen to get ready.
    Serial.println(F("TFT Initialized"));
    tft.setRotation(3);     // Rotate screen (often 3 for landscape: 320 wide, 170 tall).
    tft.fillScreen(BLACK);  // Make the whole screen black.

    // --- Initialize the BME280 Sensor ---
    bool status;
    status = bme.begin(0x76); // Try to start the sensor
    
    if (!status) {
        Serial.println(F("Could not find a valid BME280 sensor, check wiring, address, sensor ID!"));
        Serial.print(F("SensorID was: 0x")); Serial.println(bme.sensorID(), 16);
        Serial.print(F("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n"));
        Serial.print(F("   ID of 0x56-0x58 represents a BMP 280,\n"));
        Serial.print(F("        ID of 0x60 represents a BME 280.\n"));
        Serial.print(F("        ID of 0x61 represents a BME 680.\n"));

        // Show error on TFT screen too
        tft.setTextSize(2);
        tft.setTextColor(RED);
        tft.setCursor(10, 10);
        tft.println(F("BME280 Sensor Error!"));
        tft.setCursor(10, 35);
        tft.println(F("Check wiring & I2C Addr."));
        tft.setCursor(10, 60);
        tft.println(F("Halting."));
        
        while (1) delay(10); // Stop everything if sensor not found.
    }
    
    Serial.println(F("-- Weather Station --"));
    delayTime = 1000; // Set delay between readings to 1 second (1000 milliseconds).

    // --- Draw Static Labels on TFT ---
    // These labels will stay on the screen.
    tft.setTextSize(3);             // Set a medium-large text size for labels.
    tft.setTextColor(WHITE);        // Use white color for labels.
    
    // Label for Temperature
    tft.setCursor(10, 30);          // Position for "TEMP:" text (x=10, y=30)
    tft.print(F("TEMP:"));
    
    // Label for Humidity
    tft.setCursor(10, 100);         // Position for "HUMI:" text (x=10, y=100)
    tft.print(F("HUMI:"));

    Serial.println();
}


void loop() { 
    // --- Read Sensor Values ---
    float temperature = bme.readTemperature();      // Get temperature in Celsius
    float humidity = bme.readHumidity();            // Get humidity in %
    float pressure = bme.readPressure() / 100.0F;   // Get pressure in hPa
    float altitude = bme.readAltitude(SEALEVELPRESSURE_HPA); // Get altitude in meters

    // --- Print All Values to Serial Monitor for debugging ---
    Serial.print(F("Temperature = "));
    Serial.print(temperature);
    Serial.println(F(" °C"));

    Serial.print(F("Pressure = "));
    Serial.print(pressure);
    Serial.println(F(" hPa"));

    Serial.print(F("Approx. Altitude = "));
    Serial.print(altitude);
    Serial.println(F(" m"));

    Serial.print(F("Humidity = "));
    Serial.print(humidity);
    Serial.println(F(" %"));
    Serial.println();

    // --- Display Temperature on TFT ---
    // Define area to clear for the temperature value to prevent old text overlap
    // x, y, width, height, color
    tft.fillRect(130, 20, 180, 40, BLACK); // Clear previous temp value
    tft.setTextSize(4);                   // Use a large text size for the value
    tft.setTextColor(ORANGE);             // Orange color for temperature
    tft.setCursor(130, 25);               // Set cursor position for the value (right of "TEMP:")
    tft.print(temperature, 1);            // Print temperature with 1 decimal place
    tft.write(247);                       // This prints the degree symbol: °
    tft.print(F("C"));                    // Print "C" for Celsius

    // --- Display Humidity on TFT ---
    // Define area to clear for the humidity value
    tft.fillRect(130, 90, 180, 40, BLACK); // Clear previous humi value
    tft.setTextSize(4);                   // Large text size for the value
    tft.setTextColor(CYAN);               // Cyan (light blue) color for humidity
    tft.setCursor(130, 95);               // Set cursor position for the value (right of "HUMI:")
    tft.print(humidity, 1);               // Print humidity with 1 decimal place
    tft.print(F(" %"));                   // Print "%" symbol

    delay(delayTime); // Wait for 'delayTime' (1 second) before the next reading.
}
