// Acknowledgments

// Creator: Anany Sharma at the University of Florida working under NSF grant. 2405373

// This material is based upon work supported by the National Science Foundation under Grant No. 2405373. 
// Any opinions, findings, and conclusions or recommendations expressed in this material are those of the authors and do not necessarily reflect the views of the National Science Foundation.



// These are the 'brains' (libraries) we need for our sensor and the screen.


#include "Adafruit_APDS9960.h" // The library for our gesture sensor
#include <Adafruit_GFX.h>      // Core graphics library - helps draw shapes and text
#include <Adafruit_ST7789.h>   // Library for the ST7789 TFT screen




// --- Screen Pin Definitions ---
#define TFT_CS    33  // TFT Chip Select pin: Tells the screen when we're sending it data.
#define TFT_DC    25  // TFT Data/Command pin: Tells the screen if we're sending a command or actual data to display.
#define TFT_RST   26  // TFT Reset pin: Can be used to reset the screen if something goes wrong. (Sometimes connected to the Arduino's reset pin).





// --- Color Definitions ---
#define BLACK   0x0000  // 
#define WHITE   0xFFFF  
#define GREEN   0x07E0  
#define RED     0xF800

// --- Create our sensor and screen objects ---
Adafruit_APDS9960 apds; // This is our gesture sensor object.
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); // This is our screen object. We give it the pin numbers we defined.



// This variable will remember the last gesture we showed on the screen.
uint8_t last_gesture_shown = 0;




// The setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200); // Starts talking to your computer so you can see messages (for debugging).



  // --- Initialize the Screen ---
  // For a 1.9" 170x320 TFT screen. If your screen is different, these numbers might change.
  tft.init(170, 320);     // Tell the screen to get ready, specifying its pixel width and height.
  Serial.println("TFT initialized"); // Message for computer: screen is ready.
  tft.setRotation(3);     // Rotate the screen. '3' often means landscape mode (320 wide, 170 tall).
  tft.fillScreen(BLACK);  // Make the whole screen black to start with.



  // --- Display Initial Instructions on the Screen ---
  tft.setTextSize(2);             // Set text size to medium.
  tft.setTextColor(WHITE);        // Set text color to white.
  tft.setCursor(5, 5);            // Place the 'pen' (cursor) near the top-left. X=5, Y=5.
  tft.println("1. Wave hand CLOSE to sensor"); // First instruction line.
  tft.setCursor(5, 25);           // Move cursor down for the next line.
  tft.println("   to activate gesture mode.");
  


  // --- Initialize the Gesture Sensor ---
  if (!apds.begin()) {
    Serial.println("Failed to initialize device! Please check your wiring.");
    // Also show error on TFT if possible
    tft.fillRect(0, 85, tft.width(), 60, RED); // Red box for error
    tft.setCursor(10, 90);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.println("Sensor Error!");
    tft.println("Check Wiring.");
    while (1); // Stop here if sensor fails
  } else {
    Serial.println("Device initialized!");
  }



  // Gesture mode will be entered once proximity mode senses something close.
  apds.enableProximity(true); // Turn on the 'nearness' sensor.
  apds.enableGesture(true);   // Turn on the gesture sensing magic!
}



void loop() {
  // Try to read a gesture from the sensor.
  // 'apds.readGesture()' will return a number that tells us the gesture.
  uint8_t gesture = apds.readGesture();

    if (gesture == 0) {
    return; // Nothing new to show, so skip the rest.
  }

  // --- A New Gesture is Detected! ---
  // Remember this new gesture as the last one we've shown.
  last_gesture_shown = gesture;

  // Let's display the gesture nice and big!

  int gesture_display_y_start = 90;  // Y-coordinate where this area starts (below instructions).
  int gesture_display_height = 70;   // How tall this display area is.

  // Clear this part of the screen by drawing a black rectangle over it.
  tft.fillRect(0, gesture_display_y_start, tft.width(), gesture_display_height, BLACK);

  // Set up how our gesture symbol will look.
  tft.setTextSize(4);            // Make the text (our symbol) really big!
  tft.setTextColor(GREEN);       // Let's use a fun green color.

  int16_t symbol_width = 42;
  int16_t symbol_height = 56;
  // Calculate X position to be in the middle of the screen width.
  int16_t x_position = (tft.width() - symbol_width) / 2;
  // Calculate Y position to be in the middle of our 'gesture_display_area'.
  int16_t y_position = gesture_display_y_start + (gesture_display_height - symbol_height) / 2;
  tft.setCursor(x_position, y_position); 


  if (gesture == APDS9960_UP) {
    Serial.println("UP");
    tft.print("UP");     
  } else if (gesture == APDS9960_DOWN) {
    Serial.println("DOWN");
    tft.print("DOWN");     
  } else if (gesture == APDS9960_LEFT) {
    Serial.println("LEFT");
    tft.print("LEFT");    
  } else if (gesture == APDS9960_RIGHT) {
    Serial.println("RIGHT");
    tft.print("RIGHT");    
  }

  delay(50); // A small pause. This helps make sure we don't try to read gestures too fast.
}