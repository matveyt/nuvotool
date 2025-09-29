### What is this

Serial programmer (ISP client) for Nuvoton 8051 chips.

Note that most of chips go without bootloader (LDROM) installed. Please, refer to
[NuvoROM](https://github.com/matveyt/nuvorom) project on how to upload it.

### Build

If using GCC then simply run `make`. Otherwise, you may need to setup different compile
flags. The source code itself is thought to be C99 portable.

### Use

```
Usage: nuvotool [OPTION]... [FILE]
Nuvoton ISP serial programmer. Write HEX/BIN file to APROM.

-p, --port=PORT    Select serial device
-x, --erase        Erase APROM first
-c, --config=XX    Program CONFIG bytes
-h, --help         Show this message and exit
```
