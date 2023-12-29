/***************************************************
*                                                  *
*   Arduino Mega2560 based LCD automotive gauge    *
*              for displaying RPM                  *
*   This gauge is used in landscape mode           *
*                                                  *
***************************************************/


// Just in case you are not using the Arduino IDE
//#include <arduino.h>


/*
  RPM meter
  Large text and
  Bar style graphical meter
  Reversed direction of bar
  Dim on parker lights
  Outputs RPM via serial2 for shift light
  Uses pulseIn , no interupts
  Offloaded sounds to external Leonardo Tiny
*/


/*
  UTFT Libraries and fonts
  Copyright (C)2015 Rinky-Dink Electronics, Henning Karlsen. All right reserved
  web: http://www.RinkyDinkElectronics.com/
*/



#define Version "RPM Bar V19"



//========================================================================
//-------------------------- Set These Manually --------------------------
//========================================================================

int RPM_redline   = 8000;
int RPMyellowline = 6000;
int cylinders     = 4;
int minimum_RPM   = 500;

// Set whether digitial inputs are active low or active high
// example: active low = pulled to ground for a valid button press
const bool Digitial_Input_Active = LOW;

// Set high or low for valid warnings to be passed to external processing
const bool Valid_Warning = LOW;

// Kludge factor to allow for differing
// crystals and similar inconsistancies
float Kludge_Factor = 0.994;

// Set these to ensure correct voltage readings of analog inputs
const float vcc_ref = 4.92;      // measure the 5 volts DC and set it here
const float R1      = 1200.0;    // measure and set the voltage divider values
const float R2      = 3300.0;    // for accurate voltage measurements

// The range of RPM on the neopixel strip is dictated by the output from the RPM module
// set the length of the NeoPixel shiftlight strip
int LED_Count = 8;

//========================================================================



//========================================================================
//------------------------------ Demo mode -------------------------------
//========================================================================
// Removed calibration mode, it shouldnt be needed

// Demo = true gives random RPM values
bool Demo_Mode = true;

//========================================================================



// Screen and font stuff
// more fonts use more memory
#include <UTFT.h>
// needed for drawing triangles
#include <UTFT_Geometry.h>

UTFT          myGLCD(ILI9481, 38, 39, 40, 41);
UTFT_Geometry geo(&myGLCD);

/*
  My display needed the ILI8491 driver changed
  to flip the display
  LCD_Write_DATA(0x4A); <- correct
  LCD_Write_DATA(0x8A); <- was
  Possible values: 0x8A 0x4A 0x2A 0x1A
*/

/*
Available colours
  VGA_BLACK	
  VGA_WHITE	
  VGA_RED
  VGA_GREEN	
  VGA_BLUE	
  VGA_SILVER	
  VGA_GRAY	
  VGA_MAROON	
  VGA_YELLOW	
  VGA_OLIVE	
  VGA_LIME	
  VGA_AQUA	
  VGA_TEAL	
  VGA_NAVY	
  VGA_FUCHSIA
  VGA_PURPLE	
  VGA_TRANSPARENT
*/
// orange is missing from the default range
#define VGA_ORANGE 0xFD20 /* 255, 165,   0 */

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
bool dim_mode          = false;
int  text_colour1      = VGA_WHITE;     // or VGA_SILVER
int  text_colour2      = VGA_SILVER;    // or VGA_GRAY
int  block_fill_colour = VGA_GRAY;      // or VGA_BLACK
int  startup_time      = 10000;         // 10 seconds


// Voltage calculations
/*
  Read the Analog Input
  adc_value = analogRead(ANALOG_IN_PIN);

  Determine voltage at ADC input
  adc_voltage  = (adc_value+0.5) * ref_voltage / 1024.0 / (voltage divider)

  Calculate voltage at divider input
  in_voltage = adc_voltage / (R2/(R1+R2));

  R1 = 3300
  R2 = 1200
  vref = default = ~5.0 volts
  Vin = analogRead(ANALOG_IN_PIN) * 5 / 1024 / (3300/(3300+1200))
  Vin = analogRead(ANALOG_IN_PIN) * 0.00665838
  or
  Vin = analogRead(ANALOG_IN_PIN) * Input_Multiplier
*/
const float Input_Multiplier = vcc_ref / 1024.0 / (R2 / (R1 + R2));


// Common pin definitions
const int SD_Select = 53;

// Pin definitions for digital inputs
// Mega2560 Serial2 pins 17(RX), 16(TX)
const int Oil_Press_Pin    = 0;    // Oil pressure digital input pin
const int Parker_Light_Pin = 1;    // Parker lights digital input pin
const int Low_Beam_Pin     = 2;    // Low beam digital input pin
const int High_Beam_Pin    = 3;    // High beam digital input pin
const int Pbrake_Input_Pin = 4;    // Park brake input pin
const int VSS_Input_Pin    = 5;    // Speed frequency input pin
const int RPM_Input_Pin    = 6;    // RPM frequency INPUT pin
const int Button_Pin       = 7;    // Button momentary input

// Pin definitions for analog inputs
const int Temp_Pin       = A0;    // Temperature analog input pin - OneWire sensor on pin 14
const int Fuel_Pin       = A1;    // Fuel level analog input pin
const int Batt_Volt_Pin  = A2;    // Voltage analog input pin
const int Alternator_Pin = A3;    // Alternator indicator analog input pin

// Pin definitions for outputs
// Mega2560 Serial2 pins 17(RX), 16(TX)
const int LED_Pin        = 10;    // NeoPixel LED pin
const int Warning_Pin    = 11;    // Link to external Leonardo for general warning sounds
const int OP_Warning_Pin = 12;    // Link to external Leonardo for oil pressure warning sound
const int Relay_Pin      = 13;    // Relay for fan control

// RPM variables
int           RPM, peakRPM, lastRPM, demoRPM;
unsigned long hightime, lowtime, pulsein_timeout, period_min;
float         freq, period, RPM_f, RPM_constant;
int           maximumRPM = RPM_redline * 1.2;
int           lowRPM     = RPM_redline / 3;
int           RPM_LED_Pos;
int           spinnerState = 1;

// Position on display
//getDisplayXSize
//getDisplayYSize
const int RPMx      = 480 / 2 - 195;    // horizontal or long side
const int RPMy      = 320 / 2 - 50;     // vertical or short side
const int peakRPMx  = 480 / 2 - 65;
const int peakRPMy  = 320 - 50;
const int spinner_x = 10;
const int spinner_y = 320 - 40;

// Meter variables
const int barMargin = 10;
const int barLength = 480 - (2 * barMargin);      // horizontal or long side
const int barWidth  = 320 / 5;                    // vertical or short side
const int meterMin = 0, meterMax = maximumRPM;    // bar scale
int       blankBarValue, colouredBarValue, block_colour;
int       linearBarX = barMargin;
int       linearBarY = barMargin;    // bar starting position in pixels



// ##################################################################################################################################################



void setup()
    {

    // Start a serial port for sending RPM LED shift light position
    // Serial2 17(RX), 16(TX)
    Serial2.begin(57600);

    // Improved randomness for testing
    // Choose an unused analog pin
    unsigned long seed = 0, count = 32;
    while (--count)
        seed = (seed << 1) | (analogRead(A6) & 1);
    randomSeed(seed);

    // Outputs
    //pinMode(RPM_Serial_Out_Pin, OUTPUT);
    pinMode(Warning_Pin, OUTPUT);
    digitalWrite(Warning_Pin, !Valid_Warning);

    // Digital inputs
    // remove input_pullup's after testing
    // since pullups are handled by external hardware
    pinMode(Button_Pin, INPUT_PULLUP);
    pinMode(Low_Beam_Pin, INPUT_PULLUP);
    pinMode(RPM_Input_Pin, INPUT);

    // Analog inputs
    // none

    // =======================================================
    // Maximum time for pulseIn to wait in microseconds
    // Use the period of the lowest expected frequency
    // based on cylinders and minimum_RPM, and double it
    pulsein_timeout = 1000000.0 / ((float)minimum_RPM * (float)cylinders / 120.0) * 2.0;
    // =======================================================

    // =======================================================
    // Calculate the minimum period in microseconds
    // values beyond this would be considered noise or an error
    // this helps to prevent divide by zero errors
    // based on cylinders and maximumRPM, and halve it
    period_min = 1000000.0 / ((float)maximumRPM * (float)cylinders / 120.0) / 2.0;
    // =======================================================

    // =======================================================
    // Calculate this constant up front to avoid
    // doing so many divides every loop
    RPM_constant = 120000000 / (float)cylinders * Kludge_Factor;
    //based on:
    //freq = 1000000.0 / (float)period;
    //RPM = freq / (float)cylinders * 120.0;
    //RPM = 1000000.0 / (float)period / (float)cylinders * 120.0
    // =======================================================

    // Display important startup items
    myGLCD.InitLCD(LANDSCAPE);
    myGLCD.clrScr();
    myGLCD.setFont(font0);
    myGLCD.setColor(VGA_GRAY);
    myGLCD.setBackColor(VGA_BLACK);
    myGLCD.print(Version, CENTER, CENTER);
    delay(2000);

    // Clear the screen and display static items
    myGLCD.clrScr();
    myGLCD.setFont(font1);
    myGLCD.setColor(VGA_GRAY);
    myGLCD.setBackColor(VGA_BLACK);
    myGLCD.print("RPM", peakRPMx + 180, peakRPMy + 10);

    // =======================================================
    // Draw triangles at predetermined points
    // Bar goes from right to left, so these are in from the left
    int S1 = linearBarX + barLength - int((float)RPMyellowline / (float)maximumRPM * (float)barLength);    // Yellowline mark
    int S2 = linearBarX + barLength - int((float)RPM_redline / (float)maximumRPM * (float)barLength);      // Redline mark
    myGLCD.setColor(VGA_YELLOW);
    geo.fillTriangle(S1 - 4, linearBarY + barWidth + 10, S1 + 4, linearBarY + barWidth + 10, S1, linearBarY + barWidth + 5);
    myGLCD.setColor(VGA_RED);
    geo.fillTriangle(S2 - 4, linearBarY + barWidth + 10, S2 + 4, linearBarY + barWidth + 10, S2, linearBarY + barWidth + 5);
    // =======================================================

    // Draw the bar background
    myGLCD.setColor(block_fill_colour);
    myGLCD.fillRect(linearBarX, linearBarY, linearBarX + barLength, linearBarY + barWidth);

    // Display the redline value in red
    myGLCD.setFont(font7L);
    myGLCD.setColor(VGA_RED);
    myGLCD.setBackColor(VGA_BLACK);
    myGLCD.printNumI(RPM_redline, RPMx, RPMy, 4, '0');

    // Display the peak RPM value
    myGLCD.setFont(font7F);
    myGLCD.setColor(text_colour2);
    myGLCD.setBackColor(VGA_BLACK);
    myGLCD.printNumI(peakRPM, peakRPMx, peakRPMy, 4, ' ');

    delay(1000);


    }    // End void setup



// ##################################################################################################################################################
// ##################################################################################################################################################



void loop()
    {


    // =======================================================
    // Reset peak RPM by button press
    // =======================================================

    if (digitalRead(Button_Pin) == Digitial_Input_Active)
        {
        // Allow time for the button pin to settle
        // this assumes some electronic/external debounce
        delay(10);
        if (digitalRead(Button_Pin) == Digitial_Input_Active)
            peakRPM = 0;
        }


    // =======================================================
    // Dim display when headlights on
    // =======================================================

    if (millis() > startup_time)
        {
        // Dim mode when headlights are on
        if (digitalRead(Low_Beam_Pin) == Digitial_Input_Active && !dim_mode)
            {
            dim_mode          = true;
            text_colour1      = VGA_SILVER;
            text_colour2      = VGA_GRAY;
            block_fill_colour = VGA_BLACK;
            }

        // Normal colours when headlights are off
        if (digitalRead(Low_Beam_Pin) == !Digitial_Input_Active && dim_mode)
            {
            dim_mode          = false;
            text_colour1      = VGA_WHITE;
            text_colour2      = VGA_SILVER;
            block_fill_colour = VGA_GRAY;
            }
        }


    // =======================================================
    // Get the RPM and display RPM text and graph
    // =======================================================

    if (!Demo_Mode)
        {
        // Read the real RPM input
        hightime = pulseIn(RPM_Input_Pin, HIGH, pulsein_timeout);
        lowtime  = pulseIn(RPM_Input_Pin, LOW, pulsein_timeout);
        period   = hightime + lowtime;
        // for testing
        //period = random(period_min, pulsein_timeout);

        // prevent overflows or divide by zero
        if (period > period_min)
            {
            RPM = int(RPM_constant / (float)period);
            // Advance the spinner
            myGLCD.setFont(font1);
            myGLCD.setColor(text_colour2);
            myGLCD.setBackColor(VGA_BLACK);
            switch (spinnerState)
                {
                case 1:
                    // "|"
                    myGLCD.print("|", spinner_x, spinner_y);
                    spinnerState++;
                    break;
                case 2:
                    // "/"
                    myGLCD.print("/", spinner_x, spinner_y);
                    spinnerState++;
                    break;
                case 3:
                    // "_" 95
                    // print a space then an underscore shifted higher
                    // a minus sign is too narrow
                    myGLCD.print(" ", spinner_x, spinner_y);
                    myGLCD.print("_", spinner_x, spinner_y - 10);
                    spinnerState++;
                    break;
                case 4:
                    // "\"
                    // need to "eascape" this character for it to print
                    // and not interfere with the compiler
                    myGLCD.print("\\", spinner_x, spinner_y);
                    spinnerState++;
                    break;
                case 5:
                    // "|"
                    myGLCD.print("|", spinner_x, spinner_y);
                    spinnerState++;
                    break;
                case 6:
                    // "/"
                    myGLCD.print("/", spinner_x, spinner_y);
                    spinnerState++;
                    break;
                case 7:
                    // "_" 95
                    // print a space then an underscore shifted higher
                    // a minus sign is too narrow
                    myGLCD.print(" ", spinner_x, spinner_y);
                    myGLCD.print("_", spinner_x, spinner_y - 10);
                    spinnerState++;
                    break;
                case 8:
                    // "\"
                    // need to "eascape" this character for it to print
                    // and not interfere with the compiler
                    myGLCD.print("\\", spinner_x, spinner_y);
                    spinnerState = 1;
                    break;
                }
            }
        else
            {
            RPM = 0;
            }
        }
    else
        {
        // Demo mode, invent values
        demoRPM = demoRPM + random(-500, 1500);
        if (demoRPM >= maximumRPM)
            {
            demoRPM = 0;
            peakRPM = RPMyellowline / 2;
            }
        RPM = demoRPM;
        //RPM = 8000;
        }

    RPM = constrain(RPM, 0, maximumRPM);


    // =======================================================
    // Output the RPM as a PWM signal for another Arduino
    // =======================================================

    RPM_LED_Pos = map(RPM, RPMyellowline, RPM_redline, 0, LED_Count);
    RPM_LED_Pos = constrain(RPM_LED_Pos, 0, LED_Count);
    Serial2.write(RPM_LED_Pos);


    // =======================================================
    // Update the max RPM if it increased
    // =======================================================

    // Do this before we round the RPM to 10's or 100's
    if (RPM > peakRPM && RPM > RPMyellowline / 2)
        {
        peakRPM = RPM;
        myGLCD.setFont(font7F);
        myGLCD.setColor(text_colour2);
        myGLCD.setBackColor(VGA_BLACK);
        myGLCD.printNumI(peakRPM, peakRPMx, peakRPMy, 4, ' ');
        }


    // =======================================================
    // Display RPM value
    // =======================================================

    // Round RPM to nearest 100's or 10's
    RPM_f = (float)RPM;
    if (RPM >= lowRPM)
        {
        RPM = int((RPM_f + 50.0) * 0.01) * 100;
        }
    else
        {
        RPM = int((RPM_f + 5.0) * 0.1) * 10;
        }

    myGLCD.setFont(font7L);
    myGLCD.setBackColor(VGA_BLACK);
    if (RPM <= 999)
        {
        // Print a black digit 8 where a rogue digit would be
        // because this large font only has digits, no space char
        myGLCD.setColor(VGA_BLACK);
        myGLCD.printNumI(8, RPMx, RPMy, 1, '0');
        // Print the 3 digit RPM
        myGLCD.setColor(text_colour1);
        myGLCD.printNumI(RPM, RPMx + 96, RPMy, 3, '0');
        }
    else
        {
        // Print the 4 digit RPM
        myGLCD.setColor(text_colour1);
        myGLCD.printNumI(RPM, RPMx, RPMy, 4, '0');
        }


    // =======================================================
    // Draw the barmeter
    // =======================================================

    colouredBarValue = barLength - map(RPM, meterMin, meterMax, 0, barLength);

    if (!dim_mode)
        {
        // All colours available fir bright mode
        // Choose colour scheme using the Rainbow function
        // uncomment one line
        //block_colour = TFT_RED;    // Fixed colour
        //block_colour = TFT_GREEN;  // Fixed colour
        //block_colour = TFT_NAVY;   // Fixed colour
        //block_colour = rainbow(map(RPM, meterMin, meterMax, 0, 127));    // Blue to red
        block_colour = rainbow(map(RPM, meterMin, meterMax, 63, 127));    // Green to red
        //block_colour = rainbow(map(RPM, meterMin, meterMax, 127, 63));  // Red to green
        //block_colour = rainbow(map(RPM, meterMin, meterMax, 127, 0));   // Red to blue
        }
    else
        {
        // Dim mode, headlingts on
        block_colour = text_colour2;
        }

    // The bar goes from right to left

    // Fill in blank segments
    // This is the background colour of the bar graph
    if (RPM < lastRPM)
        {
        myGLCD.setColor(block_fill_colour);
        myGLCD.fillRect(linearBarX, linearBarY, linearBarX + colouredBarValue, linearBarY + barWidth);
        }

    // Fill in coloured blocks
    // This is the foreground colour of the bar graph
    // X, Y, Width(X axis), Height(Y axis), Colour
    myGLCD.setColor(block_colour);
    myGLCD.fillRect(linearBarX + colouredBarValue, linearBarY, linearBarX + barLength, linearBarY + barWidth);

    lastRPM = RPM;



    }    // End void loop



// ##################################################################################################################################################
// ##################################################################################################################################################



// =======================================================
// Reusable functions
// =======================================================


// =======================================================

// Function to return a 16 bit rainbow colour

unsigned int rainbow(byte value)
    {
    // Value is expected to be in range 0-127
    // The value is converted to a spectrum colour from 0 = blue through to 127 = red

    byte red      = 0;    // Red is the top 5 bits of a 16 bit colour value
    byte green    = 0;    // Green is the middle 6 bits
    byte blue     = 0;    // Blue is the bottom 5 bits

    byte quadrant = value / 32;

    if (quadrant == 0)
        {
        blue  = 31;
        green = 2 * (value % 32);
        red   = 0;
        }
    if (quadrant == 1)
        {
        blue  = 31 - (value % 32);
        green = 63;
        red   = 0;
        }
    if (quadrant == 2)
        {
        blue  = 0;
        green = 63;
        red   = value % 32;
        }
    if (quadrant == 3)
        {
        blue  = 0;
        green = 63 - 2 * (value % 32);
        red   = 31;
        }
    return (red << 11) + (green << 5) + blue;
    }

// =======================================================



// ##################################################################################################################################################
// ##################################################################################################################################################
