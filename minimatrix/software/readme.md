Currently a simple version of the software is finished.
Its not containing all planned features of the clock, but is complete enough to run and allow using the clock.
Features:
-DCF77 synchronization
-Showing Date and Time
-Debug print and debug control over RS232
-Adjust brightnes according to surrounding brighness
-Charge the batteries
-Set an alarm to a given time -> beep - beep - beep
-Set a counter to give alarm in a given number of minutes.
-Lets the user control with the four keys
-Shows the temperature
-Warns about low battery
-Keylock preventing accidential key presses
-Set RFM12 module to power down mode
-Many many settings:
  -Brighnes manual/automated
  -Show seconds/hide seconds
  -Manual charger control
  -Set mAh of battery
  -Set calibrated value for charge measurement
  -Set level for accepted DCF77 error
  -Set refreshrate
  -Set volume of beep
  -Set frequency of beep
  -Set current consumption for used LEDs
  -Set time until alarm goes off anyway
  -Select which type debug messages should be printed on the RS232 interface
  -Set charger between on/off/automatic
  -Selects if clock should switch display completely off when dark
-Showing many informations:
  -Current voltage
  -Current charging current
  -Current charged state of battery
  -Consument mAh since power on
  -Quality of DCF77 signal
  -Raw value of one key
  -Raw value of lightning sensor
  -Show RC oscillator and CPU usage (percentage of time they are not stopped)
  -And shows even more with RS232 connected
Known bugs:
  -Sometimes the display flickers. Seems I loose some interrupts...

The following features are planned for an advanced-clock version:
-Event logger in external I2C EEPROM (temperature, chargings...)
-RC5 remote control receiver
-Wireless control and read of logger with RFM12
-More alarms (repeating, non repeating, only some days of the week...)
-Power down on certain times of the day, saving power
