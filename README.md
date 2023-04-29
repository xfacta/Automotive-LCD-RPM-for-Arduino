# Automotive LCD RPM for Arduino
 Digital and bargraph display of RPM

Tachometer
Large text and
Bar style graphical meter
Reversed direction of bar
Dim on parker lights
Button to reset peak RPM
Outputs RPM as PWM for external shift light (on another arduino)
Uses pulseIn , no interupts
Offloaded sounds to external Leonardo Tiny

Uses 
UTFT Libraries
Copyright (C)2015 Rinky-Dink Electronics, Henning Karlsen. All right reserved
web: http://www.RinkyDinkElectronics.com/
and some associated font files

It is required to check and adjust

RPM_redline = 8000
RPM_yellowline = 6000
cylinders = 4
minimum_RPM = 500

You can also set
Demo_Mode = true or false for display of random values
Calibration_Mode = true or false for display of some raw data

Kludge_Factor is a way of adjusting timing since all Arduino crystals will be slightly different

The shift light function is offloaded to another Arduino via PWM so the other Arduino can also use the LED strip for other functions such as warning lights
