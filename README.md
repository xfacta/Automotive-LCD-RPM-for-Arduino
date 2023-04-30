# Automotive LCD Tachometer for Arduino
## Digital and bargraph display of RPM

- AT Mega2560 and 3.5" LCD display
- Tachometer
- Peak RPM
- Uses pulseIn , no interupts
- Large text and
- Bar style graphical meter
- Reversed direction of bar
- Dim on parker lights
- Button to reset peak RPM
- Outputs RPM as PWM for external shift light (on another arduino)
- Offloaded sounds to external Leonardo Tiny


![Tacho](https://user-images.githubusercontent.com/41600026/235329704-6a54a9bf-f901-4835-ae28-00de2161cee2.PNG)


### Uses 
UTFT Libraries and some associated font files

Copyright (C)2015 Rinky-Dink Electronics, Henning Karlsen. All right reserved

web: http://www.RinkyDinkElectronics.com/


### It is required to check and adjust

```
RPM_redline = 8000
RPM_yellowline = 6000
cylinders = 4
minimum_RPM = 500
```
The assumption is 4 stroke operation, for 2 stroke you could just double the number of cylinders.

### You can also set
- `Demo_Mode` = true or false for display of random values
- `Calibration_Mode` = true or false for display of some raw data like input frequency in Hz
- `Kludge_Factor` is a way of adjusting timing since all Arduino crystals will be slightly different

Pressing the button at startup also enters calibration mode.

Pressing the button during normal operation resets the Peak RPM value.
Currently there is only a crude long-press detection using delay, and some debouncing in hardware is assumed.

The shift light function is offloaded to another Arduino via PWM so the other Arduino can also use the LED strip for other functions such as warning lights.
Only the `RPM_yellowline` to `RPM_redline range` is output, lower RPM values result in PWM = 0

The sounds are offloaded to a Leonardo Tiny to avoid delays and allow one Tiny and speaker to srvice multiple other functions such as Speedo and Fuel/Temperature/OilPressure gauge warning sounds.
