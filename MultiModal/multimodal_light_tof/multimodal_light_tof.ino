

#include <Wire.h>
#include <SPI.h>
#include <VL53L0X.h>
#include "Adafruit_APDS9960.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define TFT_CS    33
#define TFT_DC    25
#define TFT_RST   26

#define BLACK   0x0000
#define WHITE   0xFFFF
#define GREEN   0x07E0
#define RED     0xF800
#define YELLOW  0xFFE0
#define BLUE    0x001F

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_APDS9960 apds;
VL53L0X sensor;

// --- State Tracking Variables ---
String lastLabel = "";
float lastDist = -1.0;
int lastC = -1;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  tft.init(170, 320);
  tft.setRotation(3);
  tft.fillScreen(BLACK);
  
  if (!apds.begin()) { Serial.println("APDS9960 Fail"); while(1); }
  
  apds.enableColor(true);

  sensor.setTimeout(500);
  if (!sensor.init()) { Serial.println("VL53L0X Fail"); while(1); }
  sensor.startContinuous();

  // Draw Static UI Header
  tft.fillRect(0, 0, tft.width(), 45, BLUE); // Blue header bar
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 15);
  tft.print("AI MONITOR ACTIVE");
}

void loop() {
  uint16_t r, g, b, c;

  // Wait for color data to be ready
  while (!apds.colorDataReady()) {
    delay(5);
  }

  apds.getColorData(&r, &g, &b, &c);
  uint16_t dist_mm = sensor.readRangeContinuousMillimeters();

  // 2. Logic Classification
  String currentLabel = "IDLE (CLEAR)";
  uint16_t color = WHITE;
  Serial.print("Clear Light Intensity Readings(C):"); Serial.println(c, 4);


  if (dist_mm < 1500 && dist_mm > 20) { 
    if (c < 3) { // NIGHT MODE
      if (dist_mm < 200) {
        currentLabel = "COYOTE ALERT!";
        color = RED;
      } else {
        currentLabel = "INTRUDER DETECTED";
        color = YELLOW;
      }
    } else { // DAY MODE
      currentLabel = "PERSON DETECTED";
      color = GREEN;
    }
  }
  else {
    currentLabel = "CLEAR";
    color = GREEN;
  }

  // 3. Conditional Update (Prevents Flicker)
  float currentDistCm = dist_mm / 10.0;
  
  // Update if Label changed, or if Distance changed by > 2cm
  if (currentLabel != lastLabel || abs(currentDistCm - lastDist) > 2.0) {
    updateDisplay(currentLabel, color, currentDistCm);
    
    // Save current states
    lastLabel = currentLabel;
    lastDist = currentDistCm;
  }
  
  delay(50);
}

void updateDisplay(String label, uint16_t color, float d) {
  // Clear only the dynamic area
  tft.fillRect(0, 50, tft.width(), 120, BLACK);
  
  // Draw Classification Label
  tft.setTextSize(3);
  tft.setTextColor(color);
  tft.setCursor(10, 60);
  tft.print(label);

  // Draw Distance
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(10, 105);
  tft.print("Range: "); 
  if(d > 200) tft.print("CLEAR");
  else 
    { 
      tft.print(d, 1); tft.print(" cm"); 
    }

}