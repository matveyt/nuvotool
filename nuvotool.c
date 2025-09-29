//
// nuvotool
//
// Nuvoton ISP serial programmer
// Write HEX/BIN file to APROM
//
// https://github.com/matveyt/nuvotool
//

#include "stdz.h"
#include "ihx.h"
#include "isp.h"
#include "ucomm.h"
#include "ya_getopt.h"

const char* z_progname = "nuvotool";

// user options
static struct {
    char* file;
    char* port;
    bool erase, config;
    uint8_t config_bytes[5];
} opt = {0};

/*noreturn*/
static void usage(int status)
{
    if (status != EXIT_SUCCESS)
        fprintf(stderr, "Try '%s --help' for more information.\n", z_progname);
    else
        printf(
"Usage: %s [OPTION]... [FILE]\n"
"Nuvoton ISP serial programmer. Write HEX/BIN file to APROM.\n"
"\n"
"-p, --port=PORT    Select serial device\n"
"-x, --erase        Erase APROM first\n"
"-c, --config=XX    Program CONFIG bytes\n"
"-h, --help         Show this message and exit\n",
        z_progname);
    exit(status);
}

static void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "erase", no_argument, NULL, 'x' },
        { "config", required_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "p:xc:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'p':
            free(opt.port);
            opt.port = z_strdup(optarg);
        break;
        case 'x':
            opt.erase = true;
        break;
        case 'c':
            if (ihx_blob(opt.config_bytes, sizeof(opt.config_bytes), optarg)
                == sizeof(opt.config_bytes)) {
                opt.config_bytes[3] = 0xff;
                opt.config = true;
            } else
                z_error(EXIT_FAILURE, EILSEQ, "CONFIG %s", optarg);
        break;
        case 'h':
            usage(EXIT_SUCCESS);
        break;
        case '?':
            usage(EXIT_FAILURE);
        break;
        }
    }

    if (optind == argc - 1)
        opt.file = z_strdup(argv[optind]);
}

// Nuvoton ID => Flash Size
static size_t nuvoton_flashsize(uint32_t id)
{
    unsigned nib1 = (uint8_t)id >> 4;
    return (nib1 > 4) ? (18 * 1024) : (4096 << nib1);
}

// Nuvoton ID => Page Size
static size_t nuvoton_pagesize(uint32_t id)
{
    return (id == 0x2f50/*N76E616*/) ? 256 : 128;
}

// CONFIG => CBS bit
static bool nuvoton_cbs(const uint8_t config[5])
{
    return !!(config[0] & 0x80);
}

// CONFIG => LDROM size
static size_t nuvoton_ldsize(const uint8_t config[])
{
    unsigned ldsize = (7 - (config[1] & 7)) * 1024;
    return min(ldsize, 4096);
}

int main(int argc, char* argv[])
{
    parse_args(argc, argv);

    // ISP connection
    uint8_t data[ISP_DATA_SIZE];
    intptr_t isp = ucomm_open(opt.port, 115200, 0x801/*8-N-1*/);
    if (isp < 0) {
        if (opt.port != NULL)
            z_error(EXIT_FAILURE, errno, "ucomm_open(\"%s\")", opt.port);
        z_error(0, -1, "missing port name");
        usage(EXIT_FAILURE);
    }
    free(opt.port);

    // assert RTS then DTR (aka nodemcu reset)
    ucomm_rts(isp, 1);
    ucomm_dtr(isp, 1);
    ucomm_rts(isp, 0);
    ucomm_dtr(isp, 0);

    // wait for connect
    puts("Wait for connection...");
    do {
        (void)ucomm_getc(isp);  // delay
        // ISP_CONNECT
    } while (!isp_command(ISP_CONNECT, data, isp));
    ucomm_purge(isp);

    // Chip Info
    uint32_t did;
    size_t fsz, psz;
    uint8_t fw_version;
    uint8_t config[5];
    bool cbs;
    size_t ldsz;

#define ISP(code)                                       \
    if (!isp_command(ISP_##code, data, isp))            \
        z_error(EXIT_FAILURE, -1, "%s failed", #code)

    // some bootloaders expect this
    ISP(SYNC_PACKNO);

    ISP(GET_DEVICEID);
    did = (data[1] << 8) | (data[0]);
    fsz = nuvoton_flashsize(did);
    psz = nuvoton_pagesize(did);

    ISP(GET_FWVER);
    fw_version = data[0];

    ISP(READ_CONFIG);
    memcpy(config, data, sizeof(config));
    cbs = nuvoton_cbs(config);
    ldsz = nuvoton_ldsize(config);

    printf("Device ID: 0x%x\n", did);
    printf("Flash Memory: %zuKB,%zup,x%zu\n", fsz / 1024, fsz / psz, psz);
    printf("FW Version: 0x%x\n", fw_version);
    printf("CONFIG: %02x%02x%02x%02x%02x\n",
        config[0], config[1], config[2], config[3], config[4]);
    printf("LDROM: %zuKB, %sactive\n", ldsz / 1024, cbs ? "in" : "");

    // Erase
    if (opt.erase) {
        ucomm_timeout(isp, 3000);
        puts("Erase APROM");
        ISP(ERASE_ALL);
        ucomm_timeout(isp, UCOMM_DEFAULT_TIMEOUT);
    }

    // Write
    if (opt.file != NULL) {
        FILE* fin = z_fopen(opt.file, "rb");
        uint8_t* image;
        size_t sz, base, entry;
        if (ihx_load(&image, &sz, &base, &entry, fin) < 0)
            z_error(EXIT_FAILURE, errno, "ihx_load");
        if (base > 0 || entry > 0)
            z_error(EXIT_FAILURE, EFAULT, "ihx_load");
        if (sz > fsz - ldsz)
            z_error(EXIT_FAILURE, EFBIG, "ihx_load");

        printf("Write APROM[%zu]\n", sz);
        if (!isp_write(0, image, sz, isp))
            z_error(EXIT_FAILURE, -1, "isp_write(%zu)", sz);

        free(image);
        fclose(fin);
    }

    // CONFIG
    if (opt.config) {
        memcpy(data, opt.config_bytes, sizeof(opt.config_bytes));
        puts("Update CONFIG");
        ISP(UPDATE_CONFIG);
    }

    ISP(RUN_APROM);
    ucomm_close(isp);
    exit(EXIT_SUCCESS);
}
