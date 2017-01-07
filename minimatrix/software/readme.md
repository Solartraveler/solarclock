# About the software
The software is divided in two projects. One simple version and one advanced version.

You can edit the menu with my other open source project from

http://menudesigner.sourceforge.net/

## The simple version
Currently a simple version of the software is finished.

Its not containing all planned features of the clock, but is complete enough to run and allow using the clock.

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

- Many many settings:

  * Brightness manual/automated

  * Show seconds/hide seconds

  * Manual charger control

  * Set mAh of battery

  * Set calibrated value for charge measurement

  * Set level for accepted DCF77 error

  * Set refreshrate

  * Set volume of beep

  * Set frequency of beep

  * Set current consumption for used LEDs

  * Set time until alarm goes off anyway

  * Select which type debug messages should be printed on the RS232 interface

  * Set charger between on/off/automatic

  * Selects if clock should switch display completely off when dark

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

Unlike the simple clock version, the advanced version contains source code from third parties:

The RC-5 decoder software is from Peter Dannegger.

The two wire interface library is from Atmel Corporation.

### Completed features

- Event logger in external I2C EEPROM (reboots and cause of it, temperature, chargings, DCF77 sync times and time delta ...)

  * Supports eeproms with one I2C address, two bytes for addressing and at least 16 byte page size.
   This is usually the case for all 8KB - 64KB eeproms. Each log entry requires 23 bytes.
   Storing is done FIFO based without explicit index storing, so eeprom life is maximized.
   Size detection is done automatically on first startup.

- RC5 remote control receiver

- More alarms (repeating, non repeating, only some days of the week...)

- Power down on certain times of the day, saving power

### Planned features

- Wireless control and read of logger with RFM12

- Currently, communication with RFM12 works, however for some reason the crystal does not start, so no RX, TX.

### Known bugs:

  - Sometimes the display flickers. Seems I loose some interrupts...

  - Enabling the RC-5 receiver sometimes seem to lock up the device. The watchdog bites after 8 seconds...
