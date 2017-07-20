# About the software
The software is divided in two projects. One simple version and one advanced version.

You can edit the menu with my other open source project from

http://menudesigner.sourceforge.net/

## The simple version

Its not containing all planned features of the clock, but is complete enough to run and allow using the clock.
This might be a starting point to get an overview how the code is managed or simply use it if you just dont want a heavily featured clock.
About 30KiB of the flash is used.

### Features:

- DCF77 synchronization

- Showing Date and Time

- Debug print and debug control over RS232

- Adjust brightness according to surrounding brightness

- Charge the batteries

- Set an alarm to a given time -> beep - beep - beep

- Set a counter to give alarm in a given number of minutes.

- Lets the user control with the four keys

- Shows the temperature

- Warns about low battery

- Keylock preventing accidental key presses

- Set RFM12 module to power down mode

- Suppors the infrared (reflex light barrier) keys.

- Many many settings:

  * Brightness manual/automated

  * Show seconds/hide seconds

  * Manual charger control (on/off/automatic)

  * Set mAh of battery

  * Set calibrated value for charge measurement

  * Set level for accepted DCF77 error

  * Set refreshrate

  * Set volume of beep

  * Set frequency of beep

  * Set current consumption for used LEDs

  * Set time until alarm a beeping alarm goes silent without user interaction

  * Select which type debug messages should be printed on the RS232 interface

  * Selects if clock should switch display completely off when dark or stay always at at least minimal brightness

- Showing many informations:

  * Current voltage

  * Current charging current

  * Current charged state of battery

  * Consumend mAh since power on

  * Quality of DCF77 signal

  * Raw value of one key

  * Raw value of lightning sensor

  * Show RC oscillator and CPU usage (percentage of time they are not stopped)

  * And shows even more with RS232 connected

### Known bugs:

  - Sometimes the display flickers. Seems I loose some interrupts...

## The advanced version

The advanced version contains all features of the simple version and uses about 51KiB flash.

Unlike the simple clock version, the advanced version contains source code from third parties:

The RC-5 decoder software is from Peter Dannegger.

The two wire interface library is from Atmel Corporation.

### Completed features

- Event logger in external I2C EEPROM (reboots and cause of it, temperature, chargings, DCF77 sync times and time delta ...)

  * Supports eeproms with one I2C address, two bytes for addressing and at least 16 byte page size.
   This is usually the case for all 8KiB - 64KiB eeproms. Each log entry requires 23 bytes.
   Storing is done FIFO based without explicit index storing, so eeprom life is maximized.
   Size detection is done automatically on first startup.

- RC5 remote control receiver

- Second alarm with repeating, non repeating, only some days of the week, settings

- Power down on certain times of the day, saving power

- Wireless control and read of logger with RFM12

- Manually setting time and date in the case there is no DCF sync

- Test pattern for the LED matrix

- Pin test for checking proper soldering

- Supports both infrared or capacitive keys - does autodetect on startup.

- Calibration of the internal crystal in the range of +- 0.036%

- Heap to stack closest gap (=free RAM) reporting. (95% of the 4KiB are used)

### Planned features

- Its finished :)

### Known bugs:

  - Sometimes the display flickers. Seems I loose some interrupts...
