/*
  RPM meter
  Large text and
  Bar style graphical meter
  Reversed direction of bar
  Dim on parker lights
  Outputs RPM as PWM for shift light
  Uses pulseIn , no interupts
  Offloaded sounds to external Leonardo Tiny
*/


/*
  UTFT Libraries and fonts
  Copyright (C)2015 Rinky-Dink Electronics, Henning Karlsen. All right reserved
  web: http://www.RinkyDinkElectronics.com/
*/


#define Version "RPM Bar V16"


//========================================================================
//========================== Set These Manually ==========================
//========================================================================

int RPM_redline = 8000;
int RPM_yellowline = 6000;
int cylinders = 4;
int minimum_RPM = 500;

// Set whether digitial inputs are active low or active high
// example: active low = pulled to ground for a valid button press
const bool Digitial_Input_Active = LOW;

// Kludge factor to allow for differing
// crystals and similar inconsistancies
// this is applied to Frequency in the RPM calcuation
float Kludge_Factor = 0.994;

//========================================================================



//========================================================================
//========================== Calibration mode ============================
//========================================================================

// Demo = true gives random RPM values
// Calibration = true displays some calculated and raw values
bool Calibration_Mode = false;
bool Demo_Mode = false;
bool Debug_Mode = false;

//========================================================================



// Screen and font stuff
// more fonts use more memory
#include <UTFT.h>
// needed for drawing triangles
#include <UTFT_Geometry.h>

UTFT myGLCD(ILI9481, 38, 39, 40, 41);
UTFT_Geometry geo(&myGLCD);

/*
  My display needed the ILI8491 driver changed
  to flip the display
  LCD_Write_DATA(0x4A); <- correct
  LCD_Write_DATA(0x8A); <- was
  Possible values: 0x8A 0x4A 0x2A 0x1A
*/

// Declare which fonts we will be using
//extern uint8_t SmallFont[];
//extern uint8_t BigFont[];
extern uint8_t franklingothic_normal[];
//extern uint8_t SevenSegNumFont[];
extern uint8_t SevenSegmentFull[];
extern uint8_t Grotesk16x32[];
//extern uint8_t Grotesk24x48[];
//extern uint8_t Grotesk32x64[];
//extern uint8_t GroteskBold32x64[];
//extern uint8_t SevenSeg_XXXL_Num[];
extern uint8_t SevenSegment96x144Num[];

#define font0 franklingothic_normal
#define font1 Grotesk16x32
//#define font2 Grotesk24x48
//#define font3 Grotesk32x64
//#define font4 GroteskBold32x64
//#define font7B SevenSeg_XXXL_Num
#define font7L SevenSegment96x144Num
#define font7F SevenSegmentFull


// Set colours used for bright or dim mode
// this display doesnt have a controllable backlight
// so darker text colours are used for dim mode
bool dim_mode = false;
int text_colour1 = VGA_WHITE;      // or VGA_SILVER
int text_colour2 = VGA_SILVER;     // or VGA_GRAY
int block_fill_colour = VGA_GRAY;  // or VGA_BLACK
int startup_time = 8000;           // 8 seconds


// Common pin definitions
#define SD_Select 53

// Pin definitions for digital inputs
#define Oil_Press_Pin 0     // Oil pressure digital input pin
#define Parker_Light_Pin 1  // Parker lights digital input pin
#define Low_Beam_Pin 2      // Low beam digital input pin
#define High_Beam_Pin 3     // High beam digital input pin
#define Pbrake_Input_Pin 4  // Park brake input pin
#define VSS_Input_Pin 5     // Speed frequency input pin
#define RPM_Input_Pin 6     // RPM frequency INPUT pin
#define Button_Pin 7        // Button momentary input
#define RPM_PWM_In_Pin 8    // Input PWM signal representing RPM

// Pin definitions for analog inputs
#define Temp_Pin A0          // Temperature analog input pin - OneWire sensor on pin 14
#define Fuel_Pin A1          // Fuel level analog input pin
#define Batt_Volt_Pin A2     // Voltage analog input pin
#define Alternator_Pin A3    // Alternator indicator analog input pin

// Pin definitions for outputs
#define RPM_PWM_Out_Pin 9  // Output of RPM as a PWM signal for shift light
#define LED_Pin 10         // NeoPixel LED pin
#define Warning_Pin 11     // Link to external Leonardo for general warning sounds
#define OP_Warning_Pin 12  // Link to external Leonardo for oil pressure warning sound
#define Relay_Pin 13       // Relay for fan control


// RPM variables
int RPM, peak_RPM, last_RPM, len, RPM_PWM;
unsigned long hightime, lowtime, pulsein_timeout;
float freq, period;
int RPM_x = 50, RPM_y = 75;
int maximum_RPM = RPM_redline * 1.2;

// Peak RPM screen position
int peak_RPM_x = 180, peak_RPM_y = 240;

// Meter variables
int blocks, new_val, num_segs, seg_size, x1, x2, block_colour;
int linearBarX = 480 - 15, linearBarY = 10;  // bar starting position in pixels
const int barLength = 450;                   // length and width in pixels
const int barWidth = 50;
const int seg_value = 250;                       // 250 RPM segments
const int seg_gap = 2;                           // gap between segments in pixels
const int meterMin = 0, meterMax = maximum_RPM;  // bar scale



// ##################################################################################################################################################



void setup() {


  // Improved randomness for testing
  unsigned long seed = 0, count = 32;
  while (--count)
    seed = (seed << 1) | (analogRead(A6) & 1);
  randomSeed(seed);

  // Outputs
  pinMode(RPM_PWM_Out_Pin, OUTPUT);
  //pinMode(Warning_Pin, OUTPUT);
  //digitalWrite(Warning_Pin, HIGH);

  // Digital inputs
  pinMode(Button_Pin, INPUT_PULLUP);
  pinMode(Low_Beam_Pin, INPUT_PULLUP);
  pinMode(RPM_Input_Pin, INPUT_PULLUP);

  // Analog inputs
  //none

  // set some more values for the bar graph
  // these only need to be calculated once
  num_segs = int(0.5 + (float)(meterMax - meterMin) / (float)seg_value);  // calculate number of segments
  seg_size = int(0.5 + (float)barLength / (float)num_segs);               // calculate segment width in pixels
  linearBarX = linearBarX - (barLength - num_segs * seg_size) / 2;        // centre the bar to allow for rounding errors


  // Maximum time for pulseIn to wait in microseconds
  // Use the period of the lowest expected frequency, and double it
  // based on cylinders and minimum_RPM
  pulsein_timeout = 1000000.0 / ((float)minimum_RPM * (float)cylinders / 120.0) * 2.0;


  // Display important startup items
  myGLCD.InitLCD(LANDSCAPE);
  myGLCD.clrScr();
  myGLCD.setColor(VGA_GRAY);
  myGLCD.setBackColor(VGA_BLACK);
  myGLCD.setFont(font0);
  myGLCD.print((char *)Version, CENTER, CENTER);
  delay(2000);

  // Clear the screen and display static items
  myGLCD.clrScr();
  myGLCD.print((char *)"RPM", peak_RPM_x + 180, peak_RPM_y);

  // Draw triangles at predetermined points
  int S1 = linearBarX - int((float)barLength / ((float)meterMax / (float)RPM_yellowline));  // Yellowline mark
  int S2 = linearBarX - int((float)barLength / ((float)meterMax / (float)RPM_redline));     // Redline mark
  myGLCD.setColor(VGA_RED);
  geo.fillTriangle(S1 - 4, linearBarY + barWidth + 8, S1 + 4, linearBarY + barWidth + 8, S1, linearBarY + barWidth + 2);
  geo.fillTriangle(S2 - 4, linearBarY + barWidth + 8, S2 + 4, linearBarY + barWidth + 8, S2, linearBarY + barWidth + 2);

  // Display the redline value in red
  myGLCD.setFont(font7L);
  myGLCD.setColor(VGA_RED);
  myGLCD.setBackColor(VGA_BLACK);
  myGLCD.printNumI(RPM_redline, RPM_x, RPM_y, 4, '0');
  delay(1000);

  // Set calibration mode from long-press button input
  // during startup
  if (digitalRead(Button_Pin) == Digitial_Input_Active) {
    // Allow time for the button pin to settle
    // this assumes some electronic/external debounce
    delay(10);
    while (digitalRead(Button_Pin) == Digitial_Input_Active) {
      // just wait until button released
      myGLCD.setColor(VGA_WHITE);
      myGLCD.setBackColor(VGA_BLACK);
      myGLCD.setFont(font0);
      myGLCD.print((char *)"CAL", LEFT, 80);
      Calibration_Mode = true;
    }
  }

  // cant have both demo mode and calibration mode at once
  if (Calibration_Mode) Demo_Mode = false;


}  // End void setup



// ##################################################################################################################################################
// ##################################################################################################################################################



void loop() {


  // =======================================================
  // Reset peak RPM by button press
  // =======================================================

  if (digitalRead(Button_Pin) == Digitial_Input_Active) {
    // Allow time for the button pin to settle
    // this assumes some electronic/external debounce
    delay(10);
    if (digitalRead(Button_Pin) == Digitial_Input_Active) peak_RPM = 0;
  }


  // =======================================================
  // Dim display when headlights on
  // =======================================================

  if (millis() > startup_time) {
    // Dim mode when headlights are on
    if (digitalRead(Low_Beam_Pin) == Digitial_Input_Active && !dim_mode) {
      dim_mode = true;
      text_colour1 = VGA_SILVER;
      text_colour2 = VGA_GRAY;
      block_fill_colour = VGA_BLACK;
    }

    // Normal colours when headlights are off
    if (digitalRead(Low_Beam_Pin) == !Digitial_Input_Active && dim_mode) {
      dim_mode = false;
      text_colour1 = VGA_WHITE;
      text_colour2 = VGA_SILVER;
      block_fill_colour = VGA_GRAY;
    }
  }


  // =======================================================
  // Get the RPM and display RPM text and graph
  // =======================================================

  if (!Demo_Mode) {
    hightime = pulseIn(RPM_Input_Pin, HIGH, pulsein_timeout);
    lowtime = pulseIn(RPM_Input_Pin, LOW, pulsein_timeout);
    period = hightime + lowtime;
    // prevent overflows or divide by zero
    if (period > 1000) {
      freq = 1000000.0 / (float)period * Kludge_Factor;
    } else {
      freq = 0;
    }
    RPM = round(freq / (float)cylinders * 120.0 );

    if (Calibration_Mode) {
      myGLCD.setColor(VGA_GRAY);
      myGLCD.setBackColor(VGA_BLACK);
      myGLCD.setFont(font0);
      myGLCD.printNumI(period, 0, 220, 4);
      myGLCD.printNumI(freq, 0, 240, 4);
      myGLCD.printNumI(RPM, 0, 260, 4);
    }
  } else {
    // Demo mode, invent values
    RPM = random(600, 8200);
    //RPM = 2000;
  }

  RPM = constrain(RPM, 0, maximum_RPM);
  last_RPM = RPM;


  // =======================================================
  // Output the RPM as a PWM signal for another Arduino
  // =======================================================

  RPM_PWM = map(RPM, RPM_yellowline, RPM_redline, 0, 255);
  analogWrite(RPM_PWM_Out_Pin, RPM_PWM);


  // =======================================================
  // Update the max RPM if it increased
  // =======================================================

  // Do this before we round the RPM to 10's or 100's
  if (RPM > peak_RPM) {
    peak_RPM = RPM;
  }
  myGLCD.setFont(font7F);
  myGLCD.setColor(text_colour2);
  myGLCD.setBackColor(VGA_BLACK);
  myGLCD.printNumI(peak_RPM, peak_RPM_x, peak_RPM_y, 4, ' ');


  // =======================================================
  // Print RPM value
  // =======================================================

  //round RPM to nearest 100's or 10's
  if (RPM > RPM_redline / 2) {
    RPM = round((RPM + 50) / 100) * 100;
  } else {
    RPM = round((RPM + 5) / 10) * 10;
  }

  myGLCD.setFont(font7L);
  if (RPM <= 999) {
    // Print a black digit 8 where a rogue digit would be
    // because this large font only has digits, no space char
    myGLCD.setColor(VGA_BLACK);
    myGLCD.setBackColor(VGA_BLACK);
    myGLCD.printNumI(8, RPM_x, RPM_y, 1, '0');
    // Print the 3 digit RPM
    myGLCD.setColor(text_colour1);
    myGLCD.printNumI(RPM, RPM_x + 96, RPM_y, 3, '0');
  } else {
    // Print the 4 digit RPM
    myGLCD.setColor(text_colour1);
    myGLCD.printNumI(RPM, RPM_x, RPM_y, 4, '0');
  }


  // =======================================================
  // Draw the barmeter
  // =======================================================

  int new_val = map(RPM, meterMin, meterMax, 0, num_segs);

  // Draw colour blocks for the segents
  for (blocks = 0; blocks < num_segs; blocks++) {
    // Calculate pair of coordinates for segment start and end
    x1 = linearBarX - (blocks * seg_size);                  // starting X coord
    x2 = linearBarX - ((blocks + 1) * seg_size) + seg_gap;  // ending X coord allowing for gap

    if (new_val > 0 && blocks < new_val) {
      if (!dim_mode) {
        // Choose colour from scheme using the Rainbow function
        // uncomment one line
        //block_colour = VGA_RED; // Fixed colour
        //block_colour = VGA_GREEN;  // Fixed colour
        //block_colour = VGA_NAVY; // Fixed colour
        //block_colour = rainbow(map(blocks, 0, num_segs, 0, 127)); // Blue to red
        block_colour = rainbow(map(blocks, 0, num_segs, 63, 127));  // Green to red
        //block_colour = rainbow(map(blocks, 0, num_segs, 127, 63)); // Red to green
        //block_colour = rainbow(map(blocks, 0, num_segs, 127, 0)); // Red to blue
      } else {
        block_colour = text_colour2;
      }

      // Fill in coloured blocks
      myGLCD.setColor(block_colour);
      myGLCD.fillRect(x1, linearBarY, x2, linearBarY + barWidth);
    }
    // Fill in blank segments
    else {
      myGLCD.setColor(block_fill_colour);
      myGLCD.fillRect(x1, linearBarY, x2, linearBarY + barWidth);
    }
  }


}  // End void loop



// ##################################################################################################################################################
// ##################################################################################################################################################



// =======================================================
// Reusable functions
// =======================================================


// =======================================================

// Function to return a 16 bit rainbow colour

unsigned int rainbow(byte value) {
  // Value is expected to be in range 0-127
  // The value is converted to a spectrum colour from 0 = blue through to 127 = red

  byte red = 0;    // Red is the top 5 bits of a 16 bit colour value
  byte green = 0;  // Green is the middle 6 bits
  byte blue = 0;   // Blue is the bottom 5 bits

  byte quadrant = value / 32;

  if (quadrant == 0) {
    blue = 31;
    green = 2 * (value % 32);
    red = 0;
  }
  if (quadrant == 1) {
    blue = 31 - (value % 32);
    green = 63;
    red = 0;
  }
  if (quadrant == 2) {
    blue = 0;
    green = 63;
    red = value % 32;
  }
  if (quadrant == 3) {
    blue = 0;
    green = 63 - 2 * (value % 32);
    red = 31;
  }
  return (red << 11) + (green << 5) + blue;
}

// =======================================================



// ##################################################################################################################################################
// ##################################################################################################################################################
