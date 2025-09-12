### What is this

Serial programmer (ISP client) for Nuvoton chips.

### Build

Run `make`.

### Use

```
Usage: nuvotool [OPTION]... [FILE]
Nuvoton ISP serial programmer.  Write HEX/BIN file to APROM.

-p, --port=PORT    Select serial device
-x, --erase        Erase APROM first
-c, --config=XX    Program CONFIG bytes
-h, --help         Show this message and exit
```
