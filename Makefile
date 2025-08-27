CFLAGS := -O -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

.PHONY : clean

nuvotool : ihex.o isp.o stdz.o ucomm.o
nuvotool.o : ihex.h isp.h stdz.h ucomm.h
ihex.o : ihex.h stdz.h
isp.o : isp.h stdz.h
stdz.o : stdz.h
ucomm.o : ucomm.h
clean : ;-rm -f nuvotool *.o
