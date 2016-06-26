
# local build configurations
-include local.mk

# unset VERBOSE to turn on verbosity
VERBOSE = @

ECHO = echo
CC = xtensa-lx106-elf-gcc
CFLAGS = -I. -mlongcalls
LDLIBS = -nostdlib -Wl,--start-group -lmain -lnet80211 -lwpa -llwip -lpp -lphy -Wl,--end-group -lgcc
LDFLAGS = -Teagle.app.v6.ld

Q = $(VERBOSE)

ifneq ($(PORT),)
SERIAL_PORT=--port $(PORT)
endif

ELF = sensorhub
src = sensorhub.c

OBJ = $(src:.c=.o)

$(ELF): $(OBJ)
	$(Q) $(ECHO) " [ LD ] $@"
	$(Q) $(CC) $(LDFLAGS) $(LDLIBS) $^ -o $@

$(ELF)-0x00000.bin: $(ELF)
	$(Q)python2 `which esptool.py` elf2image $^

%.o: %.c Makefile
	$(Q) $(ECHO) " [ CC ] $<"
	$(Q) $(CC) -c -o $@ $< $(CFLAGS)

flash: $(ELF)-0x00000.bin
	$(Q)python2 `which esptool.py` $(SERIAL_PORT) write_flash 0 $(ELF)-0x00000.bin 0x40000 $(ELF)-0x40000.bin

clean:
	$(Q) $(ECHO) [ RM ] $(ELF)
	$(Q)rm -f $(ELF)
	$(Q) $(ECHO) [ RM ] $(src:.c=.o)
	$(Q)rm -f $(src:.c=.o)
	$(Q) $(ECHO) [ RM ] $(src:.c=.d)
	$(Q)rm -f $(src:.c=.d)
	$(Q) $(ECHO) [ RM ] $(ELF)-0x00000.bin
	$(Q)rm -f $(ELF)-0x00000.bin
	$(Q) $(ECHO) [ RM ] $(ELF)-0x40000.bin
	$(Q)rm -f $(ELF)-0x40000.bin

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,.*:,$(@:.d=.o):,' < $@.$$$$  > $@; \
	rm -f $@.$$$$

-include $(src:.c=.d)
