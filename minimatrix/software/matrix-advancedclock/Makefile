TARGET=main
MCU=atxmega64a1
OPT=-Os
OPTFAST=-Ofast
DEFINES=-DAVR_BUILD -DADVANCEDCLOCK

OBJ = $(TARGET).o touch.o charger.o gui.o config.o rs232.o displayRtc.o menu-interpreter.o menu-text.o sound.o dcf77.o clocks.o rfm12.o adc.o timeconvert.o dcf77statisticsdecode.o pintest.o twi_master_driver.o  rc5decoder.o logger.o i2ceeprom.o finecalib.o debug.o stack.o
INCLUDES = $(wildcard *.h)

all: $(OBJ)
	avr-gcc -mmcu=$(MCU) -I. -gstabs $(OPT) -std=gnu99 $(OBJ) -Wl,-Map,$(TARGET).map --output $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature  $(TARGET).elf $(TARGET).hex
	avr-objcopy -j .eeprom  --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0  --no-change-warnings -O ihex $(TARGET).elf $(TARGET).epp || exit 0
	avr-objdump -h -S $(TARGET).elf > $(TARGET).lss
	avr-size -C --mcu=$(MCU) $(TARGET).elf

#the dcf77 decoding needs to be fast so other optimization here
dcf77statisticsdecode.o: dcf77statisticsdecode.c $(INCLUDES)
	avr-gcc -c -mmcu=$(MCU) -I. -gstabs $(OPTFAST) $(DEFINES) -Wall -Wextra -std=gnu99 $<

# $< found filesnames
%.o: %.c $(INCLUDES)
	avr-gcc -c -mmcu=$(MCU) -I. -gstabs $(OPT) $(DEFINES) -Wall -Wextra -std=gnu99 $<

#simple assembly listing
dcf77statisticsdecode.s: dcf77statisticsdecode.c $(INCLUDES)
	avr-gcc -S -mmcu=$(MCU) -I. -gstabs $(OPTFAST) $(DEFINES) -Wall -Wextra -std=gnu99 $<

%.s: %.c $(INCLUDES)
	avr-gcc -S -mmcu=$(MCU) -I. -gstabs $(OPT) $(DEFINES) -Wall -Wextra -std=gnu99 $<

#preprocessor output
%.pre: %.c $(INCLUDES)
	avr-gcc -E -S -mmcu=$(MCU) -I. -gstabs $(OPT) $(DEFINES) -Wall -Wextra -std=gnu99 $< > $@

#assembly listing with C source as references
dcf77statisticsdecode.sx: dcf77statisticsdecode.c $(INCLUDES)
	avr-gcc -c -Wa,-adhln -g -mmcu=$(MCU) -I. $(OPTFAST) $(DEFINES) -Wall -Wextra -std=gnu99 $< > $@

%.sx: %.c $(INCLUDES)
	avr-gcc -c -Wa,-adhln -g -mmcu=$(MCU) -I. $(OPT) $(DEFINES) -Wall -Wextra -std=gnu99 $< > $@

upload:
	avrdude -P usb -c avrispmkII -p x64a1 -U $(TARGET).hex

reset:
	avrdude -P usb -c avrispmkII -p x64a1

clean:
	rm -rf *.o
	rm -f $(TARGET).hex
	rm -rf *.sx
	rm -rf *.lss
	rm -rf *.s
	rm -rf *.pre
	rm -f $(TARGET).elf
	rm -f $(TARGET).epp
	rm -f *.map

removebackups:
	rm -rf *~
