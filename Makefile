TARGET = nuvotool
OBJECTS = ihx.o isp.o nuvotool.o stdz.o ucomm.o ucomm_ports.o

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Werror -Wpedantic
LDFLAGS += -s
MAKEFLAGS += -r

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
clean :
	-rm -f $(TARGET) $(OBJECTS)
.PHONY : clean

# !!gcc -MM *.c
ihx.o: ihx.c ihx.h stdz.h getopt.h
isp.o: isp.c stdz.h getopt.h isp.h bswap.h ucomm.h
nuvotool.o: nuvotool.c stdz.h getopt.h ihx.h isp.h ucomm.h
stdz.o: stdz.c stdz.h getopt.h getopt.c
ucomm.o: ucomm.c ucomm.h
ucomm_ports.o: ucomm_ports.c ucomm.h
