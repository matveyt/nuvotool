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

-p, --port=PORT        Select serial device
-x, --erase            Erase APROM first
-c, --config=X[,X...]  Setup CONFIG
-l, --list-ports       List available ports only
-h, --help             Show this message and exit

Valid CONFIG fields: lock, rpd, ocden, ocdpwm, cbs, ldsize=0,1024,2048,3072,4096,
        cborst, boiap, cboden, cbov=2.2,2.7,3.7,4.4, wdten=disable,enable,always
Note that '--config rpd' or '--config rpd=yes' stands for '--config rpd=0',
        while '--config cborst' for '--config cborst=1', etc.
```
