CFLAGS := -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

nuvotool : nuvotool.o ihx.o isp.o stdz.o ucomm.o
nuvotool.o : nuvotool.c ihx.h isp.h stdz.h ucomm.h
ihx.o : ihx.c ihx.h stdz.h
isp.o : isp.c isp.h bswap.h stdz.h ucomm.h
stdz.o : stdz.c stdz.h
ucomm.o : ucomm.c ucomm.h
clean : ;-rm -f nuvotool *.o
.PHONY : clean
