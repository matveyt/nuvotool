### What is this

A serial programmer (ISP client) for Nuvoton chips.

### Build

Run `make`.

### Use

```
Usage: nuvotool [OPTION]... [FILE]
Nuvoton ISP programmer.  Write HEX/BIN file to APROM.

-x, --erase        Erase APROM first
-p, --port=PORT    Select serial device
-c, --config=XX    Program CONFIG bytes
-r, --readonly     Do not write anything
-h, --help         Show this message and exit
```
