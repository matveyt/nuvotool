//
// nuvotool
//
// Nuvoton ISP programmer
// Write HEX/BIN file to APROM
//
// https://github.com/matveyt/nuvotool
//

#include "stdz.h"
#include <errno.h>
#include <getopt.h>
#include "ihex.h"
#include "isp.h"
#include "ucomm.h"

const char program_name[] = "nuvotool";

// user options
struct {
    bool erase, config;
    uint8_t config_bytes[5];
    char* port;
    char* input;
} opt = {0};

/*noreturn*/
void help(void)
{
    fprintf(stdout,
"Usage: %s [OPTION]... [FILE]\n"
"Nuvoton ISP programmer. Write HEX/BIN file to APROM.\n"
"\n"
"-x, --erase        Erase APROM first\n"
"-p, --port=PORT    Select serial device\n"
"-c, --config=XX    Program CONFIG bytes\n"
"-h, --help         Show this message and exit\n",
        program_name);
    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "erase", no_argument, NULL, 'x' },
        { "port", required_argument, NULL, 'p' },
        { "config", required_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "xp:c:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'x':
            opt.erase = true;
        break;
        case 'p':
            free(opt.port);
            opt.port = z_strdup(optarg);
        break;
        case 'c':
            if (ihex_blob(opt.config_bytes, sizeof(opt.config_bytes), optarg)
                == sizeof(opt.config_bytes)) {
                opt.config_bytes[3] = UINT8_MAX;
                opt.config = true;
            } else {
                errno = EILSEQ;
                z_die("CONFIG");
            }
        break;
        break;
        case 'h':
            help();
        break;
        case '?':
            fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
            exit(EXIT_FAILURE);
        break;
        }
    }

    if (optind == argc - 1)
        opt.input = z_strdup(argv[optind]);
}

// Device ID => Flash Size
unsigned did2size(uint32_t did)
{
    unsigned nib1 = (uint8_t)did >> 4;
    return (nib1 > 4) ? (18 * 1024) : (4096 << nib1);
}

// CONFIG1 => LDROM size
unsigned cfg2size(uint8_t cfg1)
{
    unsigned sz = (7 - (cfg1 & 7)) * 1024;
    return min(sz, 4096);
}

int main(int argc, char* argv[])
{
    parse_args(argc, argv);

    // ISP connection
    ISP_DATA data;
    ISP_CONNECTION conn = {
        .handle = ucomm_open(opt.port, 115200),
        .read = ucomm_read,
        .write = ucomm_write,
    };
    free(opt.port);
    if (conn.handle < 0)
        z_die("ucomm_open");

    // 300 ms read timeout
    ucomm_timeout(conn.handle, 300);
    // assert both DTR and RTS
    ucomm_ready(conn.handle, 3);
    ucomm_ready(conn.handle, 0);

    // Wait for CONNECT
    puts("Wait for connection...");
    while (!isp_command(ISP_CONNECT, &data, &conn))
        /*nothing*/;
    ucomm_purge(conn.handle);

    // Chip Info
    uint32_t did;
    unsigned flash_size, ldrom_size;
    bool cbs;
    uint8_t fw_version;

#define ISP(code)                               \
    if (!isp_command(ISP_##code, &data, &conn)) \
        z_die(#code)

    // some bootloaders expect this
    ISP(SYNC_PACKNO);

    ISP(GET_DEVICEID);
    did = data.l[0];
    flash_size = did2size(did);
    printf("Device ID: 0x%04x\n", did);
    printf("Flash Memory: %uKB\n", flash_size / 1024);

    ISP(READ_CONFIG);
    cbs = !!(data.b[0] & 0x80);
    ldrom_size = cfg2size(data.b[1]);
    printf("CONFIG: %02x%02x%02x%02x%02x\n", data.b[0], data.b[1], data.b[2],
        data.b[3], data.b[4]);
    printf("LDROM: %uKB, %sactive\n", ldrom_size / 1024, cbs ? "in" : "");

    ISP(GET_FWVER);
    fw_version = data.b[0];
    printf("FW Version: 0x%x\n", fw_version);

    // Erase
    if (opt.erase) {
        ucomm_timeout(conn.handle, 3000);
        puts("Erase APROM");
        ISP(ERASE_ALL);
        ucomm_timeout(conn.handle, 300);
    }

    // Write
    if (opt.input != NULL) {
        FILE* fin = z_fopen(opt.input, "rb");
        size_t sz, base, entry;
        uint8_t* image = ihex_load8(&sz, &base, &entry, fin);

        if (image == NULL)
            errno = ENOEXEC;
        else if (sz > flash_size - ldrom_size)
            errno = EFBIG;
        else if (base > 0 || entry > 0)
            errno = EFAULT;
        if (errno != 0)
            z_die("ihex");

        printf("Write APROM[%zu]\n", sz);
        if (!isp_write(0, image, sz, &conn))
            z_die("isp_write");

        free(image);
        fclose(fin);
    }

    // CONFIG
    if (opt.config) {
        memcpy(data.b, opt.config_bytes, sizeof(opt.config_bytes));
        puts("Update CONFIG");
        ISP(UPDATE_CONFIG);
    }

    puts("Exit LDROM");
    ISP(RUN_APROM);

    ucomm_close(conn.handle);
    exit(EXIT_SUCCESS);
}
